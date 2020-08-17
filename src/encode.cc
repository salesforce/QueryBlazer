/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include <iostream>
#include "common.h"
#include "encoder.h"
#include "fst/fstlib.h"

using namespace qbz;

int Usage(const char *program) {
    std::cerr << "Usage: " << program << " ENCODER INPUT" << std::endl;
    std::cerr << "\tENCODER: encoder FST" << std::endl;
    std::cerr << "\tINPUT: input text file to encode" << std::endl;
    return EXIT_FAILURE;
}

int main(int argc, const char **argv) {
    std::ios::sync_with_stdio(false);

    if (argc != 3) return Usage(argv[0]);
    auto encoder = fst::StdFst::Read(argv[1]);
    QBZ_ASSERT(encoder, "Failed to read encoder " + std::string{argv[1]});
    std::string line;
    std::ifstream ifs{argv[2]};
    QBZ_ASSERT(ifs, "Failed to read input " + std::string{argv[2]});

    fst::SortedMatcher<fst::StdFst> matcher{encoder, fst::MatchType::MATCH_INPUT, IDX_UNK + 1};
    matcher.SetState(encoder->Start());
    QBZ_ASSERT(matcher.Find(encoder->InputSymbols()->Find(ToString({SPACE}))),
               "space char not found in the encoder");
    // start state (initial space transition)
    const auto start = matcher.Value().nextstate;

    while (std::getline(ifs, line)) {
        // join multi-space into one
        line = Join(Split(line), " ");
        auto utf8_line = ToUtf8(line);
        std::replace(utf8_line.begin(), utf8_line.end(), static_cast<char32_t>(' '), SPACE);
        std::vector<int> ilabels;
        ilabels.reserve(utf8_line.size());
        std::vector<std::string> oovs;
        for (auto utf8_char : utf8_line) {
            auto c = ToString({utf8_char});
            auto ilabel = encoder->InputSymbols()->Find(c);
            if (ilabel == fst::kNoLabel) {
                ilabel = IDX_UNK;
                oovs.push_back(std::move(c));
            }
            ilabels.push_back(ilabel);
        }

        auto olabels = Encode(*encoder, matcher, start, ilabels, true);

        std::vector<std::string> output;
        auto idx = 0;
        auto prev_oov = false;
        for (auto olabel : olabels) {
            if (olabel == IDX_UNK) {
                // each consecutive OOV chars are written as a single token
                if (prev_oov)
                    output.back() += oovs.at(idx++);
                else
                    output.push_back(std::move(oovs.at(idx++)));
                prev_oov = true;
            }
            else {
                output.push_back(encoder->OutputSymbols()->Find(olabel));
                prev_oov = false;
            }
        }
        QBZ_ASSERT(idx == oovs.size(), "OOV size mismatch");

        std::cout << Join(output) << std::endl;
    }

    delete encoder;
    return 0;
}