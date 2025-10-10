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
#include "core/configurable_options.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_synthesis_settings_features.json";

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

// BEGIN of tests of synthesis settings features
TYPED_TEST_P(BaseSimulationTestFixture, OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesModuleWithMainIdentiferAsMainModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesLastDefinedModuleAsMainModuleIfNoModuleWithIdentifierMainExists) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesLastDefinedModuleAsMainModuleIfNoModuleWithIdentifierMatchingMainExactlyExists) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesLastDefinedModuleAsMainModuleIfNoModuleWithIdentifierMatchingMainInSameCasingExists) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsNotValidCausesError) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "2_main";

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module main(inout a(4)) ++= a";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsChoosesMatchingModuleInsteadOfModuleWithIdentifierMain) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "incr";

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsNotMatchingAnyModuleAndModuleWithIdentifierMainExistingCausesError) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "a";

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module decr(inout a(4)) --= a module sub(inout a(4), inout b(4)) a -= b module main(inout a(4), inout b(4)) call decr(a); call sub(a, b)";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsNotMatchingAnyModuleAndModuleWithIdentifierMainNotExistingCausesError) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "add";

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module decr(inout a(4)) --= a module sub(inout a(4), inout b(4)) a -= b";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsBeingEmptyCausesError) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "";

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module main(inout a(4)) ++= a";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsOnlyPartiallyMatchingModuleWithNoFullMatchFoundCausesError) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "add";

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module add_4(inout a(4), inout b(4)) a += b module twoQubit_add_2(inout a(2), inout b(2)) a += b module twoQubit_add(inout a(2), inout b(2)) a += b";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsOnlyPartiallyMatchingModuleWithFullMatchFoundSelectsLatterAsModuleModule) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "incr";

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsMatchingMultipleModulesCausesError) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "incr";

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module incr(inout a(1)) ++= a module incr(inout a(2)) ++= a.1 module incr(inout a(3)) ++= a.2";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedModuleIdentifierInSynthesisSettingsOnlyMatchingModulesWithSameIdentifierCharacterCasing) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.optionalProgramEntryPointModuleIdentifier = "INCR";

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentUsingBitwiseAndIntegerTruncationOperation) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.integerConstantTruncationOperation = utils::IntegerConstantTruncationOperation::BitwiseAnd;

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentUsingModuloIntegerTruncationOperation) {
    syrec::ConfigurableOptions synthesisSettings;
    synthesisSettings.integerConstantTruncationOperation = utils::IntegerConstantTruncationOperation::Modulo;

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBit) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitDefinedAsIntegerConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitrangeWithKnownBounds) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitrangeWithStartLargerThanEndAndStartDefinedAsIntegerConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitrangeWithStartSmallerThanEndAndEndDefinedAsIntegerConstantExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantInShiftExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantInUnaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantInLhsOperandOfBinaryExpression) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantInRhsOperandOfBinaryExpression) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalOperandModifiesExpectedBitwidthForIntegerTruncationInNestedExpressionOfBinaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfCompileTimeConstantExpressionInNestedExpressionOfBinaryExpression) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantInGuardConditionOfIfStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TruncationOfIntegerConstantInExpressionUsedInDimensionAccessOfVariableAccess) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, IntegerConstantTruncationOnlyPerformedAfterCompileTimeConstantExpressionWasEvaluatedNotDuringEvaluation) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, PartialSimplificationInNestedExpressionOfUnaryExpressionPerformedDueToIntegerConstantTruncation) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, PartialSimplificationInNestedExpressionOfBinaryExpressionPerformedDueToIntegerConstantTruncation) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, PartialSimplificationInNestedExpressionOfShiftExpressionPerformedDueToIntegerConstantTruncation) {
    if constexpr (BaseSimulationTestFixture<TypeParam>::isTestingLineAwareSynthesis()) {
        GTEST_SKIP() << "Test disabled due to issue #280 (incorrect line aware synthesis of assignments) that needs to be resolved before statements with a variable access using a non-compile time constant expression as index can be synthesized";
    } else {
        this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
    }
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesModuleWithMainIdentiferAsMainModule,
                            OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesLastDefinedModuleAsMainModuleIfNoModuleWithIdentifierMainExists,
                            OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesLastDefinedModuleAsMainModuleIfNoModuleWithIdentifierMatchingMainExactlyExists,
                            OmittingUserDefinedMainModuleIdentifierInSynthesisSettingsChoosesLastDefinedModuleAsMainModuleIfNoModuleWithIdentifierMatchingMainInSameCasingExists,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsNotValidCausesError,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsChoosesMatchingModuleInsteadOfModuleWithIdentifierMain,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsNotMatchingAnyModuleAndModuleWithIdentifierMainExistingCausesError,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsNotMatchingAnyModuleAndModuleWithIdentifierMainNotExistingCausesError,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsBeingEmptyCausesError,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsOnlyPartiallyMatchingModuleWithNoFullMatchFoundCausesError,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsOnlyPartiallyMatchingModuleWithFullMatchFoundSelectsLatterAsModuleModule,
                            UserDefinedMainModuleIdentifierInSynthesisSettingsMatchingMultipleModulesCausesError,
                            UserDefinedModuleIdentifierInSynthesisSettingsOnlyMatchingModulesWithSameIdentifierCharacterCasing,

                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentUsingBitwiseAndIntegerTruncationOperation,
                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentUsingModuloIntegerTruncationOperation,
                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBit,
                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitDefinedAsIntegerConstantExpression,
                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitrangeWithKnownBounds,
                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitrangeWithStartLargerThanEndAndStartDefinedAsIntegerConstantExpression,
                            TruncationOfIntegerConstantOnRightHandSideOfAssignmentToBitrangeWithStartSmallerThanEndAndEndDefinedAsIntegerConstantExpression,
                            TruncationOfIntegerConstantInShiftExpression,
                            TruncationOfIntegerConstantInUnaryExpression,
                            TruncationOfIntegerConstantInLhsOperandOfBinaryExpression,
                            TruncationOfIntegerConstantInRhsOperandOfBinaryExpression,
                            LogicalOperandModifiesExpectedBitwidthForIntegerTruncationInNestedExpressionOfBinaryExpression,
                            TruncationOfCompileTimeConstantExpressionInNestedExpressionOfBinaryExpression,
                            TruncationOfIntegerConstantInGuardConditionOfIfStatement,
                            TruncationOfIntegerConstantInExpressionUsedInDimensionAccessOfVariableAccess,
                            IntegerConstantTruncationOnlyPerformedAfterCompileTimeConstantExpressionWasEvaluatedNotDuringEvaluation,

                            PartialSimplificationInNestedExpressionOfUnaryExpressionPerformedDueToIntegerConstantTruncation,
                            PartialSimplificationInNestedExpressionOfBinaryExpressionPerformedDueToIntegerConstantTruncation,
                            PartialSimplificationInNestedExpressionOfShiftExpressionPerformedDueToIntegerConstantTruncation);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
