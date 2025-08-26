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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_shiftExpression.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

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

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
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
                            RightShiftWithShiftedExpressionEqualToShiftExpression);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
