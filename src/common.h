/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_COMMON_H
#define QUERYBLAZER_COMMON_H

#include "fst/fstlib.h"
#include "utf8.h"
#include <cstdlib>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace fst {
REGISTER_FST(VectorFst, StdArc);
REGISTER_FST(ConstFst, StdArc);
} // namespace fst

namespace qbz {

#define QBZ_ASSERT(cond, msg)                                                  \
    if (!(cond)) throw std::runtime_error(msg)

#define QBZ_LOG(msg) std::cerr << msg << std::endl

// space unit, adopted from sentencepiece
constexpr char32_t SPACE = 0x00002581;

// FST symbol table
constexpr char SYMBOL_EPSILON[] = "<eps>";
constexpr char SYMBOL_PHI[] = "<phi>";
constexpr char SYMBOL_BOS[] = "<s>";
constexpr char SYMBOL_EOS[] = "</s>";
constexpr char SYMBOL_UNK[] = "<unk>";

constexpr int IDX_EPSILON = 0;
constexpr int IDX_PHI = 1;
constexpr int IDX_BOS = 2;
constexpr int IDX_EOS = 3;
constexpr int IDX_UNK = 4;

// String operations
template <typename UnaryPredicate>
std::vector<std::string> Split(const std::string &input, UnaryPredicate pred) {
    std::vector<std::string> result;
    auto begin = input.begin();
    for (auto end = input.begin(); end != input.end(); ++end) {
        const auto c = *end;
        if (pred(c)) {
            if (begin != end) result.emplace_back(begin, end);
            begin = end + 1;
        }
    }
    if (begin != input.end()) result.emplace_back(begin, input.end());
    return result;
}

std::string Join(const std::vector<std::string> &tokens,
                 const std::string &delimiter = " ") {
    std::string result;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        result += *it;
        if (it != tokens.end() - 1) result += delimiter;
    }
    return result;
}

std::vector<std::string> Split(const std::string &input) {
    return Split(input, [](char c) { return std::isspace(c); });
}

using Utf8 = std::vector<char32_t>;

// utf8 operations
Utf8 ToUtf8(const std::string &input) {
    Utf8 output;
    QBZ_ASSERT(utf8::find_invalid(input.begin(), input.end()) == input.end(),
               "Invalid UTF8 string: " + input);

    output.reserve(utf8::distance(input.begin(), input.end()));
    auto it = input.data();
    auto end_it = input.data() + input.size();
    while (it < input.data() + input.size()) {
        output.push_back(utf8::next(it, end_it));
    }
    return output;
}

std::string ToString(const Utf8 &input) {
    std::string output(input.size() * sizeof(*input.data()), '\0');
    auto it = &output.at(0);
    for (auto c : input) { it = utf8::append(c, it); }
    output.erase(output.begin() + (it - output.data()), output.end());
    return output;
}

/**
 * Simple container holding top k, defined by compare
 * @tparam Value
 * @tparam Compare
 */
template <typename Value, typename Compare = std::less<Value>>
class TopK {
  public:
    explicit TopK(size_t k) : k{k} {
        QBZ_ASSERT(k >= 1, "Top K must be positive");
    }

    /**
     * Insert value into topK
     * @param value
     * @return true if value is within top k; else return false
     */
    bool Insert(Value value) {
        if (queue.size() < k || compare(value, queue.top())) {
            queue.push(std::move(value));
            if (queue.size() > k) queue.pop();
            return true;
        }
        return false;
    }

    /**
     * Same as Insert but do not actually insert
     * @param value
     * @return
     */
    bool WillInsert(const Value &value) {
        return queue.size() < k || compare(value, queue.top());
    }

  private:
    const size_t k;
    Compare compare;
    std::priority_queue<Value, std::vector<Value>, Compare> queue;
};

} // namespace qbz

#endif // QUERYBLAZER_COMMON_H
