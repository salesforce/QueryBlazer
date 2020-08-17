/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include <iostream>
#include "queryblazer.h"

int Usage(const char* program) {
    std::cerr << "Usage: " << program << " ENCODER MODEL PRECOMPUTED PREFIX_FILE" << std::endl;
    std::cerr << "ENCODER: LPM encoder in FST" << std::endl;
    std::cerr << "MODEL: ngram language model in FST" << std::endl;
    std::cerr << "PRECOMPUTED: precomputed binary if available; use '-' if not" << std::endl;
    std::cerr << "PREFIX_FILE: a file with prefix in each line to trigger autocomplete" << std::endl;
    return EXIT_FAILURE;
}

using namespace qbz;

int main(int argc, const char** argv) {
    std::ios::sync_with_stdio(false);
    if (argc != 5) return Usage(argv[0]);
    const std::string precomputed{argv[3]};
    const std::string prefixes{argv[4]};

    QueryBlazer completer{argv[1], argv[2],
                          Config{30, 30, 10, 100, false}};
    if (std::string{"-"} != precomputed) {
        std::cerr << "Loading precomputed from " << precomputed << std::endl;
        completer.LoadPrecomputed(precomputed);
    }

    std::ifstream ifs{prefixes};
    QBZ_ASSERT(ifs, "Error reading " +prefixes);
    std::vector<std::string> candidates(completer.GetConfig().topk);
    std::string prefix;

    auto t_start = std::chrono::high_resolution_clock::now();
    size_t count = 0;
    while (std::getline(ifs, prefix)) {
        auto completions = completer.Complete(prefix).first;
        for (auto idx = 0; idx < candidates.size(); ++idx)
            candidates.at(idx) = std::move(completions.at(idx).first);

        std::cout << Join(candidates, "\t") << std::endl;
        ++count;
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(t_end - t_start).count();
    std::cerr << "Completion speed: " << static_cast<double>(count) / duration << " QPS" << std::endl;

    return 0;
}