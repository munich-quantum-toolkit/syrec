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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_binaryExpression.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationAddition) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationSubtraction) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationXor) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationMultiplication) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationDivision) {
    // Since the expected values in case of a division by zero are dependent on the used synthesis algorithm, all test cases in which the divisor is 0 are omitted.
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationModulo) {
    // Since the expected values in case of a modulo operation in which the modulus is zero are dependent on the used synthesis algorithm, all test cases in which the modulus is 0 are omitted.
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationLogicalAnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationLogicalOr) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationBitwiseAnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationBitwiseOr) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationLessThan) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationGreaterThan) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationLessOrEqualThan) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationGreaterOrEqualThan) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationEquals) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationNotEquals) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithLhsOperandBeingIntegerConstant) {
    GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithLhsOperandBeingCompileTimeConstantExpression) {
    GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithLhsOperandBeingNestedExpression) {
    GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithLhsOperandBeingVariableExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithRhsOperandBeingIntegerConstant) {
    GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithRhsOperandBeingCompileTimeConstantExpression) {
    GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithRhsOperandBeingNestedExpression) {
    GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments)";
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithRhsOperandBeingVariableExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithBothOperandsBeingNestedExpressions) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithBothOperandsBeingCompileTimeConstantExpressions) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryExpressionWithBothOperandsBeingVariableAccesses) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            BinaryOperationAddition,
                            BinaryOperationSubtraction,
                            BinaryOperationXor,
                            BinaryOperationMultiplication,
                            BinaryOperationDivision,
                            BinaryOperationModulo,
                            BinaryOperationLogicalAnd,
                            BinaryOperationLogicalOr,
                            BinaryOperationBitwiseAnd,
                            BinaryOperationBitwiseOr,
                            BinaryOperationLessThan,
                            BinaryOperationLessOrEqualThan,
                            BinaryOperationGreaterThan,
                            BinaryOperationGreaterOrEqualThan,
                            BinaryOperationEquals,
                            BinaryOperationNotEquals,

                            BinaryExpressionWithLhsOperandBeingIntegerConstant,
                            BinaryExpressionWithLhsOperandBeingCompileTimeConstantExpression,
                            BinaryExpressionWithLhsOperandBeingNestedExpression,
                            BinaryExpressionWithLhsOperandBeingVariableExpression,
                            BinaryExpressionWithRhsOperandBeingIntegerConstant,
                            BinaryExpressionWithRhsOperandBeingCompileTimeConstantExpression,
                            BinaryExpressionWithRhsOperandBeingNestedExpression,
                            BinaryExpressionWithRhsOperandBeingVariableExpression,
                            BinaryExpressionWithBothOperandsBeingNestedExpressions,
                            BinaryExpressionWithBothOperandsBeingCompileTimeConstantExpressions,
                            BinaryExpressionWithBothOperandsBeingVariableAccesses);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
