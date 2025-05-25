/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/quantum_computation_simulation_for_state.hpp"
#include "algorithms/synthesis/quantum_computation_synthesis_cost_metrics.hpp"
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"
#include "ir/QuantumComputation.hpp"

#include <functional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;
using namespace syrec;

PYBIND11_MODULE(pysyrec, m) {
    py::module::import("mqt.core.ir");
    m.doc() = "Python interface for the SyReC programming language for the synthesis of reversible circuits";

    py::class_<AnnotatableQuantumComputation, qc::QuantumComputation>(m, "annotatable_quantum_computation")
            .def(py::init<>(), "Constructs an annotatable quantum computation")
            .def_property_readonly("qubit_labels", &AnnotatableQuantumComputation::getQubitLabels, "Get the qubit labels of the quantum computation")
            .def(
                    "get_quantum_cost_for_synthesis", [](const AnnotatableQuantumComputation& annotatableQuantumComputation) { return getQuantumCostForSynthesis(annotatableQuantumComputation); }, "Get the quantum costs of the circuit")
            .def(
                    "get_transistor_cost_for_synthesis", [](const AnnotatableQuantumComputation& annotatableQuantumComputation) { return getTransistorCostForSynthesis(annotatableQuantumComputation); }, "Get the transistor costs of the circuit")
            .def("get_annotations_of_quantum_operation", &AnnotatableQuantumComputation::getAnnotationsOfQuantumOperation, "indexOfQuantumOperationInQuantumComputation"_a, "Get the annotations for specific quantum operation in the quantum computation");

    py::class_<Properties, std::shared_ptr<Properties>>(m, "properties")
            .def(py::init<>(), "Constructs property map object.")
            .def("set_string", &Properties::set<std::string>)
            .def("set_bool", &Properties::set<bool>)
            .def("set_int", &Properties::set<int>)
            .def("set_unsigned", &Properties::set<unsigned>)
            .def("set_double", &Properties::set<double>)
            .def("get_string", py::overload_cast<const std::string&>(&Properties::get<std::string>, py::const_))
            .def("get_double", py::overload_cast<const std::string&>(&Properties::get<double>, py::const_));

    py::class_<ReadProgramSettings>(m, "read_program_settings")
            .def(py::init<>(), "Constructs ReadProgramSettings object.")
            .def_readwrite("default_bitwidth", &ReadProgramSettings::defaultBitwidth);

    py::class_<Program>(m, "program")
            .def(py::init<>(), "Constructs SyReC program object.")
            .def("add_module", &Program::addModule)
            .def("read", &Program::read, "filename"_a, "settings"_a = ReadProgramSettings{}, "Read a SyReC program from a file.");

    m.def("cost_aware_synthesis", &CostAwareSynthesis::synthesize, "annotatedQuantumComputation"_a, "program"_a, "settings"_a = Properties::ptr(), "statistics"_a = Properties::ptr(), "Cost-aware synthesis of the SyReC program.");
    m.def("line_aware_synthesis", &LineAwareSynthesis::synthesize, "annotatedQuantumComputation"_a, "program"_a, "settings"_a = Properties::ptr(), "statistics"_a = Properties::ptr(), "Line-aware synthesis of the SyReC program.");
    // Due to pybind11 no being able to handle pass by reference STL containers as parameter (which will create and use a copy instead) the output state of the simulation is return from the function.
    // If one were to use opaque types for STL containers, then pass by reference of such containers would be possible but would require the user to use the opaque type for the parameter.
    // See: https://pybind11.readthedocs.io/en/stable/advanced/cast/stl.html#automatic-conversion
    m.def("quantum_computation_simulation_for_state", &simulateQuantumComputationExecutionForState, "quantumComputation"_a, "inputState"_a, "statistics"_a = Properties::ptr(), "Simulation of a quantum computation for a given input state");
}
