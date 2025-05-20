/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/circuit.hpp"
#include "core/syrec/program.hpp"
#include "ir/QuantumComputation.hpp"
#include "quantum_computation_synthesis_cost_metrics.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// .clang-tidy reports a false positive here since we are including the required nlohman json header file
using json = nlohmann::json; // NOLINT(misc-include-cleaner)

using namespace syrec;

class SyrecSynthesisTest: public testing::TestWithParam<std::string> {
protected:
    std::string                                             testConfigsDir  = "./configs/";
    std::string                                             testCircuitsDir = "./circuits/";
    std::string                                             fileName;
    std::size_t                                             expectedNumGates        = 0;
    std::size_t                                             expectedNumLines        = 0;
    quantumComputationSynthesisCostMetrics::CostMetricValue expectedQuantumCosts    = 0;
    quantumComputationSynthesisCostMetrics::CostMetricValue expectedTransistorCosts = 0;

    void SetUp() override {
        const std::string& synthesisParam = GetParam();
        fileName                          = testCircuitsDir + GetParam() + ".src";
        std::ifstream i(testConfigsDir + "circuits_line_aware_synthesis.json");
        json          j         = json::parse(i);
        expectedNumGates        = j[synthesisParam]["num_gates"];
        expectedNumLines        = j[synthesisParam]["lines"];
        expectedQuantumCosts    = j[synthesisParam]["quantum_costs"];
        expectedTransistorCosts = j[synthesisParam]["transistor_costs"];
    }
};

INSTANTIATE_TEST_SUITE_P(SyrecSynthesisTest, SyrecSynthesisTest,
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
                         [](const testing::TestParamInfo<SyrecSynthesisTest::ParamType>& info) {
                             auto s = info.param;
                             std::replace( s.begin(), s.end(), '-', '_');
                             return s; });

TEST_P(SyrecSynthesisTest, GenericSynthesisTest) {
    qc::QuantumComputation quantumComputation;
    Program                prog;
    ReadProgramSettings    settings;
    const std::string      errorString = prog.read(fileName, settings);
    ASSERT_TRUE(errorString.empty());

    ASSERT_TRUE(LineAwareSynthesis::synthesize(quantumComputation, prog));
    ASSERT_EQ(expectedNumGates, quantumComputation.getNops());
    ASSERT_EQ(expectedNumLines, quantumComputation.getNqubits());

    const quantumComputationSynthesisCostMetrics::CostMetricValue actualQuantumCosts    = quantumComputationSynthesisCostMetrics::quantumCost(quantumComputation);
    const quantumComputationSynthesisCostMetrics::CostMetricValue actualTransistorCosts = quantumComputationSynthesisCostMetrics::transistorCost(quantumComputation);
    ASSERT_EQ(expectedQuantumCosts, actualQuantumCosts);
    ASSERT_EQ(expectedTransistorCosts, actualTransistorCosts);
}

TEST_P(SyrecSynthesisTest, GenericSynthesisQASMTest) {
    qc::QuantumComputation quantumComputation;
    Program                prog;
    ReadProgramSettings    settings;

    const auto errorString = prog.read(fileName, settings);
    ASSERT_TRUE(errorString.empty());
    ASSERT_TRUE(LineAwareSynthesis::synthesize(quantumComputation, prog));

    const auto lastIndex      = fileName.find_last_of('.');
    const auto outputFileName = fileName.substr(0, lastIndex);
    ASSERT_NO_FATAL_FAILURE(quantumComputation.dump(outputFileName));
}
