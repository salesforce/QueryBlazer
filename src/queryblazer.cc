/*
 * Copyright (c) 2018, salesforce.com, inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */

#include "queryblazer.h"
#include "mpc.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;
using namespace qbz;

PYBIND11_MODULE(queryblazer, m) {
    py::class_<Config>(m, "Config")
        .def(py::init<size_t, size_t, size_t, size_t, int, bool>(),
             py::arg("branch_factor") = 30, py::arg("beam_size") = 30,
             py::arg("topk") = 10, py::arg("length_limit") = 100,
             py::arg("precompute") = false, py::arg("verbose") = false);

    py::class_<qbz::QueryBlazer>(m, "QueryBlazer")
        .def(py::init<const std::string &, const std::string &,
                      const Config &>(),
             py::arg("encoder"), py::arg("model"),
             py::arg("config") = Config{})
        .def("Complete", &QueryBlazer::Complete, py::arg("query"))
        .def("LoadPrecomputed", &QueryBlazer::LoadPrecomputed,
             py::arg("input_file"))
        .def("SavePrecomputed", &QueryBlazer::SavePrecomputed,
             py::arg("output_file"));

    py::class_<Mpc>(m, "Mpc")
        .def(py::init<const std::string &, const std::string &>(),
             py::arg("trie"), py::arg("mpc"))
        .def("Complete", &Mpc::Complete, py::arg("prefix"));
}
