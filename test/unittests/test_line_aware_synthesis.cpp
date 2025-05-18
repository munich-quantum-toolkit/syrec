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
#include "core/properties.hpp"
#include "core/syrec/program.hpp"
#include "ir/QuantumComputation.hpp"

#include "gtest/gtest.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

using namespace syrec;

class SyrecSynthesisTest: public testing::TestWithParam<std::string> {
protected:
    std::string  testConfigsDir  = "./configs/";
    std::string  testCircuitsDir = "./circuits/";
    std::string  fileName;
    Gate::cost_t qc               = 0;
    Gate::cost_t tc               = 0;
    unsigned     expectedNumGates = 0;
    unsigned     expectedLines    = 0;
    Gate::cost_t expectedQc       = 0;
    Gate::cost_t expectedTc       = 0;

    void SetUp() override {
        std::string synthesisParam = GetParam();
        fileName                   = testCircuitsDir + GetParam() + ".src";
        std::ifstream i(testConfigsDir + "circuits_line_aware_synthesis.json");
        json          j  = json::parse(i);
        expectedNumGates = j[synthesisParam]["num_gates"];
        expectedLines    = j[synthesisParam]["lines"];
        expectedQc       = j[synthesisParam]["quantum_costs"];
        expectedTc       = j[synthesisParam]["transistor_costs"];
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
    qc::QuantumComputation             quantumComputation;
    Program             prog;
    ReadProgramSettings settings;
    const std::string         errorString = prog.read(fileName, settings);
    EXPECT_TRUE(errorString.empty());

    EXPECT_TRUE(LineAwareSynthesis::synthesize(quantumComputation, prog));

    // TODO
    /*qc = quantumComputation.quantumCost();
    tc = quantumComputation.transistorCost();

    EXPECT_EQ(expectedNumGates, quantumComputation.numGates());
    EXPECT_EQ(expectedLines, quantumComputation.getLines());*/
    EXPECT_EQ(expectedQc, qc);
    EXPECT_EQ(expectedTc, tc);
}

TEST_P(SyrecSynthesisTest, GenericSynthesisQASMTest) {
    qc::QuantumComputation             quantumComputation;
    Program             prog;
    ReadProgramSettings settings;

    const auto errorString = prog.read(fileName, settings);
    EXPECT_TRUE(errorString.empty());
    EXPECT_TRUE(LineAwareSynthesis::synthesize(quantumComputation, prog));

    const auto lastIndex      = fileName.find_last_of('.');
    const auto outputFileName = fileName.substr(0, lastIndex);

    // TODO:
    const auto& dumpedQasmString = quantumComputation.toQASM();
    ASSERT_FALSE(dumpedQasmString.empty());
}
