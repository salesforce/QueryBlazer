/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include <iostream>
#include <set>

#include "common.h"
#include "encoder.h"
#include "fst/fstlib.h"
#include "matcher.h"
#include "transition.h"

using namespace qbz;

int Usage(const char *program) {
    std::cerr << "Usage: " << program << " VOCAB_FILE ENCODER_OUTPUT" << std::endl;
    std::cerr << "\tVOCAB_FILE: path to vocabulary file, obtained from sentencepiece" << std::endl;
    std::cerr << "\tENCODER_OUTPUT: path to save encoder FST" << std::endl;
    return EXIT_FAILURE;
}

std::vector<std::string> ReadVocabulary(const std::string &vocab_file) {
    std::set<std::string> vocabulary;
    std::string line;
    std::ifstream ifs{vocab_file};
    QBZ_ASSERT(ifs, "Failed to read " + vocab_file);
    while (std::getline(ifs, line)) {
        auto subword = Split(line);
        QBZ_ASSERT(subword.size() == 1, "Invalid vocab file format: " + vocab_file);
        vocabulary.insert(std::move(subword.front()));
    }

    // remove special symbols from the vocabulary
    for (const auto &symbol : {SYMBOL_UNK, SYMBOL_BOS, SYMBOL_EOS})
        vocabulary.erase(symbol);

    QBZ_LOG("Successfully read " + std::to_string(vocabulary.size()) + " valid tokens");
    return {vocabulary.begin(), vocabulary.end()};
}

void AddToken(fst::StdVectorFst *graph, const std::string &token, const Utf8 &utoken) {
    auto src = graph->Start();
    for (auto c : utoken) {
        auto dst = graph->AddState();
        graph->AddArc(src, fst::StdArc(graph->InputSymbols()->Find(ToString({c})),
                                       IDX_EPSILON,
                                       dst));
        src = dst;
    }
    graph->AddArc(src, fst::StdArc(IDX_PHI,
                                   graph->OutputSymbols()->Find(token),
                                   graph->Start()));
}

void AddPhiTransitions(fst::StdVectorFst *graph) {
    struct TraverseState {
        explicit TraverseState(int state, int prev_state, int olabel = IDX_EPSILON) :
                state{state}, prev_state{prev_state}, ilabel{olabel} {}

        int state; // current state
        int prev_state; // previous state (which must have transitions to every ilabel)
        int ilabel; // ilabel from prev_state to state
    };
    std::set<int> visitedStates;
    std::queue<TraverseState> queue;
    queue.emplace(graph->Start(), graph->Start());
    // use unsorted matcher (linear search)
    UnsortedMatcher<fst::StdFst> matcher{graph};
    while (!queue.empty()) {
        auto traverseState = queue.front();
        queue.pop();

        const auto state = traverseState.state;
        const auto prev_state = traverseState.prev_state;
        const auto ilabel = traverseState.ilabel;

        QBZ_ASSERT(visitedStates.find(state) == visitedStates.end(),
                   "state " + std::to_string(state) + " visited again");
        visitedStates.insert(state);

        auto to_add_phi = state != graph->Start(); // no need to add phi at start state
        fst::ArcIterator<fst::StdVectorFst> aiter{*graph, state};
        for (; !aiter.Done(); aiter.Next()) {
            const auto &arc = aiter.Value();
            if (arc.ilabel == IDX_PHI) {
                to_add_phi = false;
                continue;
            }
            if (arc.nextstate == graph->Start()) continue;
            queue.emplace(arc.nextstate, state, arc.ilabel);
        }

        if (to_add_phi) {
            // take phi transition from previous state and append olabel
            std::vector<int> olabels;
            int dest;
            MakeTransitions(*graph, matcher, prev_state, IDX_PHI, &olabels, &dest);
            MakeTransitions(*graph, matcher, dest, ilabel, &olabels, &dest);
            // create extra states if olabels is more than 1
            auto s = state;
            for (auto idx = 0; idx < olabels.size() - 1; ++idx) {
                auto temp = graph->AddState();
                graph->AddArc(s, fst::StdArc(IDX_PHI, olabels.at(idx), temp));
                s = temp;
            }
            graph->AddArc(s, fst::StdArc(IDX_PHI, olabels.back(), dest));
        }
    }

    fst::Minimize(graph);
    fst::ArcSort(graph, fst::ILabelCompare<fst::StdArc>{});
}

void BuildPrefixTree(fst::StdVectorFst *graph,
                     const std::vector<std::string> &vocab,
                     const std::vector<Utf8> &utf8_vocab) {
    for (auto i = 0; i < vocab.size(); ++i) {
        AddToken(graph, vocab.at(i), utf8_vocab.at(i));
    }

    fst::Determinize(*graph, graph);
    fst::ArcSort(graph, fst::ILabelCompare<fst::StdArc>{});
}

int main(int argc, const char **argv) {
    std::ios::sync_with_stdio(false);
    if (argc != 3) return Usage(argv[0]);
    auto vocabulary = ReadVocabulary(argv[1]);

    auto isymtable = new fst::SymbolTable;
    for (const auto &symbol : {SYMBOL_EPSILON, SYMBOL_PHI, SYMBOL_BOS, SYMBOL_EOS, SYMBOL_UNK})
        isymtable->AddSymbol(symbol);
    auto osymtable = isymtable->Copy();

    for (const auto &token : vocabulary)
        osymtable->AddSymbol(token);

    std::vector<Utf8> utf8_vocab;
    utf8_vocab.reserve(vocabulary.size());
    for (const auto &token : vocabulary)
        utf8_vocab.push_back(ToUtf8(token));

    auto characters = ExtractCharacters(utf8_vocab.begin(), utf8_vocab.end());
    for (auto c : characters)
        isymtable->AddSymbol(ToString({c}));

    fst::StdVectorFst encoder;
    encoder.SetInputSymbols(isymtable);
    encoder.SetOutputSymbols(osymtable);
    encoder.SetStart(encoder.AddState());
    encoder.SetFinal(encoder.Start());

    BuildPrefixTree(&encoder, vocabulary, utf8_vocab);
    AddPhiTransitions(&encoder);

    fst::StdConstFst const_encoder{encoder}; // convert to const fst for faster speed
    QBZ_ASSERT(const_encoder.Write(argv[2]), "Write to " + std::string{argv[2]} + "failed");

    delete isymtable;
    delete osymtable;
    return 0;
}
