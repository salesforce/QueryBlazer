/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include "fst/fstlib.h"
#include "common.h"
#include "encoder.h"

using namespace qbz;

int Usage(const char *program) {
    std::cerr << "Usage: " << program << " ENCODER" << std::endl;
    std::cerr << "\tENCODER: LPM encoder in FST" << std::endl;

    return EXIT_FAILURE;
}

int main(int argc, const char **argv) {
    std::ios::sync_with_stdio(false);
    if (argc != 2) return Usage(argv[0]);
    auto encoder = fst::StdExpandedFst::Read(argv[1]);
    QBZ_ASSERT(encoder, "Failed to read encoder " + std::string{argv[1]});
    for (auto state = 0; state < encoder->NumStates(); ++state) {
        auto candidates = CandidateOlabels(*encoder, state);
        std::cout << "State " << state << ": " << candidates.size() << " candidates" << std::endl;
        std::set<std::string> outputs;
        for (const auto &seq : candidates) {
            std::string output;
            for (auto olabel : seq)
                output += encoder->OutputSymbols()->Find(olabel);
            std::cout << output << std::endl;
            QBZ_ASSERT(outputs.insert(std::move(output)).second, "Same output string not filtered");
        }
        std::cout << std::endl;
    }

    return 0;
}