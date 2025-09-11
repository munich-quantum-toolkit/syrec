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
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "base_simulation_test_fixture.hpp"

#include <gtest/gtest.h>
#include <string>

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_swapStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariableWithBitOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariableWithBitrangeOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariableWithValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithSameBitOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithOtherBitOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithOverlappingBitOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithBitOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOnLhsOfSwapStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOnRhsOfSwapStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOfBothOperandsOfSwapStatementAccessingFullBitwidth) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOfBothOperandsOfSwapStatementAccessingBit) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOfBothOperandsOfSwapStatementAccessingBitrange) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            SwapWithLeftOperationBeingAccessOn1DVariable,
                            SwapWithLeftOperationBeingAccessOn1DVariableWithBitOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOn1DVariableWithBitrangeOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOn1DVariableWithValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithSameBitOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithOtherBitOfOtherVariable,

                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithOverlappingBitOfValueOfOtherDimensionOfSameVariable,

                            SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartSmallerThanEnd,
                            SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartLargerThanEnd,
                            SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartEqualToEnd,
                            SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithBitOfValueOfDimensionOfOtherVariable,

                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable,

                            UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOnLhsOfSwapStatement,
                            UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOnRhsOfSwapStatement,
                            UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOfBothOperandsOfSwapStatementAccessingFullBitwidth,
                            UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOfBothOperandsOfSwapStatementAccessingBit,
                            UsageOfVariableAccessWithNonCompileTimeConstantExpressionInDimensionAccessOfBothOperandsOfSwapStatementAccessingBitrange);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
