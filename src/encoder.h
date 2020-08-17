/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_ENCODER_H
#define QUERYBLAZER_ENCODER_H

#include "common.h"
#include "fst/fstlib.h"
#include "transition.h"

namespace qbz {

/**
 * Transduce ilabels into olabels using encoder FST
 *
 * @param encoder: LPM encoder in FST
 * @param matcher: matcher to use for encoding
 * @param in_state: initial state to begin encoding
 * @param ilabels: sequence of ilabels to encode to
 * @param complete: whether ilabel sequence is complete (will transition to exit
 * state)
 * @param out_state: optional pointer to which output state will be written
 *
 * @return: sequence of olabels emitted during transitions
 *          any UNK ilabel will be mapped to UNK olabel
 */
template <typename FST, typename Matcher>
std::vector<int> Encode(const FST &encoder, Matcher &matcher, int in_state,
                        const std::vector<int> &ilabels, bool complete = false,
                        int *out_state = nullptr) {

    std::vector<int> olabels;
    for (auto ilabel : ilabels) {
        QBZ_ASSERT(ilabel >= IDX_UNK,
                   "Unexpected ilabel: " + std::to_string(ilabel));
        if (ilabel == IDX_UNK) {
            MakeExitTransitions(encoder, matcher, in_state, &olabels,
                                &in_state);
            olabels.push_back(IDX_UNK);
        } else
            MakeTransitions(encoder, matcher, in_state, ilabel, &olabels,
                            &in_state);
    }

    if (complete)
        MakeExitTransitions(encoder, matcher, in_state, &olabels, &in_state);
    if (out_state) *out_state = in_state;

    return olabels;
}

/**
 * Return every possible candidate olabel sequences from the given state
 *
 * @param encoder: ilabel-sorted LPM encoder in FST
 * @param state: initial state
 *
 * @return: all possible sequences of output labels that leads to start state
 */
template <typename FST>
std::vector<std::vector<int>> CandidateOlabels(const FST &encoder, int state) {
    struct VisitState {
        explicit VisitState(std::vector<int> olabels, int state)
            : olabels{std::move(olabels)},
              state{state} {}

        std::vector<int> olabels;
        int state;
    };
    std::set<std::vector<int>> sequences;

    std::queue<VisitState> queue;
    queue.emplace(std::vector<int>{}, state);
    while (!queue.empty()) {
        auto &visitState = queue.front();
        if (visitState.state == encoder.Start()) {
            if (!visitState.olabels.empty())
                sequences.insert(std::move(visitState.olabels));
        } else {
            if (sequences.find(visitState.olabels) == sequences.end()) {
                fst::ArcIterator<fst::StdFst> aiter{encoder, visitState.state};
                for (; !aiter.Done(); aiter.Next()) {
                    const auto &arc = aiter.Value();
                    auto olabels = visitState.olabels;
                    if (arc.olabel != IDX_EPSILON)
                        olabels.push_back(arc.olabel);
                    queue.emplace(std::move(olabels), arc.nextstate);
                }
            }
        }

        queue.pop();
    }

    // remove those do not comply with LPM encoding
    struct EncodedSequence {
        explicit EncodedSequence(std::vector<int> olabels,
                                 std::vector<size_t> token_lengths)
            : olabels{std::move(olabels)},
              token_lengths{std::move(token_lengths)} {}

        std::vector<int> olabels;
        std::vector<size_t> token_lengths;
    };

    std::map<std::string, EncodedSequence> str2seq;
    for (const auto &seq : sequences) {
        std::string output;
        std::vector<size_t> token_lengths;
        for (auto olabel : seq) {
            auto token = encoder.OutputSymbols()->Find(olabel);
            auto utf8_token = ToUtf8(token);
            output += token;
            token_lengths.push_back(utf8_token.size());
        }
        EncodedSequence candidate{seq, std::move(token_lengths)};
        auto it = str2seq.find(output);
        if (it == str2seq.end())
            str2seq.emplace(std::move(output), std::move(candidate));
        else if (candidate.olabels.size() < it->second.olabels.size())
            it->second = std::move(candidate);
        else if (candidate.olabels.size() == it->second.olabels.size())
            QBZ_ASSERT(false, "This should not happen");
    }

    std::vector<std::vector<int>> result;
    result.reserve(str2seq.size());
    for (auto &pair : str2seq) {
        result.push_back(std::move(pair.second.olabels));
    }

    std::sort(result.begin(), result.end(),
              [](const std::vector<int> &a, const std::vector<int> &b) {
                  return a.size() < b.size();
              });

    return result;
}


template<typename Iterator>
auto ExtractCharacters(Iterator begin, Iterator end)
-> std::set<typename std::iterator_traits<typename std::iterator_traits<Iterator>::value_type::iterator>::value_type> {
    using Char = typename std::iterator_traits<typename std::iterator_traits<Iterator>::value_type::iterator>::value_type;
    std::set<Char> characters;
    for (auto it = begin; it != end; ++it) {
        for (auto c : *it) characters.insert(c);
    }

    return characters;
}

} // namespace qbz

#endif // QUERYBLAZER_ENCODER_H
