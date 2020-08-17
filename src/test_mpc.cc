/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include <iostream>
#include <chrono>
#include "mpc.h"

using namespace qbz;

int Usage(const char* program) {
    std::cerr << "Usage: " << program << " TRIE PRECOMPUTED PREFIX_FILE" << std::endl;
    std::cerr << "\tTRIE: trie storing query history in FST" << std::endl;
    std::cerr << "\tPRECOMPUTED: serialized precomputed result" << std::endl;
    std::cerr << "\tPREFIX_FILE: file containing prefixes to trigger autocomplete" << std::endl;
    return EXIT_FAILURE;
}

int main(int argc, const char **argv) {
    std::ios::sync_with_stdio(false);
    if (argc != 4) return Usage(argv[0]);
    Mpc mpc{argv[1], argv[2]};
    std::string query;
    std::vector<std::string> candidates;
    std::ifstream ifs{argv[3]};
    QBZ_ASSERT(ifs, "Error loading " + std::string{argv[3]});

    auto t_start = std::chrono::high_resolution_clock::now();
    size_t count = 0;
    while (std::getline(ifs, query)) {
        auto completions = mpc.Complete(query);
        candidates.resize(completions.size());
        for (auto idx = 0; idx < completions.size(); ++idx)
            candidates.at(idx) = std::move(completions.at(idx).first);
        std::cout << Join(candidates, "\t") << std::endl;
        ++count;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(t_end - t_start).count();
    std::cerr << "Completion speed: " << static_cast<double>(count) / duration << " QPS" << std::endl;

    return 0;
}