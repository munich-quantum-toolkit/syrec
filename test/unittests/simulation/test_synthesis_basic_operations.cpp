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

const std::string relativePathToTestCaseDataJsonFile = "./unittests/simulation/data/test_synthesis_of_basic_operations.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfConstantZero) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfConstantOne) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfNestedExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfConstant) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfBinaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(relativePathToTestCaseDataJsonFile, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            LogicalNegationOfConstantZero,
                            LogicalNegationOfConstantOne,
                            LogicalNegationOfNestedExpression,
                            LogicalNegationOfUnaryExpression,
                            LogicalNegationOfVariable,
                            BitwiseNegationOfConstant,
                            BitwiseNegationOfVariable,
                            BitwiseNegationOfBinaryExpression,
                            BitwiseNegationOfShiftExpression,
                            BitwiseNegationOfUnaryExpression);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
