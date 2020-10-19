/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or
 * https://opensource.org/licenses/BSD-3-Clause
 */

#include "queryblazer.h"

using namespace qbz;

int Usage(const char *program) {
    std::cerr << "Usage: " << program << " ENCODER LM PRECOMPUTED" << std::endl;
    std::cerr << "\tENCODER: subword encoder FST" << std::endl;
    std::cerr << "\tLM: subword language model FST build from the query log"
              << std::endl;
    std::cerr << "\tPRECOMPUTED: beam search result output" << std::endl;

    return EXIT_FAILURE;
}

int main(int argc, const char **argv) {
    if (argc != 4) return Usage(argv[0]);

    Config config{30,  30,   10,
                  100, true, false}; // TODO: get config from user input
    QueryBlazer queryBlazer{argv[1], argv[2], config};
    QBZ_ASSERT(queryBlazer.SavePrecomputed(argv[3]), "Precomputation failed");

    return 0;
}