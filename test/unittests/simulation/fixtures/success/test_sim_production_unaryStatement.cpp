/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_unaryStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, DecrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignOfBitOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignBitOfValueOfDimensionOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegateAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IncrementAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UnaryAssignmentToFullBitwidthWithDimensionAccessOfAssignedToVariableContainingNonCompileTimeConstantExpressions) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, UnaryAssignmentToBitWithDimensionAccessOfAssignedToVariableContainingNonCompileTimeConstantExpressions) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, UnaryAssignmentToBitrangeWithDimensionAccessOfAssignedToVariableContainingNonCompileTimeConstantExpressions) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}
REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            IncrementAssignOfVariable,
                            IncrementAssignOfBitOfVariable,
                            IncrementAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd,
                            IncrementAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd,
                            IncrementAssignValueOfDimensionOfVariable,
                            IncrementAssignBitOfValueOfDimensionOfVariable,
                            IncrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd,
                            IncrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd,
                            IncrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd,

                            DecrementAssignOfVariable,
                            DecrementAssignOfBitOfVariable,
                            DecrementAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd,
                            DecrementAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd,
                            DecrementAssignValueOfDimensionOfVariable,
                            DecrementAssignBitOfValueOfDimensionOfVariable,
                            DecrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd,
                            DecrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd,
                            DecrementAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd,

                            BitwiseNegateAssignOfVariable,
                            BitwiseNegateAssignOfBitOfVariable,
                            BitwiseNegateAssignOfBitrangeOfVariableWithBitrangeStartSmallerThanEnd,
                            BitwiseNegateAssignOfBitrangeOfVariableWithBitrangeStartLargerThanEnd,
                            BitwiseNegateAssignValueOfDimensionOfVariable,
                            BitwiseNegateAssignBitOfValueOfDimensionOfVariable,
                            BitwiseNegateAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartSmallerThanEnd,
                            BitwiseNegateAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartLargerThanEnd,
                            BitwiseNegateAssignBitrangeOfValueOfDimensionOfVariableWithBitrangeStartEqualToEnd,

                            UnaryAssignmentToFullBitwidthWithDimensionAccessOfAssignedToVariableContainingNonCompileTimeConstantExpressions,
                            UnaryAssignmentToBitWithDimensionAccessOfAssignedToVariableContainingNonCompileTimeConstantExpressions,
                            UnaryAssignmentToBitrangeWithDimensionAccessOfAssignedToVariableContainingNonCompileTimeConstantExpressions);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
