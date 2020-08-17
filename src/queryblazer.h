/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_QUERYBLAZER_H
#define QUERYBLAZER_QUERYBLAZER_H

#include "ThreadPool.h"
#include "boost/archive/binary_iarchive.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/serialization/utility.hpp"
#include "boost/serialization/vector.hpp"
#include "common.h"
#include "encoder.h"
#include "fst/fstlib.h"
#include "prefix_tree.h"
#include "transition.h"
#include <fstream>
#include <memory>
#include <string>

namespace qbz {

struct Config {
    explicit Config(size_t branch_factor = 30, size_t beam_size = 30,
                    size_t topk = 10, size_t length_limit = 100,
                    bool precompute = false, bool verbose = false)
        : branch_factor{branch_factor},
          beam_size{beam_size},
          topk{topk},
          length_limit{length_limit},
          precompute{precompute},
          verbose{verbose} {
        QBZ_ASSERT(branch_factor >= 1,
                   "Branch factor must be greater positive");
        QBZ_ASSERT(beam_size >= 1, "Beam size must be positive");
        QBZ_ASSERT(beam_size >= topk, "Beam size must be geq to topk");
    }

    const size_t branch_factor;
    const size_t beam_size;
    const size_t topk;
    const size_t length_limit;
    const bool precompute;
    const bool verbose;
};

class QueryBlazer {
  private:
    // (vector(olabel_sequences, cost), decoding length)
    using BeamSearchResult =
        std::pair<std::vector<std::pair<std::vector<int>, float>>, size_t>;

    class Arc {
      private:
        // required for serialization but want to hide it otherwise
        friend class boost::serialization::access;
        Arc() {}

        template <class Archive>
        void serialize(Archive &ar, const unsigned int version) {
            ar &olabel;
            ar &ilabel;
            ar &nextstate;
            ar &weight;
        }

      public:
        int olabel, ilabel, nextstate;
        float weight;

        explicit Arc(const fst::StdArc &arc)
            : olabel{arc.olabel},
              ilabel{arc.ilabel},
              nextstate{arc.nextstate},
              weight{arc.weight.Value()} {}
    };

    const unsigned num_proc;
    const std::unique_ptr<fst::StdExpandedFst> encoder, model;
    const Config config;
    std::vector<std::vector<Arc>> topArcs;
    std::vector<BeamSearchResult> topResults;
    std::vector<std::vector<std::vector<int>>> encoderTransitions;

    using PM = fst::PhiMatcher<fst::SortedMatcher<fst::StdExpandedFst>>;
    fst::SortedMatcher<fst::StdExpandedFst> encoderMatcher;
    PM phiMatcher;
    int encoder_begin_state;

  public:
    explicit QueryBlazer(const std::string &encoder, const std::string &model,
                         const Config &config = Config{})
        : num_proc{std::thread::hardware_concurrency()},
          encoder{fst::StdExpandedFst::Read(encoder)},
          model{fst::StdExpandedFst::Read(model)},
          config{config},
          encoderMatcher{this->encoder.get(), fst::MatchType::MATCH_INPUT},
          phiMatcher{this->model.get(),
                     fst::MatchType::MATCH_INPUT,
                     IDX_PHI,
                     true,
                     fst::MATCHER_REWRITE_AUTO,
                     new fst::SortedMatcher<fst::StdExpandedFst>{
                         this->model.get(), fst::MatchType::MATCH_INPUT,
                         IDX_UNK + 1}} {
        QBZ_ASSERT(this->encoder, "Invalid encoder: " + encoder);
        QBZ_ASSERT(this->model, "Invalid model: " + model);
        QBZ_ASSERT(this->encoder->OutputSymbols()->LabeledCheckSum() ==
                       this->model->InputSymbols()->LabeledCheckSum(),
                   "Encoder's symbols does not match with that of model's");
        encoderMatcher.SetState(this->encoder->Start());
        QBZ_ASSERT(encoderMatcher.Find(
                       this->encoder->InputSymbols()->Find(ToString({SPACE}))),
                   "Encoder begin state not found");
        encoder_begin_state = encoderMatcher.Value().nextstate;
        ComputeEncoderTransitions();
        PrecomputeTopResults(config.precompute);
    }

    /**
     * Load beam search results from a serialized file
     */
    bool LoadPrecomputed(const std::string &input_file) {
        if (config.precompute) return false;

        std::ifstream ifs{input_file};
        QBZ_ASSERT(ifs, "Error opening " + input_file);
        boost::archive::binary_iarchive iarchive{ifs};
        size_t size, topk;
        iarchive >> size;
        iarchive >> topk;
        if (size != topResults.size() || topk != config.topk) return false;
        topResults.clear();
        iarchive >> topResults;
        QBZ_ASSERT(topResults.size() == model->NumStates(),
                   "NumStates mismatch");
        topArcs.clear();
        return true;
    }

    /**
     * Save beam search results into a serialized file
     */
    bool SavePrecomputed(const std::string &output_file) {
        if (!config.precompute) return false;

        std::ofstream ofs{output_file};
        QBZ_ASSERT(ofs, "Error opening " + output_file);
        boost::archive::binary_oarchive oarchive{ofs};
        oarchive << topResults.size();
        oarchive << config.topk;
        oarchive << topResults;
        return true;
    }

    std::pair<std::vector<std::pair<std::string, float>>, size_t>
    Complete(const std::string &query) {
        std::vector<std::pair<std::string, float>> suggestions;
        suggestions.reserve(config.topk);

        auto prefix = ToUtf8(query);
        std::replace(prefix.begin(), prefix.end(), static_cast<char32_t>(' '),
                     SPACE);
        Utf8 oovs;
        std::vector<int> ilabels;
        ilabels.reserve(prefix.size());
        for (auto c : prefix) {
            auto ilabel = encoder->InputSymbols()->Find(ToString({c}));
            if (ilabel == fst::kNoSymbol) {
                oovs.push_back(c);
                ilabel = IDX_UNK;
            }
            ilabels.push_back(ilabel);
        }

        int encoder_state;
        auto stable_output_seq =
            Encode(*encoder, encoderMatcher, encoder_begin_state, ilabels,
                   false, &encoder_state);
        std::string stable_prefix;
        auto oov_idx = 0;
        for (auto id : stable_output_seq) {
            if (id == IDX_UNK) {
                stable_prefix += ToString({oovs.at(oov_idx++)});
            } else {
                stable_prefix += encoder->OutputSymbols()->Find(id);
            }
        }
        QBZ_ASSERT(oov_idx == oovs.size(), "OOV size mismatch");

        auto model_state = model->Start();
        float init_cost = 0.0f;
        for (auto id : stable_output_seq) {
            phiMatcher.SetState(model_state);
            if (!phiMatcher.Find(id)) {
                QBZ_ASSERT(phiMatcher.Find(IDX_UNK),
                           "UNK token not found in the model");
            }
            init_cost += phiMatcher.Value().weight.Value();
            model_state = phiMatcher.Value().nextstate;
        }

        auto beams = InitBeams(encoder_state, model_state);
        std::pair<std::vector<std::pair<std::vector<int>, float>>, size_t>
            autocomplete;

        TopK<float> topK{config.topk};
        for (const auto &beam : beams) {
            // beam stores score before beam search (i.e., minimal olabel
            // sequences) will not insert cost but make sure it is within range
            if (!topK.WillInsert(beam.second.cost)) break;
            const auto &pair = GetTopResult(beam.second.state);
            for (const auto &precomputed : pair.first) {
                const auto cost = beam.second.cost + precomputed.second;
                if (!topK.Insert(cost)) break;
                auto olabels = beam.first;
                olabels.insert(olabels.end(), precomputed.first.begin(),
                               precomputed.first.end());
                autocomplete.first.emplace_back(std::move(olabels), cost);
            }
            autocomplete.second = std::max(autocomplete.second, pair.second);
        }
        QBZ_ASSERT(config.topk <= autocomplete.first.size(),
                   "not enough completions for topK");
        std::partial_sort(autocomplete.first.begin(),
                          autocomplete.first.begin() + config.topk,
                          autocomplete.first.end(),
                          [](const std::pair<std::vector<int>, float> &a,
                             const std::pair<std::vector<int>, float> &b) {
                              return a.second < b.second;
                          });
        autocomplete.first.erase(autocomplete.first.begin() + config.topk,
                                 autocomplete.first.end());

        for (const auto &candidate : autocomplete.first) {
            std::string output = stable_prefix;
            for (auto id : candidate.first) {
                if (id == IDX_UNK) continue;
                output += model->OutputSymbols()->Find(id);
            }

            auto utf_output = ToUtf8(output);
            std::replace(utf_output.begin(), utf_output.end(), SPACE,
                         static_cast<char32_t>(' '));
            // merge consecutive white spaces
            output = Join(Split(ToString(utf_output)));

            suggestions.emplace_back(output, init_cost + candidate.second);
        }

        return {suggestions, autocomplete.second};
    }

    const Config& GetConfig() const { return config; }

  private:
    struct Beam {
        explicit Beam(int state, float cost) : state{state}, cost{cost} {}

        bool operator<(const Beam &that) const { return cost < that.cost; }

        int state;
        float cost;
    };

    /**
     * Return top emitting transitions equal to branch_factor
     */
    const std::vector<Arc> &GetTopArcs(int state) {
        if (!topArcs.at(state).empty()) return topArcs.at(state);

        std::vector<Arc> arcs;
        std::vector<bool> ilabels(model->InputSymbols()->AvailableKey(), false);

        using Backoff = std::pair<int, float>;
        std::queue<Backoff> queue;
        queue.emplace(state, 0);
        while (!queue.empty()) {
            auto phi_state = queue.front().first;
            auto cost = queue.front().second;
            queue.pop();

            fst::ArcIterator<fst::StdFst> aiter{*model, phi_state};
            for (; !aiter.Done(); aiter.Next()) {
                Arc arc{aiter.Value()};
                if (arc.ilabel == IDX_PHI) {
                    queue.emplace(arc.nextstate, cost + arc.weight);
                } else if (!ilabels.at(arc.ilabel)) {
                    ilabels.at(arc.ilabel) = true;
                } else
                    continue; // fewer-phi-transition already exists for the
                              // ilabel

                arc.weight = arc.weight + cost;
                arcs.push_back(arc);
            }

            if (arcs.size() > config.branch_factor)
                std::partial_sort(arcs.begin(),
                                  arcs.begin() + config.branch_factor,
                                  arcs.end(), [](const Arc &a, const Arc &b) {
                                      return a.weight < b.weight;
                                  });

            // if phi transition is not within the top arcs, we are done
            auto it = std::find_if(arcs.begin(), arcs.end(), [](const Arc &a) {
                return a.ilabel == IDX_PHI;
            });

            if (arcs.size() > config.branch_factor &&
                std::distance(arcs.begin(), it) >= config.branch_factor) {
                arcs.erase(arcs.begin() + config.branch_factor, arcs.end());
                arcs.shrink_to_fit();
                QBZ_ASSERT(std::find_if(arcs.begin(), arcs.end(),
                                        [](const Arc &a) {
                                            return a.olabel == IDX_EPSILON;
                                        }) == arcs.end(),
                           "Non-emitting transition within top arcs");
                topArcs.at(state) = std::move(arcs);
                return topArcs.at(state);
            }

            // swap with the last element and remove so that it is O(1)
            QBZ_ASSERT(it != arcs.end(), "phi transition not found");
            std::swap(arcs.back(), *it);
            arcs.erase(arcs.end() - 1);
        }

        QBZ_ASSERT(false, "This must not be reached");
    }

    /**
     * Returns beam search result for the given model state
     */
    const BeamSearchResult &GetTopResult(int state) {
        if (topResults.at(state).first.empty()) {
            PrefixTree<int, Beam> prefixTree;
            prefixTree.Insert({}, Beam{state, 0.0f});
            size_t decode_length;
            auto autocomplete = BeamSearch(prefixTree, &decode_length);
            topResults.at(state) = {std::move(autocomplete), decode_length};
        }
        return topResults.at(state);
    }

    /**
     * Pre-compute beam search results on all states
     */
    void PrecomputeTopResults(bool precompute) {
        topArcs.resize(model->NumStates());
        topResults.resize(model->NumStates());
        if (!precompute) return;

        ThreadPool pool{num_proc};
        std::cerr << "Precomputing top arcs for " << model->NumStates()
                  << " states" << std::endl;
        std::vector<std::future<void>> results(model->NumStates());
        const auto fn_top_arcs = [this](int state) { this->GetTopArcs(state); };
        for (auto state = 0; state < model->NumStates(); ++state) {
            results.at(state) = pool.enqueue(fn_top_arcs, state);
        }
        for (auto state = 0; state < results.size(); ++state) {
            if (state % 1000000 == 0)
                std::cerr << "state " << state << "..." << std::endl;
            results.at(state).get();
        }

        std::cerr << "Precomputing top results for " << model->NumStates()
                  << " states" << std::endl;
        const auto fn_top_results = [this](int state) {
            this->GetTopResult(state);
        };
        for (auto state = 0; state < model->NumStates(); ++state) {
            results.at(state) = pool.enqueue(fn_top_results, state);
        }
        for (auto state = 0; state < results.size(); ++state) {
            if (state % 1000000 == 0)
                std::cerr << "state " << state << "..." << std::endl;
            results.at(state).get();
        }
        // remove topArcs results, since they are no long needed
        topArcs.clear();

        std::cerr << "Precomputing top results complete" << std::endl;
    }

    void ComputeEncoderTransitions() {
        encoderTransitions.reserve(encoder->NumStates());
        std::cerr << "Computing encoder transitions for "
                  << encoder->NumStates() << " states..." << std::endl;
        for (auto state = 0; state < encoder->NumStates(); ++state) {
            auto sequences = CandidateOlabels(*encoder, state);
            if (sequences.empty()) {
                std::vector<int> olabels;
                int out_state;
                MakeExitTransitions(*encoder, encoderMatcher, state, &olabels,
                                    &out_state);
                QBZ_ASSERT(
                    out_state == encoder->Start() && olabels.empty(),
                    "Getting empty seq from an unexpected encoder state " +
                        std::to_string(state));
                sequences.emplace_back();
            }
            encoderTransitions.push_back(std::move(sequences));
        }
    }

    /**
     * Returns best beam_size beams that give the best transitions to encoder's
     * start state
     */
    std::vector<std::pair<std::vector<int>, Beam>> InitBeams(int encoder_state,
                                                             int model_state) {
        const auto &sequences = encoderTransitions.at(encoder_state);
        TopK<float> topK{config.beam_size};
        std::vector<std::pair<std::vector<int>, Beam>> beams;

        for (const auto &sequence : sequences) {
            auto score = 0.0f;
            auto state = model_state;
            std::vector<int> olabels;
            auto skip_flag = false;
            for (auto ilabel : sequence) {
                phiMatcher.SetState(state);
                if (!phiMatcher.Find(ilabel)) {
                    // ilabel unigram does not exist (may have been pruned away
                    // during LM construction)
                    QBZ_ASSERT(phiMatcher.Find(IDX_UNK),
                               "UNK token not found in model");
                }
                score += phiMatcher.Value().weight.Value();
                if (!topK.WillInsert(score)) {
                    skip_flag = true;
                    break;
                }
                state = phiMatcher.Value().nextstate;
                olabels.push_back(ilabel);
            }
            if (skip_flag) continue;

            beams.emplace_back(std::move(olabels), Beam{state, score});
            topK.Insert(score);
        }

        const auto beam_size = std::min(beams.size(), config.beam_size);
        std::partial_sort(beams.begin(), beams.begin() + beam_size, beams.end(),
                          [](const std::pair<std::vector<int>, Beam> &a,
                             const std::pair<std::vector<int>, Beam> &b) {
                              return a.second < b.second;
                          });
        beams.erase(beams.begin() + beam_size, beams.end());

        return beams;
    }

    /**
     *
     * @param prefixTree
     * @param model
     * @return
     */
    std::vector<std::pair<std::vector<int>, float>>
    BeamSearch(PrefixTree<int, Beam> &prefixTree,
               size_t *decode_length = nullptr) {
        std::vector<std::pair<std::vector<int>, float>> result;
        TopK<float> topK{config.topk};
        size_t max_dl = 0;
        fst::SortedMatcher<fst::StdExpandedFst> matcher{
            model.get(), fst::MatchType::MATCH_INPUT, IDX_UNK + 1};

        while (!prefixTree.Empty()) {
            auto prefixes = prefixTree.FindAll();
            const auto beam_size = std::min(prefixes.size(), config.beam_size);
            std::partial_sort(prefixes.begin(), prefixes.begin() + beam_size,
                              prefixes.end(),
                              [](const PrefixLeaf<int, Beam> &a,
                                 const PrefixLeaf<int, Beam> &b) {
                                  return a.Data() < b.Data();
                              });

            for (auto it = prefixes.begin(); it != prefixes.begin() + beam_size;
                 ++it) {
                // process valid beams
                auto &prefix = *it;
                auto state = prefix.Data().state;
                if (!topK.WillInsert(prefix.Data().cost)) {
                    // no need to process the rest of the beams
                    break;
                }

                max_dl = std::max(prefix.Depth(), max_dl);
                if (prefix.Depth() >= config.length_limit) {
                    if (config.verbose)
                        std::cerr << "non-epsilon transition length limit "
                                     "exceeded; skipping"
                                  << std::endl;
                    continue;
                }

                auto final_cost = MakeExitTransitions(*model, matcher, state);
                final_cost += prefix.Data().cost;

                if (topK.Insert(final_cost)) {
                    result.emplace_back(prefix.Prefix(), final_cost);
                }

                const auto &arcs = GetTopArcs(state);
                for (const auto &arc : arcs) {
                    auto weight = prefix.Data().cost + arc.weight;
                    if (!topK.WillInsert(weight)) continue;
                    prefixTree.Insert(prefix, {arc.olabel},
                                      Beam{arc.nextstate, weight});
                }
            }

            // remove all processed beams
            for (auto & prefix : prefixes)
                prefixTree.Erase(prefix);
        }

        if (result.size() > config.topk) {
            std::partial_sort(result.begin(), result.begin() + config.topk,
                              result.end(),
                              [](const std::pair<std::vector<int>, float> &a,
                                 const std::pair<std::vector<int>, float> &b) {
                                  return a.second < b.second;
                              });
            result.erase(result.begin() + config.topk, result.end());
            result.shrink_to_fit();
        }

        if (decode_length) *decode_length = max_dl;

        return result;
    }
};

} // namespace qbz

#endif // QUERYBLAZER_QUERYBLAZER_H
