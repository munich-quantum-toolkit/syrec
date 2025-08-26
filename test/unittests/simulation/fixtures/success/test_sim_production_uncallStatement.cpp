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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_uncallStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInAsValueForParameterOfTypeInOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeOutAsValueForParameterOfTypeInOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeWireAsValueForParameterOfTypeInOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeStateAsValueForParameterOfTypeInOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInoutOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeOutAsValueForParameterOfTypeInoutOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeWireAsValueForParameterOfTypeInoutOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeInoutAsValueForParameterOfTypeOutOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeOutAsValueForParameterOfTypeOutOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfVariableOfTypeWireAsValueForParameterOfTypeOutOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfNDimensionalModuleParameterAsValueForParameterOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfNDimensionalLocalModuleVariableAsValueForParameterOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfAssignStatementUsingAddAssignOperationInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfAssignStatementUsingSubAssignOperationInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfAssignStatementUsingXorAssignOperationInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfCallStatementInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfForStatementWithStartSmallerThanEndInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfForStatementWithStartLargerThanEndInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfForStatementWithOnlyUpperBoundOfIterationRangeDefinedInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfIfStatementInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfSkipStatementInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfSwapStatementInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfUncallStatementInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfUnaryAssignStatementUsingIncrementOperationInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfUnaryAssignStatementUsingDecrementOperationInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfUnaryAssignStatementUsingBitwiseNegationOperationInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, InverseOfMultipleStatementsInUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfRepeatedUncallsOfSameModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SynthesisOfNestedModuleUncallHierarchy) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            UsageOfVariableOfTypeInAsValueForParameterOfTypeInOfUncalledModule,
                            UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInOfUncalledModule,
                            UsageOfVariableOfTypeOutAsValueForParameterOfTypeInOfUncalledModule,
                            UsageOfVariableOfTypeWireAsValueForParameterOfTypeInOfUncalledModule,
                            UsageOfVariableOfTypeStateAsValueForParameterOfTypeInOfUncalledModule,
                            UsageOfVariableOfTypeInoutAsValueForParameterOfTypeInoutOfUncalledModule,
                            UsageOfVariableOfTypeOutAsValueForParameterOfTypeInoutOfUncalledModule,
                            UsageOfVariableOfTypeWireAsValueForParameterOfTypeInoutOfUncalledModule,
                            UsageOfVariableOfTypeInoutAsValueForParameterOfTypeOutOfUncalledModule,
                            UsageOfVariableOfTypeOutAsValueForParameterOfTypeOutOfUncalledModule,
                            UsageOfVariableOfTypeWireAsValueForParameterOfTypeOutOfUncalledModule,
                            UsageOfNDimensionalModuleParameterAsValueForParameterOfUncalledModule,
                            UsageOfNDimensionalLocalModuleVariableAsValueForParameterOfUncalledModule,

                            InverseOfAssignStatementUsingAddAssignOperationInUncalledModule,
                            InverseOfAssignStatementUsingSubAssignOperationInUncalledModule,
                            InverseOfAssignStatementUsingXorAssignOperationInUncalledModule,
                            InverseOfCallStatementInUncalledModule,
                            InverseOfForStatementWithStartSmallerThanEndInUncalledModule,
                            InverseOfForStatementWithStartLargerThanEndInUncalledModule,
                            InverseOfForStatementWithOnlyUpperBoundOfIterationRangeDefinedInUncalledModule,
                            InverseOfIfStatementInUncalledModule,
                            InverseOfSkipStatementInUncalledModule,
                            InverseOfSwapStatementInUncalledModule,
                            InverseOfUncallStatementInUncalledModule,
                            InverseOfUnaryAssignStatementUsingIncrementOperationInUncalledModule,
                            InverseOfUnaryAssignStatementUsingDecrementOperationInUncalledModule,
                            InverseOfUnaryAssignStatementUsingBitwiseNegationOperationInUncalledModule,
                            InverseOfMultipleStatementsInUncalledModule,
                            SynthesisOfRepeatedUncallsOfSameModule,
                            SynthesisOfNestedModuleUncallHierarchy);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
