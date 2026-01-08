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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_callStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInAsValueForParameterOfTypeInOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeOutAsValueForParameterOfTypeInOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeWireAsValueForParameterOfTypeInOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeStateAsValueForParameterOfTypeInOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInoutOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeOutAsValueForParameterOfTypeInoutOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeWireAsValueForParameterOfTypeInoutOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInoutAsValueForParameterOfTypeOutOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeOutAsValueForParameterOfTypeOutOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeWireAsValueForParameterOfTypeOutOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfNDimensionalModuleParameterAsValueForParameterOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfNDimensionalLocalModuleVariableAsValueForParameterOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfAssignStatementUsingAddAssignOperationInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfAssignStatementUsingSubAssignOperationInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfAssignStatementUsingXorAssignOperationInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfCallStatementInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfForStatementWithStartSmallerThanEndInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfForStatementWithStartLargerThanEndInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfForStatementWithOnlyUpperBoundOfIterationRangeDefinedInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfIfStatementInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfSkipStatementInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfSwapStatementInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfUncallStatementInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfUnaryAssignStatementUsingIncrementOperationInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfUnaryAssignStatementUsingDecrementOperationInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfUnaryAssignStatementUsingBitwiseNegationOperationInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfMultipleStatementsInCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfRepeatedCallsOfSameModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfNestedModuleCallHierarchy) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            UsageOfVariableOfTypeInAsValueForParameterOfTypeInOfCalledModule,
                            UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInOfCalledModule,
                            UsageOfVariableOfTypeOutAsValueForParameterOfTypeInOfCalledModule,
                            UsageOfVariableOfTypeWireAsValueForParameterOfTypeInOfCalledModule,
                            UsageOfVariableOfTypeStateAsValueForParameterOfTypeInOfCalledModule,
                            UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInoutOfCalledModule,
                            UsageOfVariableOfTypeOutAsValueForParameterOfTypeInoutOfCalledModule,
                            UsageOfVariableOfTypeWireAsValueForParameterOfTypeInoutOfCalledModule,
                            UsageOfVariableOfTypeInoutAsValueForParameterOfTypeOutOfCalledModule,
                            UsageOfVariableOfTypeOutAsValueForParameterOfTypeOutOfCalledModule,
                            UsageOfVariableOfTypeWireAsValueForParameterOfTypeOutOfCalledModule,
                            UsageOfNDimensionalModuleParameterAsValueForParameterOfCalledModule,
                            UsageOfNDimensionalLocalModuleVariableAsValueForParameterOfCalledModule,

                            SynthesisOfAssignStatementUsingAddAssignOperationInCalledModule,
                            SynthesisOfAssignStatementUsingSubAssignOperationInCalledModule,
                            SynthesisOfAssignStatementUsingXorAssignOperationInCalledModule,
                            SynthesisOfCallStatementInCalledModule,
                            SynthesisOfForStatementWithStartSmallerThanEndInCalledModule,
                            SynthesisOfForStatementWithStartLargerThanEndInCalledModule,
                            SynthesisOfForStatementWithOnlyUpperBoundOfIterationRangeDefinedInCalledModule,
                            SynthesisOfIfStatementInCalledModule,
                            SynthesisOfSkipStatementInCalledModule,
                            SynthesisOfSwapStatementInCalledModule,
                            SynthesisOfUncallStatementInCalledModule,
                            SynthesisOfUnaryAssignStatementUsingIncrementOperationInCalledModule,
                            SynthesisOfUnaryAssignStatementUsingDecrementOperationInCalledModule,
                            SynthesisOfUnaryAssignStatementUsingBitwiseNegationOperationInCalledModule,
                            SynthesisOfMultipleStatementsInCalledModule,
                            SynthesisOfRepeatedCallsOfSameModule,
                            SynthesisOfNestedModuleCallHierarchy);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
