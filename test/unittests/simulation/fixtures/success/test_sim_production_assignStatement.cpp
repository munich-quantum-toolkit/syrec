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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_assignStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

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

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignWithRightHandSideEqualToVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignWithRightHandSideEqualToConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignWithRightHandSideEqualToShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignWithRightHandSideEqualToUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignWithRightHandSideEqualToNestedExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitrangeOfVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitrangeOfVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitrangeOfValueOfDimensionOfVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitrangeOfValueOfDimensionOfVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SubAssignOfBitrangeOfValueOfDimensionOfVariableWithStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignWithRightHandSideEqualToVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignWithRightHandSideEqualToConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignWithRightHandSideEqualToShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignWithRightHandSideEqualToUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignWithRightHandSideEqualToNestedExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitrangeOfVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitrangeOfVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitrangeOfValueOfDimensionOfVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitrangeOfValueOfDimensionOfVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, XorAssignOfBitrangeOfValueOfDimensionOfVariableWithStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnSameQubitOfGuardConditionPossibleOnLefthandSideOfAssignment) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, OverlappingAccessOnQubitOfGuardConditionPossibleOnLefthandSideOfAssignment) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnSameQubitOfGuardConditionPossibleOnRighthandSideOfAssignmentUsingPrefixAssignmentOperand) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, OverlappingAccessOnQubitOfGuardConditionPossibleOnRighthandSideOfAssignmentUsingPrefixAssignmentOperand) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
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

                            SubAssignWithRightHandSideEqualToVariable,
                            SubAssignWithRightHandSideEqualToConstant,
                            SubAssignWithRightHandSideEqualToShiftExpression,
                            SubAssignWithRightHandSideEqualToUnaryExpression,
                            SubAssignWithRightHandSideEqualToNestedExpression,
                            SubAssignOfBitOfVariable,
                            SubAssignOfBitrangeOfVariableWithStartSmallerThanEnd,
                            SubAssignOfBitrangeOfVariableWithStartLargerThanEnd,
                            SubAssignOfValueOfDimensionOfVariable,
                            SubAssignOfBitOfValueOfDimensionOfVariable,
                            SubAssignOfBitrangeOfValueOfDimensionOfVariableWithStartSmallerThanEnd,
                            SubAssignOfBitrangeOfValueOfDimensionOfVariableWithStartLargerThanEnd,
                            SubAssignOfBitrangeOfValueOfDimensionOfVariableWithStartEqualToEnd,
                            XorAssignWithRightHandSideEqualToVariable,
                            XorAssignWithRightHandSideEqualToConstant,
                            XorAssignWithRightHandSideEqualToShiftExpression,
                            XorAssignWithRightHandSideEqualToUnaryExpression,
                            XorAssignWithRightHandSideEqualToNestedExpression,
                            XorAssignOfBitOfVariable,
                            XorAssignOfBitrangeOfVariableWithStartSmallerThanEnd,
                            XorAssignOfBitrangeOfVariableWithStartLargerThanEnd,
                            XorAssignOfValueOfDimensionOfVariable,
                            XorAssignOfBitOfValueOfDimensionOfVariable,
                            XorAssignOfBitrangeOfValueOfDimensionOfVariableWithStartSmallerThanEnd,
                            XorAssignOfBitrangeOfValueOfDimensionOfVariableWithStartLargerThanEnd,
                            XorAssignOfBitrangeOfValueOfDimensionOfVariableWithStartEqualToEnd,

                            AccessOnSameQubitOfGuardConditionPossibleOnLefthandSideOfAssignment,
                            OverlappingAccessOnQubitOfGuardConditionPossibleOnLefthandSideOfAssignment,
                            AccessOnSameQubitOfGuardConditionPossibleOnRighthandSideOfAssignmentUsingPrefixAssignmentOperand,
                            OverlappingAccessOnQubitOfGuardConditionPossibleOnRighthandSideOfAssignmentUsingPrefixAssignmentOperand);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
