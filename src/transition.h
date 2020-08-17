/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef QUERYBLAZER_EMITTINGPHI_H
#define QUERYBLAZER_EMITTINGPHI_H

#include "common.h"
#include "fst/fstlib.h"

namespace qbz {

/**
 * Make transitions (possibly phi-transition)
 * @param graph: deterministic fst graph
 * @param matcher: fst matcher to use (either sorted or generic); this is
 * because construction of matcher is very slow
 * @param ilabel: if phi, will take phi transition(s); else, will make viable
 * transition (including phi)
 * @param olabels: olabel vector to which to append olabels
 * @param out_state: if not null, destination state will be written
 * @return: output labels collected during the transition (will ignore epsilon
 * outputs)
 */
template <typename FST, typename Matcher>
void MakeTransitions(const FST &graph, Matcher &matcher, int in_state,
                     int ilabel, std::vector<int> *olabels = nullptr,
                     int *out_state = nullptr) {
    auto state = in_state;
    auto break_flag = false;

    while (!break_flag) {
        matcher.SetState(state);
        if (matcher.Find(ilabel)) {
            break_flag = true;
        } else {
            QBZ_ASSERT(matcher.Find(IDX_PHI),
                       "no viable transition found at state " +
                           std::to_string(state));
        }
        const auto &arc = matcher.Value();
        if (olabels && arc.olabel != IDX_EPSILON)
            olabels->push_back(arc.olabel);
        state = arc.nextstate;
    }

    // make phi transition if state is not an exit state and state has only a
    // single phi transition
    while (graph.Final(state) == fst::StdArc::Weight::Zero() &&
           graph.NumArcs(state) == 1) {
        matcher.SetState(state);
        if (!matcher.Find(IDX_PHI)) break;
        const auto &arc = matcher.Value();
        if (olabels && arc.olabel != IDX_EPSILON)
            olabels->push_back(arc.olabel);
        state = arc.nextstate;
    }

    if (out_state) *out_state = state;
}

/**
 * Make phi-transitions until exit state is reached
 * @param graph: deterministic graph
 */
template <typename FST, typename Matcher>
float MakeExitTransitions(const FST &graph, Matcher &matcher, int in_state,
                          std::vector<int> *olabels = nullptr,
                          int *out_state = nullptr) {
    auto state = in_state;
    float cost = 0.0f;

    while (graph.Final(state) == fst::StdArc::Weight::Zero()) {
        matcher.SetState(state);
        QBZ_ASSERT(matcher.Find(IDX_PHI), "Final state transitions not found");
        const auto &arc = matcher.Value();
        state = arc.nextstate;
        cost += arc.weight.Value();
        if (arc.olabel != IDX_EPSILON && olabels)
            olabels->push_back(arc.olabel);
    }

    cost += graph.Final(state).Value();
    if (out_state) *out_state = state;

    return cost;
}

} // namespace qbz

#endif // QUERYBLAZER_EMITTINGPHI_H
