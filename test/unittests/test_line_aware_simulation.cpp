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
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
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

    NBitValuesContainer inputState(annotatableQuantumComputation.getNqubits());
    NBitValuesContainer outputState;

    for (const auto setLine: setLines) {
        ASSERT_TRUE(setLine >= 0);
        ASSERT_LT(static_cast<std::size_t>(setLine), inputState.size());
        inputState.set(static_cast<std::size_t>(setLine));
    }

    ASSERT_NO_FATAL_FAILURE(syrec::simpleSimulation(outputState, annotatableQuantumComputation, inputState));
    for (std::size_t i = 0; i < inputState.size(); ++i) {
        const char actualStringifiedOutputStateValue   = outputState[i] ? '1' : '0';
        const char expectedStringifiedOutputStateValue = expectedSimOut[i];
        ASSERT_EQ(expectedStringifiedOutputStateValue, actualStringifiedOutputStateValue) << "Mismatch of output qubit values at qubit " << std::to_string(i) << " | Expected: " << expectedStringifiedOutputStateValue << " Actual: " << actualStringifiedOutputStateValue;
    }
}
