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
#include "core/circuit.hpp"
#include "core/gate.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"

#include <functional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace pybind11::literals;
using namespace syrec;

PYBIND11_MODULE(pysyrec, m) {
    m.doc() = "Python interface for the SyReC programming language for the synthesis of reversible circuits";

    py::class_<Circuit, std::shared_ptr<Circuit>>(m, "circuit")
            .def(py::init<>(), "Constructs circuit object.")
            .def_property("lines", &Circuit::getLines, &Circuit::setLines, "Returns the number of circuit lines.")
            .def_property_readonly("num_gates", &Circuit::numGates, "Returns the total number of gates in the circuit.")
            .def("__iter__",
                 [](Circuit& circ) { return py::make_iterator(circ.begin(), circ.end()); })
            .def_property("inputs", &Circuit::getInputs, &Circuit::setInputs, "Returns the input names of the lines in a circuit.")
            .def_property("outputs", &Circuit::getOutputs, &Circuit::setOutputs, "Returns the output names of the lines in a circuit.")
            .def_property("constants", &Circuit::getConstants, &Circuit::setConstants, "Returns the constant input line specification.")
            .def_property("garbage", &Circuit::getGarbage, &Circuit::setGarbage, "Returns whether outputs are garbage or not.")
            .def(
                    "annotations", [](const Circuit& c, const Gate& g) {
                        py::dict   d{};
                        const auto annotations = c.getAnnotations(g);
                        if (annotations) {
                            for (const auto& [first, second]: *annotations) {
                                d[py::cast(first)] = second;
                            }
                        }
                        return d;
                    },
                    "This method returns all annotations for a given gate.")
            .def("quantum_cost", &Circuit::quantumCost, "Returns the quantum cost of the circuit.")
            .def("transistor_cost", &Circuit::transistorCost, "Returns the transistor cost of the circuit.")
            .def("to_qasm_str", &Circuit::toQasm, "Returns the QASM representation of the circuit.")
            .def("to_qasm_file", &Circuit::toQasmFile, "filename"_a, "Writes the QASM representation of the circuit to a file.");

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

    py::class_<NBitValuesContainer>(m, "n_bit_values_container")
            .def(py::init<>(), "Constructs an empty container of size zero.")
            .def(py::init<std::size_t>(), "n"_a, "Constructs a zero-initialized container of size n.")
            .def(py::init<std::size_t, uint64_t>(), "n"_a, "initialLineValues"_a, "Constructs a container of size n from an integer initialLineValues")
            .def("test", &NBitValuesContainer::test, "n"_a, "Determine the value of the bit at position n")
            .def("set", py::overload_cast<std::size_t>(&NBitValuesContainer::set), "n"_a, "Set the value of the bit at position n to TRUE")
            .def("set", py::overload_cast<std::size_t, bool>(&NBitValuesContainer::set), "n"_a, "value"_a, "Set the of the bit at position n to a specific value")
            .def("reset", &NBitValuesContainer::reset, "n"_a, "Set the value of the bit at position n to FALSE")
            .def("resize", &NBitValuesContainer::resize, "n"_a, "Changes the number of bits stored in the container")
            .def("flip", &NBitValuesContainer::flip, "n"_a, "Flip the value of the bit at position n")
            .def(
                    "__str__", [](const NBitValuesContainer& container) {
                        return container.stringify();
                    },
                    "Returns a string containing the stringified values of the stored bits.");

    py::enum_<Gate::Type>(m, "gate_type")
            .value("toffoli", Gate::Type::Toffoli, "Toffoli gate type.")
            .value("fredkin", Gate::Type::Fredkin, "Fredkin gate type.")
            .export_values();

    py::class_<Gate, std::shared_ptr<Gate>>(m, "gate")
            .def(py::init<>(), "Constructs gate object.")
            .def_readwrite("controls", &Gate::controls, "Controls of the gate.")
            .def_readwrite("targets", &Gate::targets, "Targets of the gate.")
            .def_readwrite("type", &Gate::type, "Type of the gate.");

    m.def("cost_aware_synthesis", &CostAwareSynthesis::synthesize, "circ"_a, "program"_a, "settings"_a = Properties::ptr(), "statistics"_a = Properties::ptr(), "Cost-aware synthesis of the SyReC program.");
    m.def("line_aware_synthesis", &LineAwareSynthesis::synthesize, "circ"_a, "program"_a, "settings"_a = Properties::ptr(), "statistics"_a = Properties::ptr(), "Line-aware synthesis of the SyReC program.");
    m.def("simple_simulation", &simpleSimulation, "output"_a, "circ"_a, "input"_a, "statistics"_a = Properties::ptr(), "Simulation of the synthesized circuit circ.");
}
