/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_MATCHER_H
#define QUERYBLAZER_MATCHER_H

#include "fst/fstlib.h"

namespace qbz {

template <typename FST>
class UnsortedMatcher {
    using Arc = typename FST::Arc;

  public:
    explicit UnsortedMatcher(const FST *graph)
        : graph{graph},
          state{graph->Start()} {}

    void SetState(int state) { this->state = state; }

    bool Find(int label) {
        fst::ArcIterator<FST> aiter{*graph, state};
        auto found = false;
        for (; !aiter.Done(); aiter.Next()) {
            const auto &a = aiter.Value();
            if (label == a.ilabel) {
                arc = a;
                found = true;
                break;
            }
        }
        return found;
    }

    const Arc &Value() { return arc; }

  private:
    const FST *graph;
    int state;
    Arc arc;
};

} // namespace qbz
#endif // QUERYBLAZER_MATCHER_H
