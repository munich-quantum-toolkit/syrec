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

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_sim_production_forStatement.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

// Our current assumption is that the stringified input circuit is used as the test case data and processed by the parser prior to synthesis instead
// of the raw intermediate representation thus some test cases are prevented by the parser due to it adding defaults to not provided values (i.e. default values
// for the iteration range start value or step size in a ForStatement). Future synthesis tests using the intermediate representation as test case data should check
// the behaviour of the synthesis algorithms for the now omitted test cases.

TYPED_TEST_P(BaseSimulationTestFixture, NoLoopVariableAndOnlyIterationRangeStartValueDefinedInForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, NoLoopVariableAndOnlyIterationRangeStartValueAndStepSizeDefinedInForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, NoLoopVariableAndIterationRangeStartAndEndValueDefinedInForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, NoLoopVariableAndFullIterationRangeDefinedInForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, NoLoopVariableAndIterationRangeWithStartLargerThanEndAndOnlyStartAndEndValueDefined) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, NoLoopVariableAndFullIterationRangeDefinitionWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopVariableAndIterationRangeStartAndEndValueDefinedInForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopVariableAndFullIterationRangeDefinedInForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopVariableAndIterationRangeWithStartLargerThanEndAndOnlyStartAndEndValueDefined) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopVariableAndFullIterationRangeDefinitionWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfCompileTimeConstantExpressionInIterationRangeStartDefinitionOfForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfCompileTimeConstantExpressionInIterationRangeEndDefinitionOfForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfCompileTimeConstantExpressionInStepsizeDefinitionOfForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfLoopVariableOfCurrentForStatementInIterationRangeEndDefinition) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfLoopVariableOfCurrentForStatementInStepsizeDefinition) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfParentLoopVariableInIterationRangeStartDefinition) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfParentLoopVariableInIterationRangeEndDefinition) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UsageOfParentLoopVariableInStepSizeDefinition) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopIterationRangeWithStartEqualToEndAndNoStepsizeDefinitionPerformsNoIteration) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopIterationRangeWithStartEqualToEndAndStepsizeDefinitionPerformsNoIteration) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, LoopIterationRangeWithStartEqualToEndDefinedUsingNotEqualConstantExpressionsAndNoStepsizeDefinitionPerformsNoIteration) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, ReuseOfSameLoopVariableIdentifierInNotNestedLoop) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            NoLoopVariableAndOnlyIterationRangeStartValueDefinedInForStatement,
                            NoLoopVariableAndOnlyIterationRangeStartValueAndStepSizeDefinedInForStatement,
                            NoLoopVariableAndIterationRangeStartAndEndValueDefinedInForStatement,
                            NoLoopVariableAndFullIterationRangeDefinedInForStatement,
                            NoLoopVariableAndIterationRangeWithStartLargerThanEndAndOnlyStartAndEndValueDefined,
                            NoLoopVariableAndFullIterationRangeDefinitionWithStartLargerThanEnd,
                            LoopVariableAndIterationRangeStartAndEndValueDefinedInForStatement,
                            LoopVariableAndFullIterationRangeDefinedInForStatement,
                            LoopVariableAndIterationRangeWithStartLargerThanEndAndOnlyStartAndEndValueDefined,
                            LoopVariableAndFullIterationRangeDefinitionWithStartLargerThanEnd,
                            UsageOfCompileTimeConstantExpressionInIterationRangeStartDefinitionOfForStatement,
                            UsageOfCompileTimeConstantExpressionInIterationRangeEndDefinitionOfForStatement,
                            UsageOfCompileTimeConstantExpressionInStepsizeDefinitionOfForStatement,
                            UsageOfLoopVariableOfCurrentForStatementInIterationRangeEndDefinition,
                            UsageOfLoopVariableOfCurrentForStatementInStepsizeDefinition,
                            UsageOfParentLoopVariableInIterationRangeStartDefinition,
                            UsageOfParentLoopVariableInIterationRangeEndDefinition,
                            UsageOfParentLoopVariableInStepSizeDefinition,
                            LoopIterationRangeWithStartEqualToEndAndNoStepsizeDefinitionPerformsNoIteration,
                            LoopIterationRangeWithStartEqualToEndAndStepsizeDefinitionPerformsNoIteration,
                            LoopIterationRangeWithStartEqualToEndDefinedUsingNotEqualConstantExpressionsAndNoStepsizeDefinitionPerformsNoIteration,
                            ReuseOfSameLoopVariableIdentifierInNotNestedLoop);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
