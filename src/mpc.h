/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_MPC_H
#define QUERYBLAZER_MPC_H

#include "prefix_tree.h"
#include "boost/serialization/utility.hpp"
#include "boost/serialization/vector.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/archive/binary_iarchive.hpp"

namespace qbz {

class Mpc {
  public:
    explicit Mpc(const std::string &trie_file,
                            const std::string &serialized)
        : trie{fst::StdExpandedFst::Read(trie_file)} {
        QBZ_ASSERT(trie, "Error reading " + trie_file);
        QBZ_ASSERT(Load(serialized), "Error lading from " + serialized);
    }

    /**
     * @param queries: query for each (final) state
     * @param counts : count for each (final) state
     */
    explicit Mpc(const std::string &trie_file,
                            std::vector<std::string> &&queries,
                            std::vector<size_t> &&counts)
        : queries{std::move(queries)},
          counts{std::move(counts)},
          completions(this->queries.size()),
          trie{fst::StdExpandedFst::Read(trie_file)} {
        QBZ_ASSERT(trie, "Error reading " + trie_file);
        QBZ_ASSERT(trie->NumStates() == this->counts.size(), "queries & counts size mismatch");
        QBZ_ASSERT(this->queries.size() == this->counts.size(), "queries & counts size mismatch");
    }

    void FindCompletions(size_t topk) {
        TopK(trie->Start(), topk);
    }

    std::vector<std::pair<std::string, size_t>> Complete(const std::string &prefix) {
        std::vector<std::pair<std::string, size_t>> result;
        fst::SortedMatcher<fst::StdExpandedFst> matcher{*trie, fst::MatchType::MATCH_INPUT, IDX_UNK + 1};
        auto state = trie->Start();
        for (auto c : ToUtf8(prefix)) {
            matcher.SetState(state);
            auto ilabel = trie->InputSymbols()->Find(ToString({c}));
            if (!matcher.Find(ilabel)) return result;
            state = matcher.Value().nextstate;
        }

        result.reserve(completions.at(state).size());
        for (const auto &pair : completions.at(state)) {
            result.emplace_back(queries.at(pair.second), pair.first);
        }

        return result;
    }

    bool Save(const std::string &file) {
        std::ofstream ofs{file};
        if (!ofs) return false;

        boost::archive::binary_oarchive oarchive{ofs};
        oarchive << trie->NumStates();
        oarchive << completions;
        oarchive << queries;

        return true;
    }

  private:
    /**
     * Return {count, query_idx} up to topK
     * @param state
     * @param k
     */
    void TopK(int state,
              size_t topk) {
        if (!completions.at(state).empty()) return;
        std::vector<std::pair<size_t, size_t>> result;
        // if this is the final state, then add its own query
        if (!queries.at(state).empty()) {
            result.emplace_back(counts.at(state), state);
        }

        fst::ArcIterator<fst::StdExpandedFst> aiter{*trie, state};
        for (; !aiter.Done(); aiter.Next()) {
            auto nextstate = aiter.Value().nextstate;
            TopK(nextstate, topk);
            const auto &candidates = completions.at(nextstate);
            result.insert(result.end(), candidates.begin(), candidates.end());
        }
        auto n = std::min(result.size(), topk);
        std::partial_sort(result.begin(), result.begin() + n, result.end(),
                          std::greater<std::pair<size_t, size_t>>{});
        result.erase(result.begin() + n, result.end());
        completions.at(state) = std::move(result);
    }

    bool Load(const std::string &file) {
        std::ifstream ifs{file};
        if (!ifs) return false;

        boost::archive::binary_iarchive iarchive{ifs};
        int n;
        iarchive >> n;
        if (n != trie->NumStates()) return false;
        iarchive >> completions;
        if (completions.size() != n) return false;
        iarchive >> queries;
        if (queries.size() != n) return false;

        return true;
    }

    // completion queries to be referenced by its idx
    std::vector<std::string> queries;
    std::vector<size_t> counts;
    // topk completion score & indices at each state
    std::vector<std::vector<std::pair<size_t, size_t>>> completions;
    std::unique_ptr<const fst::StdExpandedFst> trie;
};

}

#endif // QUERYBLAZER_MPC_H
