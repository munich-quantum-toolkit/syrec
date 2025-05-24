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
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

// .clang-tidy reports a false positive here since we are including the required nlohman json header file
using json = nlohmann::json; // NOLINT(misc-include-cleaner)

using namespace syrec;

class SyrecLineAwareSimulationTest: public testing::TestWithParam<std::string> {
protected:
    std::string      testConfigsDir  = "./configs/";
    std::string      testCircuitsDir = "./circuits/";
    std::string      fileName;
    std::vector<int> setLines;
    std::string      expectedSimOut;
    std::string      outputString;

    void SetUp() override {
        const std::string& synthesisParam = GetParam();
        fileName                          = testCircuitsDir + GetParam() + ".src";
        std::ifstream i(testConfigsDir + "circuits_line_aware_simulation.json");
        json          j = json::parse(i);
        expectedSimOut  = j[synthesisParam]["sim_out"];
        setLines        = j[synthesisParam]["set_lines"].get<std::vector<int>>();
    }
};

INSTANTIATE_TEST_SUITE_P(SyrecSimulationTest, SyrecLineAwareSimulationTest,
                         testing::Values(
                                 "alu_2",
                                 "swap_2",
                                 "simple_add_2",
                                 "multiply_2",
                                 "modulo_2",
                                 "negate_8"),
                         [](const testing::TestParamInfo<SyrecLineAwareSimulationTest::ParamType>& info) {
                             auto s = info.param;
                             std::replace( s.begin(), s.end(), '-', '_');
                             return s; });

TEST_P(SyrecLineAwareSimulationTest, GenericSimulationTest) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    Program                       prog;
    const ReadProgramSettings     settings;
    const Properties::ptr         statistics;
    const std::string             errorString = prog.read(fileName, settings);
    ASSERT_TRUE(errorString.empty());
    ASSERT_TRUE(LineAwareSynthesis::synthesize(annotatableQuantumComputation, prog));

    const std::size_t nInputQubits = annotatableQuantumComputation.getNqubitsWithoutAncillae();
    ASSERT_TRUE(setLines.size() < nInputQubits);

    std::vector initialQuantumComputationInputValues(nInputQubits, false);
    for (const auto setLine: setLines) {
        ASSERT_TRUE(setLine >= 0);
        initialQuantumComputationInputValues[static_cast<std::size_t>(setLine)] = true;
    }

    std::vector<bool> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(simulateQuantumComputationExecutionForState(annotatableQuantumComputation, initialQuantumComputationInputValues, quantumComputationOutputQubitValues, statistics));

    // Sometimes the full expected simulation output is defined in the .json file but we are only interested in the values of the non-ancillary qubits (whos qubit index is assumed to be larger than the one of the input qubits)
    const std::string_view& expectedOutputStateExcludingAncillaryQubits = std::string_view(expectedSimOut).substr(0, nInputQubits); // NOLINT (google-readability-casting)
    ASSERT_EQ(expectedOutputStateExcludingAncillaryQubits.size(), quantumComputationOutputQubitValues.size()) << "Expected output state to contain " << std::to_string(expectedOutputStateExcludingAncillaryQubits.size()) << " qubits but after simulation had " << quantumComputationOutputQubitValues.size() << " qubits";
    for (std::size_t i = 0; i < quantumComputationOutputQubitValues.size(); ++i) {
        // We are not interested in the value of garbage qubits
        if (annotatableQuantumComputation.getGarbage()[i]) {
            continue;
        }

        const char actualStringifiedOutputStateValue   = quantumComputationOutputQubitValues[i] ? '1' : '0';
        const char expectedStringifiedOutputStateValue = expectedOutputStateExcludingAncillaryQubits[i];
        ASSERT_EQ(expectedStringifiedOutputStateValue, actualStringifiedOutputStateValue) << "Missmatch of output qubit values at qubit " << std::to_string(i) << " | Expected: " << expectedStringifiedOutputStateValue << " Actual: " << actualStringifiedOutputStateValue;
    }
}
