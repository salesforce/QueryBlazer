/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include "common.h"
#include "encoder.h"
#include "fst/fstlib.h"
#include "mpc.h"
#include <iostream>
#include <map>

using namespace qbz;

int Usage(const char* program) {
    std::cerr << "Usage: " << program << " TRAIN_FILE TRIE COMPLETIONS" << std::endl;
    std::cerr << "\tTRAIN_FILE: query history file to train from" << std::endl;
    std::cerr << "\tTRIE: output trie in FST" << std::endl;
    std::cerr << "\tCOMPLETIONS: completion serialization file" << std::endl;

    return EXIT_FAILURE;
}

std::pair<std::vector<std::string>, std::vector<size_t>>
CountQueries(const std::string &file) {
    std::string query;
    std::ifstream ifs{file};
    QBZ_ASSERT(ifs, "Error reading " + file);

    std::unordered_map<std::string, size_t> counter;
    while (std::getline(ifs, query)) {
        auto it = counter.find(query);
        if (it == counter.end())
            counter.emplace(std::move(query), 1);
        else
            ++it->second;
    }

    std::vector<std::string> queries;
    std::vector<size_t> counts;
    queries.reserve(counter.size());
    counts.reserve(counter.size());
    for (auto it = counter.begin(); it != counter.end(); ) {
        queries.push_back(it->first);
        counts.push_back(it->second);
        it = counter.erase(it);
    }

    return {queries, counts};
}

std::pair<std::vector<std::string>, std::vector<size_t>>
CopyToFst(const Trie<int> &mpc, fst::StdVectorFst &trie) {
    std::vector<size_t> counts{0}; // start state
    std::vector<std::string> queries(1); // queries at each state if final
    std::queue<std::pair<const PrefixNode<int, Trie<int>::Data>*, int>> queue;
    queue.emplace(&mpc.Root(), trie.Start());
    while (!queue.empty()) {
        const auto &pair = queue.front();
        for (auto child : pair.first->Children()) {
            auto nextstate = trie.AddState();
            counts.push_back(0);
            queries.emplace_back("");
            trie.AddArc(pair.second, fst::StdArc(child.first, child.first, nextstate));
            queue.emplace(child.second, nextstate);
        }

        if (pair.first->Data()) {
            trie.SetFinal(pair.second);
            counts.at(pair.second) = pair.first->Data()->count;
            for (auto ilabel : pair.first->Prefix()) {
                queries.at(pair.second) += trie.InputSymbols()->Find(ilabel);
            }
        }

        queue.pop();
    }

    return {queries, counts};
}

int main(int argc, const char** argv) {
    std::ios::sync_with_stdio(false);
    if (argc != 4) return Usage(argv[0]);

    std::vector<std::string> queries;
    std::vector<size_t> counts;
    std::tie(queries, counts) = CountQueries(argv[1]);
    std::vector<Utf8> utf8_queries;
    utf8_queries.reserve(queries.size());
    for (const auto &query : queries) utf8_queries.push_back(ToUtf8(query));

    auto vocab = ExtractCharacters(utf8_queries.begin(), utf8_queries.end());

    // create FST
    auto p_trie = new fst::StdVectorFst;
    fst::StdVectorFst &trie = *p_trie;
    trie.SetStart(trie.AddState());
    auto symtable = new fst::SymbolTable;
    for (const auto &symbol : {SYMBOL_EPSILON, SYMBOL_PHI, SYMBOL_BOS, SYMBOL_EOS, SYMBOL_UNK})
        symtable->AddSymbol(symbol);

    for (auto c : vocab)
        symtable->AddSymbol(ToString({c}));

    trie.SetInputSymbols(symtable);
    trie.SetOutputSymbols(symtable);

    {
        Trie<int> mpc;
        std::cerr << "Building a prefixtree..." << std::endl;
        for (auto i = 0; i < utf8_queries.size(); ++i) {
            std::vector<int> ilabels;
            const auto &query = utf8_queries.at(i);
            ilabels.reserve(query.size());
            for (auto c : query)
                ilabels.push_back(symtable->Find(ToString({c})));
            mpc.Insert(ilabels, counts.at(i));
        }

        std::cerr << "Copying to an FST" << std::endl;
        // count per FST state
        std::tie(queries, counts) = CopyToFst(mpc, trie);
    }

    std::cerr << "Converting to ConstFST" << std::endl;
    {
        fst::StdConstFst const_trie{trie};
        delete p_trie;
        const_trie.Write(argv[2]);
    }

    std::cerr << "Precomputing topk completions" << std::endl;
    Mpc completions(argv[2], std::move(queries), std::move(counts));
    completions.FindCompletions(10);
    std::cerr << "Writing to " << argv[3] << std::endl;
    QBZ_ASSERT(completions.Save(argv[3]), "Error saving to " + std::string{argv[3]});

    delete symtable;
    return 0;
}