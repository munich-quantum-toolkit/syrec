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

class SyrecLineAwareSynthesisTest: public testing::TestWithParam<std::string> {
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
        std::ifstream i(testConfigsDir + "circuits_line_aware_synthesis.json");
        json          j         = json::parse(i);
        expectedNumGates        = j[synthesisParam]["num_gates"];
        expectedNumLines        = j[synthesisParam]["lines"];
        expectedQuantumCosts    = j[synthesisParam]["quantum_costs"];
        expectedTransistorCosts = j[synthesisParam]["transistor_costs"];
    }
};

INSTANTIATE_TEST_SUITE_P(SyrecSynthesisTest, SyrecLineAwareSynthesisTest,
                         testing::Values(
                                 "alu_2",
                                 "binary_numeric",
                                 "bitwise_and_2",
                                 "bitwise_or_2",
                                 "bn_2",
                                 "call_8",
                                 "constExpr_8",
                                 "divide_2",
                                 "for_4",
                                 "for_32",
                                 "gray_binary_conversion_16",
                                 "ifCondVariants_4",
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
                                 "relationalOp_4",
                                 "shift_4",
                                 "simple_add_2",
                                 "single_longstatement_4",
                                 "skip",
                                 "swap_2"),
                         [](const testing::TestParamInfo<SyrecLineAwareSynthesisTest::ParamType>& info) {
                             auto s = info.param;
                             std::replace( s.begin(), s.end(), '-', '_');
                             return s; });

TEST_P(SyrecLineAwareSynthesisTest, GenericSynthesisTest) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    Program                       prog;
    const ReadProgramSettings     settings;
    std::string                   errorString;
    ASSERT_NO_FATAL_FAILURE(errorString = prog.read(fileName, settings)) << "Unexpected crash during processing of SyReC program";
    ASSERT_TRUE(errorString.empty()) << "Found errors during processing of SyReC program: " << errorString;

    ASSERT_TRUE(LineAwareSynthesis::synthesize(annotatableQuantumComputation, prog));
    ASSERT_EQ(expectedNumGates, annotatableQuantumComputation.getNops());
    ASSERT_EQ(expectedNumLines, annotatableQuantumComputation.getNqubits());

    const AnnotatableQuantumComputation::SynthesisCostMetricValue actualQuantumCosts    = annotatableQuantumComputation.getQuantumCostForSynthesis();
    const AnnotatableQuantumComputation::SynthesisCostMetricValue actualTransistorCosts = annotatableQuantumComputation.getTransistorCostForSynthesis();
    ASSERT_EQ(expectedQuantumCosts, actualQuantumCosts);
    ASSERT_EQ(expectedTransistorCosts, actualTransistorCosts);
}

TEST_P(SyrecLineAwareSynthesisTest, GenericSynthesisQASMTest) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    Program                       prog;
    const ReadProgramSettings     settings;

    std::string errorString;
    ASSERT_NO_FATAL_FAILURE(errorString = prog.read(fileName, settings)) << "Unexpected crash during processing of SyReC program";
    ASSERT_TRUE(errorString.empty()) << "Found errors during processing of SyReC program: " << errorString;
    // We are not asserting that the synthesis completes successfully since the 'dump' of the circuit into the .qasm file might help debugging the error.
    ASSERT_TRUE(LineAwareSynthesis::synthesize(annotatableQuantumComputation, prog));

    const auto lastIndex      = fileName.find_last_of('.');
    const auto outputFileName = fileName.substr(0, lastIndex);
    ASSERT_NO_FATAL_FAILURE(annotatableQuantumComputation.dump(outputFileName));

    const std::ifstream outputFileStream(outputFileName);
    ASSERT_TRUE(outputFileStream.good());
}
