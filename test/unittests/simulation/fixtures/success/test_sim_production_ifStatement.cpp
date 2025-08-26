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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_ifStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingWholeBitwidthOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingBitrangeOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToBinaryExpressionWithOperandsHavingBitwidthOfOne) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToBinaryExpressionWithOperandsHavingBitwidthLargerThanOne) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToIntegerConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToLoopVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfAssignStatementInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfUnaryAssignStatementInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfForStatementInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfCallStatementInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfUncallStatementInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfSwapStatementInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfMultipleStatementsInTrueBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfAssignStatementInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfUnaryAssignStatementInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfForStatementInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfCallStatementInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfUncallStatementInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfSwapStatementInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementExecutionOfMultipleStatementsInFalseBranchIfBranchIsExecuted) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingWholeBitwidthOfVariable,
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOf1DVariable,
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOfValueOfDimensionOfVariable,
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingBitrangeOf1DVariable,
                            IfStatementWithGuardConditionEqualToUnaryExpression,
                            IfStatementWithGuardConditionEqualToShiftExpression,
                            IfStatementWithGuardConditionEqualToBinaryExpressionWithOperandsHavingBitwidthOfOne,
                            IfStatementWithGuardConditionEqualToBinaryExpressionWithOperandsHavingBitwidthLargerThanOne,
                            IfStatementWithGuardConditionEqualToIntegerConstant,
                            IfStatementWithGuardConditionEqualToLoopVariable,
                            IfStatementWithGuardConditionEqualToConstantExpression,

                            IfStatementExecutionOfAssignStatementInTrueBranchIfBranchIsExecuted,
                            IfStatementExecutionOfUnaryAssignStatementInTrueBranchIfBranchIsExecuted,
                            IfStatementExecutionOfForStatementInTrueBranchIfBranchIsExecuted,
                            IfStatementExecutionOfCallStatementInTrueBranchIfBranchIsExecuted,
                            IfStatementExecutionOfUncallStatementInTrueBranchIfBranchIsExecuted,
                            IfStatementExecutionOfSwapStatementInTrueBranchIfBranchIsExecuted,
                            IfStatementExecutionOfMultipleStatementsInTrueBranchIfBranchIsExecuted,

                            IfStatementExecutionOfAssignStatementInFalseBranchIfBranchIsExecuted,
                            IfStatementExecutionOfUnaryAssignStatementInFalseBranchIfBranchIsExecuted,
                            IfStatementExecutionOfForStatementInFalseBranchIfBranchIsExecuted,
                            IfStatementExecutionOfCallStatementInFalseBranchIfBranchIsExecuted,
                            IfStatementExecutionOfUncallStatementInFalseBranchIfBranchIsExecuted,
                            IfStatementExecutionOfSwapStatementInFalseBranchIfBranchIsExecuted,
                            IfStatementExecutionOfMultipleStatementsInFalseBranchIfBranchIsExecuted);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
