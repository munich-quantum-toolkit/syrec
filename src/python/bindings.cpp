/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/simple_simulation.hpp"
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
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
            .def_property_readonly("qubit_labels", &AnnotatableQuantumComputation::getQubitLabels, "Get the label of each qubit in the quantum computation")
            .def("get_quantum_cost_for_synthesis", &AnnotatableQuantumComputation::getQuantumCostForSynthesis, "Get the quantum cost to synthesis the quantum computation")
            .def("get_transistor_cost_for_synthesis", &AnnotatableQuantumComputation::getTransistorCostForSynthesis, "Get the transistor cost to synthesis the quantum computation")
            .def("get_annotations_of_quantum_operation", &AnnotatableQuantumComputation::getAnnotationsOfQuantumOperation, "quantum_operation_index_in_quantum_operation"_a, "Get the annotations of a specific quantum operation in the quantum computation");

    py::class_<NBitValuesContainer>(m, "n_bit_values_container")
            .def(py::init<>(), "Constructs an empty container of size zero.")
            .def(py::init<std::size_t>(), "n"_a, "Constructs a zero-initialized container of size n.")
            .def(py::init<std::size_t, uint64_t>(), "n"_a, "initialLineValues"_a, "Constructs a container of size n from an integer initialLineValues")
            .def("__getitem__", [](const NBitValuesContainer& nBitValuesContainer, std::size_t bitIndex) { return nBitValuesContainer[bitIndex]; })
            .def("test", &NBitValuesContainer::test, "n"_a, "Determine the value of the bit at position n")
            .def("set", py::overload_cast<std::size_t>(&NBitValuesContainer::set), "n"_a, "Set the value of the bit at position n to TRUE")
            .def("set", py::overload_cast<std::size_t, bool>(&NBitValuesContainer::set), "n"_a, "value"_a, "Set the bit at position n to a specific value")
            .def("reset", &NBitValuesContainer::reset, "n"_a, "Set the value of the bit at position n to FALSE")
            .def("resize", &NBitValuesContainer::resize, "n"_a, "Changes the number of bits stored in the container")
            .def("size", &NBitValuesContainer::size, "Get the number of values stored in the container")
            .def("flip", &NBitValuesContainer::flip, "n"_a, "Flip the value of the bit at position n")
            .def(
                    "__str__", [](const NBitValuesContainer& container) {
                        return container.stringify();
                    },
                    "Returns a string containing the stringified values of the stored bits.");

    py::class_<Properties, std::shared_ptr<Properties>>(m, "properties")
            .def(py::init<>(), "Constructs property map object.")
            .def("set_string", &Properties::set<std::string>)
            .def("set_bool", &Properties::set<bool>)
            .def("set_int", &Properties::set<int>)
            .def("set_unsigned", &Properties::set<unsigned>)
            .def("set_double", &Properties::set<double>)
            .def("get_string", py::overload_cast<const std::string&>(&Properties::get<std::string>, py::const_))
            .def("get_double", py::overload_cast<const std::string&>(&Properties::get<double>, py::const_));

    py::enum_<utils::IntegerConstantTruncationOperation>(m, "integer_constant_truncation_operation")
            .value("modulo", utils::IntegerConstantTruncationOperation::Modulo, "Use the modulo operation for the truncation of constant values")
            .value("bitwise_and", utils::IntegerConstantTruncationOperation::BitwiseAnd, "Use the bitwise AND operation for the truncation of constant values")
            .export_values();

    py::class_<ReadProgramSettings>(m, "read_program_settings")
            .def(py::init<>(), "Constructs ReadProgramSettings object.")
            .def_readwrite("default_bitwidth", &ReadProgramSettings::defaultBitwidth, "Defines the default variable bitwidth used by the SyReC parser for variables whose bitwidth specification was omitted")
            .def_readwrite("integer_constant_truncation_operation", &ReadProgramSettings::integerConstantTruncationOperation, "Defines the operation used by the SyReC parser for the truncation of integer constant values. For further details we refer to the semantics of the SyReC language")
            .def_readwrite("allow_access_on_assigned_to_variable_parts_in_dimension_access_of_variableAccess", &ReadProgramSettings::allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess, "Defines whether an access on the assigned to signal parts of an assigned is allowed in variable accesses defined in any operand of the assignment. For further details we refer to the semantics of the SyReC language.");

    py::class_<Program>(m, "program")
            .def(py::init<>(), "Constructs SyReC program object.")
            .def("add_module", &Program::addModule)
            .def("read", &Program::read, "filename"_a, "settings"_a = ReadProgramSettings{}, "Read and process a SyReC program from a file.")
            .def("read_from_string", &Program::readFromString, "stringifiedProgram"_a, "settings"_a = ReadProgramSettings{}, "Process an already stringified SyReC program.");

    m.def("cost_aware_synthesis", &CostAwareSynthesis::synthesize, "annotated_quantum_computation"_a, "program"_a, "settings"_a = Properties::ptr(), "statistics"_a = Properties::ptr(), "Cost-aware synthesis of the SyReC program.");
    m.def("line_aware_synthesis", &LineAwareSynthesis::synthesize, "annotated_quantum_computation"_a, "program"_a, "settings"_a = Properties::ptr(), "statistics"_a = Properties::ptr(), "Line-aware synthesis of the SyReC program.");
    m.def("simple_simulation", &simpleSimulation, "output"_a, "quantum_computation"_a, "input"_a, "statistics"_a = Properties::ptr(), "Simulation of a synthesized SyReC program");
}
