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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_synthesis_of_basic_operations.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

// BEGIN of tests for production UnaryExpression
TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfConstantZero) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfConstantOne) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfNestedExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfBinaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}
// END of tests for production UnaryExpression

// BEGIN of tests for production BinaryExpression
TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationDivision) {
    // Since the expected values in case of a division by zero are dependent on the used synthesis algorithm, all test cases in which the divisor is 0 are omitted.
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BinaryOperationModulo) {
    // Since the expected values in case of a modulo operation in which the modulus is zero are dependent on the used synthesis algorithm, all test cases in which the modulus is 0 are omitted.
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}
// END of tests for production BinaryExpression

// BEGIN of tests for production ShiftExpression
TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToIntegerConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToLoopVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToIntegerConstantZero) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEvaluatingDuringSynthesisToConstantZero) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToIntegerConstantEqualToShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueEqualToShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToIntegerConstantLargerThanShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueLargerThanShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftedExpressionEqualToIntegerConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftedExpressionEqualToConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftedExpressionEqualToVariableAccess) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftedExpressionEqualToBinaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftedExpressionEqualToUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LeftShiftWithShiftedExpressionEqualToShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToIntegerConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToLoopVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToIntegerConstantZero) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEvaluatingDuringSynthesisToConstantZero) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToIntegerConstantEqualToShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueEqualToShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToIntegerConstantLargerThanShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueLargerThanShiftedExpressionBitwidth) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftedExpressionEqualToIntegerConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftedExpressionEqualToConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftedExpressionEqualToVariableAccess) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftedExpressionEqualToBinaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftedExpressionEqualToUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, RightShiftWithShiftedExpressionEqualToShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}
// END of tests for production ShiftExpression

// BEGIN of tests for production AssignStatement
TYPED_TEST_P(BaseSimulationTestFixture, AddAssignWithRightHandSideEqualToVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignWithRightHandSideEqualToConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignWithRightHandSideEqualToShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignWithRightHandSideEqualToUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignWithRightHandSideEqualToNestedExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitrangeOfVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitrangeOfVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitrangeOfValueOfDimensionOfVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitrangeOfValueOfDimensionOfVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AddAssignOfBitrangeOfValueOfDimensionOfVariableWithStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            // BEGIN of tests for production UnaryExpression
                            LogicalNegationOfConstantZero,
                            LogicalNegationOfConstantOne,
                            LogicalNegationOfNestedExpression,
                            LogicalNegationOfUnaryExpression,
                            LogicalNegationOfVariable,
                            BitwiseNegationOfConstant,
                            BitwiseNegationOfVariable,
                            BitwiseNegationOfBinaryExpression,
                            BitwiseNegationOfShiftExpression,
                            BitwiseNegationOfUnaryExpression,
                            // END of tests for production UnaryExpression

                            // BEGIN of tests for production BinaryExpression
                            BinaryOperationDivision,
                            BinaryOperationModulo,
                            // END of tests for production BinaryExpression

                            // BEGIN of tests for production ShiftExpression
                            LeftShiftWithShiftAmountEqualToIntegerConstant,
                            LeftShiftWithShiftAmountEqualToLoopVariable,
                            LeftShiftWithShiftAmountEqualToConstantExpression,
                            LeftShiftWithShiftAmountEqualToIntegerConstantZero,
                            LeftShiftWithShiftAmountEvaluatingDuringSynthesisToConstantZero,
                            LeftShiftWithShiftAmountEqualToIntegerConstantEqualToShiftedExpressionBitwidth,
                            LeftShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueEqualToShiftedExpressionBitwidth,
                            LeftShiftWithShiftAmountEqualToIntegerConstantLargerThanShiftedExpressionBitwidth,
                            LeftShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueLargerThanShiftedExpressionBitwidth,
                            LeftShiftWithShiftedExpressionEqualToIntegerConstant,
                            LeftShiftWithShiftedExpressionEqualToConstantExpression,
                            LeftShiftWithShiftedExpressionEqualToVariableAccess,
                            LeftShiftWithShiftedExpressionEqualToBinaryExpression,
                            LeftShiftWithShiftedExpressionEqualToUnaryExpression,
                            LeftShiftWithShiftedExpressionEqualToShiftExpression,

                            RightShiftWithShiftAmountEqualToIntegerConstant,
                            RightShiftWithShiftAmountEqualToLoopVariable,
                            RightShiftWithShiftAmountEqualToConstantExpression,
                            RightShiftWithShiftAmountEqualToIntegerConstantZero,
                            RightShiftWithShiftAmountEvaluatingDuringSynthesisToConstantZero,
                            RightShiftWithShiftAmountEqualToIntegerConstantEqualToShiftedExpressionBitwidth,
                            RightShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueEqualToShiftedExpressionBitwidth,
                            RightShiftWithShiftAmountEqualToIntegerConstantLargerThanShiftedExpressionBitwidth,
                            RightShiftWithShiftAmountEqualToIntegerConstantEvaluatingDuringSynthesisToValueLargerThanShiftedExpressionBitwidth,
                            RightShiftWithShiftedExpressionEqualToIntegerConstant,
                            RightShiftWithShiftedExpressionEqualToConstantExpression,
                            RightShiftWithShiftedExpressionEqualToVariableAccess,
                            RightShiftWithShiftedExpressionEqualToBinaryExpression,
                            RightShiftWithShiftedExpressionEqualToUnaryExpression,
                            RightShiftWithShiftedExpressionEqualToShiftExpression,
                            // END of tests for production ShiftExpression

                            // BEGIN of tests for production AssignStatement
                            AddAssignWithRightHandSideEqualToVariable,
                            AddAssignWithRightHandSideEqualToConstant,
                            AddAssignWithRightHandSideEqualToShiftExpression,
                            AddAssignWithRightHandSideEqualToUnaryExpression,
                            AddAssignWithRightHandSideEqualToNestedExpression,
                            AddAssignOfBitOfVariable,
                            AddAssignOfBitrangeOfVariableWithStartSmallerThanEnd,
                            AddAssignOfBitrangeOfVariableWithStartLargerThanEnd,
                            AddAssignOfValueOfDimensionOfVariable,
                            AddAssignOfBitOfValueOfDimensionOfVariable,
                            AddAssignOfBitrangeOfValueOfDimensionOfVariableWithStartSmallerThanEnd,
                            AddAssignOfBitrangeOfValueOfDimensionOfVariableWithStartLargerThanEnd,
                            AddAssignOfBitrangeOfValueOfDimensionOfVariableWithStartEqualToEnd,
                            IncrementAssignOfVariable,
                            IncrementAssignOfBitOfVariable,
                            IncrementAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd,
                            IncrementAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd,
                            IncrementValueOfDimensionOfVariable,
                            IncrementBitOfValueOfDimensionOfVariable,
                            IncrementBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd,
                            IncrementBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd,
                            IncrementBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd
                            // END of tests for production AssignStatement
);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
