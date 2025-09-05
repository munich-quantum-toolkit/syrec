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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_variableExpression.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitUsingConstantAsIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitUsingLoopVariableAsIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitUsingConstantExpressionAsIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable) {
    GTEST_SKIP() << "Test disable due to truncation of integer constants not being performed in synthesis leading to different operand bitwidths (see issue #295)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable) {
    GTEST_SKIP() << "Test disable due to truncation of integer constants not being performed in synthesis leading to different operand bitwidths (see issue #295)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingConstantAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingLoopVariableAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingConstantExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingVariableAccessAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingBinaryExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingShiftExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingUnaryExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DimensionAccessDefiningMultipleNonCompileTimeConstantExpressionsAsIndex) {
    GTEST_SKIP();
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IndexExpressionInDimensionAccessContainingVariableAccessWithNonCompileTimeConstantExpressionIndex) {
    GTEST_SKIP();
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnEveryElementOfVariableUsingNonCompileTimeConstantExpressionInDimensionAccess) {
    GTEST_SKIP();
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitInNonCompileTimeConstantExpressionInDimensionAccessOfVariableAccess) {
    GTEST_SKIP();
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeInNonCompileTimeConstantExpressionInDimensionAccessOfVariableAccess) {
    GTEST_SKIP();
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, CombinationOfDimensionAndBitAccess) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, CombinationOfDimensionAndBitrangeAccess) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            AccessOnBitUsingConstantAsIndexOf1DVariable,
                            AccessOnBitUsingLoopVariableAsIndexOf1DVariable,
                            AccessOnBitUsingConstantExpressionAsIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnValueOfDimensionUsingConstantAsIndex,
                            AccessOnValueOfDimensionUsingLoopVariableAsIndex,
                            AccessOnValueOfDimensionUsingConstantExpressionAsIndex,
                            AccessOnValueOfDimensionUsingVariableAccessAsIndex,
                            AccessOnValueOfDimensionUsingBinaryExpressionAsIndex,
                            AccessOnValueOfDimensionUsingShiftExpressionAsIndex,
                            AccessOnValueOfDimensionUsingUnaryExpressionAsIndex,
                            DimensionAccessDefiningMultipleNonCompileTimeConstantExpressionsAsIndex,
                            IndexExpressionInDimensionAccessContainingVariableAccessWithNonCompileTimeConstantExpressionIndex,
                            AccessOnEveryElementOfVariableUsingNonCompileTimeConstantExpressionInDimensionAccess,
                            AccessOnBitInNonCompileTimeConstantExpressionInDimensionAccessOfVariableAccess,
                            AccessOnBitrangeInNonCompileTimeConstantExpressionInDimensionAccessOfVariableAccess,
                            CombinationOfDimensionAndBitAccess,
                            CombinationOfDimensionAndBitrangeAccess);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
