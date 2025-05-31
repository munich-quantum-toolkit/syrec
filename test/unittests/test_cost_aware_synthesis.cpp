/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/syrec/program.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

// .clang-tidy reports a false positive here since we are including the required nlohman json header file
using json = nlohmann::json; // NOLINT(misc-include-cleaner)

using namespace syrec;

class SyrecCostAwareSynthesisTest: public testing::TestWithParam<std::string> {
protected:
    std::string                                             testConfigsDir  = "./configs/";
    std::string                                             testCircuitsDir = "./circuits/";
    std::string                                             fileName;
    std::size_t                                             expectedNumGates        = 0;
    std::size_t                                             expectedNumLines        = 0;
    AnnotatableQuantumComputation::SynthesisCostMetricValue expectedQuantumCosts    = 0;
    AnnotatableQuantumComputation::SynthesisCostMetricValue expectedTransistorCosts = 0;

    void SetUp() override {
        const std::string& synthesisParam = GetParam();
        fileName                          = testCircuitsDir + GetParam() + ".src";
        std::ifstream i(testConfigsDir + "circuits_cost_aware_synthesis.json");
        json          j         = json::parse(i);
        expectedNumGates        = j[synthesisParam]["num_gates"];
        expectedNumLines        = j[synthesisParam]["lines"];
        expectedQuantumCosts    = j[synthesisParam]["quantum_costs"];
        expectedTransistorCosts = j[synthesisParam]["transistor_costs"];
    }
};

INSTANTIATE_TEST_SUITE_P(SyrecSynthesisTest, SyrecCostAwareSynthesisTest,
                         testing::Values(
                                 "alu_2",
                                 "binary_numeric",
                                 "bitwise_and_2",
                                 "bitwise_or_2",
                                 "bn_2",
                                 "call_8",
                                 "divide_2",
                                 "for_4",
                                 "for_32",
                                 "gray_binary_conversion_16",
                                 "input_repeated_2",
                                 "input_repeated_4",
                                 "logical_and_1",
                                 "logical_or_1",
                                 "modulo_2",
                                 "multiply_2",
                                 "negate_8",
                                 "numeric_2",
                                 "operators_repeated_4",
                                 "parity_4",
                                 "parity_check_16",
                                 "shift_4",
                                 "simple_add_2",
                                 "single_longstatement_4",
                                 "skip",
                                 "swap_2"),
                         [](const testing::TestParamInfo<SyrecCostAwareSynthesisTest::ParamType>& info) {
                             auto s = info.param;
                             std::replace( s.begin(), s.end(), '-', '_');
                             return s; });

TEST_P(SyrecCostAwareSynthesisTest, GenericSynthesisTest) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    Program                       prog;
    const ReadProgramSettings     settings;
    const std::string             errorString = prog.read(fileName, settings);
    ASSERT_TRUE(errorString.empty());

    ASSERT_TRUE(CostAwareSynthesis::synthesize(annotatableQuantumComputation, prog));
    ASSERT_EQ(expectedNumGates, annotatableQuantumComputation.getNops());
    ASSERT_EQ(expectedNumLines, annotatableQuantumComputation.getNqubits());

    const AnnotatableQuantumComputation::SynthesisCostMetricValue actualQuantumCosts    = annotatableQuantumComputation.getQuantumCostForSynthesis();
    const AnnotatableQuantumComputation::SynthesisCostMetricValue actualTransistorCosts = annotatableQuantumComputation.getTransistorCostForSynthesis();
    ASSERT_EQ(expectedQuantumCosts, actualQuantumCosts);
    ASSERT_EQ(expectedTransistorCosts, actualTransistorCosts);
}
