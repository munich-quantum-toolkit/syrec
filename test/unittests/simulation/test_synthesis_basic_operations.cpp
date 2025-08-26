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
#include "algorithms/synthesis/syrec_synthesis.hpp"
#include "base_simulation_test_fixture.hpp"
#include "core/properties.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>

const std::string RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE = "./unittests/simulation/data/test_synthesis_of_basic_operations.json";

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
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "2_main");

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module main(inout a(4)) ++= a";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsChoosesMatchingModuleInsteadOfModuleWithIdentifierMain) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "incr");

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsNotMatchingAnyModuleAndModuleWithIdentifierMainExistingCausesError) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "a");

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module decr(inout a(4)) --= a module sub(inout a(4), inout b(4)) a -= b module main(inout a(4), inout b(4)) call decr(a); call sub(a, b)";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsNotMatchingAnyModuleAndModuleWithIdentifierMainNotExistingCausesError) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "add");

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module decr(inout a(4)) --= a module sub(inout a(4), inout b(4)) a -= b";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsBeingEmptyCausesError) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "");

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module main(inout a(4)) ++= a";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsOnlyPartiallyMatchingModuleWithNoFullMatchFoundCausesError) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "add");

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module add_4(inout a(4), inout b(4)) a += b module twoQubit_add_2(inout a(2), inout b(2)) a += b module twoQubit_add(inout a(2), inout b(2)) a += b";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsOnlyPartiallyMatchingModuleWithFullMatchFoundSelectsLatterAsModuleModule) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "incr");

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedMainModuleIdentifierInSynthesisSettingsMatchingMultipleModulesCausesError) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "incr");

    constexpr std::string_view stringifiedCircuitToParseAndSynthesis = "module incr(inout a(1)) ++= a module incr(inout a(2)) ++= a.1 module incr(inout a(3)) ++= a.2";
    this->performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(stringifiedCircuitToParseAndSynthesis, synthesisSettings);
}

TYPED_TEST_P(BaseSimulationTestFixture, UserDefinedModuleIdentifierInSynthesisSettingsOnlyMatchingModulesWithSameIdentifierCharacterCasing) {
    auto synthesisSettings = std::make_shared<syrec::Properties>();
    synthesisSettings->set<std::string>(syrec::SyrecSynthesis::MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "INCR");

    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest(), synthesisSettings);
}
// END of tests of synthesis settings features

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

// BEGIN of tests for production VariableExpression
TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitUsingConstantAsIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitUsingLoopVariableAsIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitUsingConstantExpressionAsIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingConstantAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingLoopVariableAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingConstantExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingVariableAccessAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingBinaryExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingShiftExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, AccessOnValueOfDimensionUsingUnaryExpressionAsIndex) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, CombinationOfDimensionAndBitAccess) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, CombinationOfDimensionAndBitrangeAccess) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}
// END of tests for production VariableExpression

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

// BEGIN of tests for production CallStatement
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
// END of tests for production CallStatement

// BEGIN of tests for production IfStatement
TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingWholeBitwidthOfVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOf1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOfValueOfDimensionOfVariable) {
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
// END of tests for production ForStatement

// BEGIN of tests for production ForStatement

// END of tests for production ForStatement

// BEGIN of tests for production SkipStatement
TYPED_TEST_P(BaseSimulationTestFixture, SkipStatementInTrueBranchOfIfStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SkipStatementInFalseBranchOfIfStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SkipStatementInLoopBodyOfForStatement) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SkipStatementInModuleBody) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SkipStatementInModuleBodyOfCalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SkipStatementInModuleBodyOfUncalledModule) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}
// END of tests for production SkipStatement

// BEGIN of tests for production SwapStatement
TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariableWithBitOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariableWithBitrangeOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOn1DVariableWithValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithSameBitOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithOtherBitOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithOverlappingBitOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartSmallerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartLargerThanEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartEqualToEnd) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithBitOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable) {
    this->performTestExecutionForCircuitLoadedFromJson(RELATIVE_PATH_TO_TEST_CASE_DATA_JSON_FILE, this->getNameOfCurrentlyExecutedTest());
}
// END of tests for production SwapStatement

// BEGIN of tests for production UncallStatement
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
// END of tests for production UncallStatement

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            // BEGIN of tests of synthesis settings features
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
                            // END of tests of synthesis settings features

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

                            // BEGIN of tests for production VariableExpression
                            AccessOnBitUsingConstantAsIndexOf1DVariable,
                            AccessOnBitUsingLoopVariableAsIndexOf1DVariable,
                            AccessOnBitUsingConstantExpressionAsIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingLoopVariableAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndLoopVariableAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnBitrangeUsingConstantExpressionAsIndexOfBitrangeStartAndConstantExpressionAsIndexOfBitrangeEndIndexOf1DVariable,
                            AccessOnValueOfDimensionUsingConstantAsIndex,
                            AccessOnValueOfDimensionUsingLoopVariableAsIndex,
                            AccessOnValueOfDimensionUsingConstantExpressionAsIndex,
                            AccessOnValueOfDimensionUsingVariableAccessAsIndex,
                            AccessOnValueOfDimensionUsingBinaryExpressionAsIndex,
                            AccessOnValueOfDimensionUsingShiftExpressionAsIndex,
                            AccessOnValueOfDimensionUsingUnaryExpressionAsIndex,
                            CombinationOfDimensionAndBitAccess,
                            CombinationOfDimensionAndBitrangeAccess,
                            // END of tests for production VariableExpression

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

                            AccessOnSameQubitOfGuardConditionPossibleOnLefthandSideOfAssignment,
                            OverlappingAccessOnQubitOfGuardConditionPossibleOnLefthandSideOfAssignment,
                            AccessOnSameQubitOfGuardConditionPossibleOnRighthandSideOfAssignmentUsingPrefixAssignmentOperand,
                            OverlappingAccessOnQubitOfGuardConditionPossibleOnRighthandSideOfAssignmentUsingPrefixAssignmentOperand,
                            // END of tests for production AssignStatement

                            // BEGIN of tests for production CallStatement
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
                            SynthesisOfNestedModuleCallHierarchy,
                            // END of tests for production CallStatement

                            // BEGIN of tests for production IfStatement
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingWholeBitwidthOfVariable,
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOf1DVariable,
                            IfStatementWithGuardConditionEqualToVariableAccessAccessingBitOfValueOfDimensionOfVariable,
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
                            IfStatementExecutionOfMultipleStatementsInFalseBranchIfBranchIsExecuted,
                            // END of tests for production IfStatement

                            // BEGIN of tests for production SkipStatement
                            SkipStatementInTrueBranchOfIfStatement,
                            SkipStatementInFalseBranchOfIfStatement,
                            SkipStatementInLoopBodyOfForStatement,
                            SkipStatementInModuleBody,
                            SkipStatementInModuleBodyOfCalledModule,
                            SkipStatementInModuleBodyOfUncalledModule,
                            // END of tests for production SkipStatement

                            // BEGIN of tests for production SwapStatement
                            SwapWithLeftOperationBeingAccessOn1DVariable,
                            SwapWithLeftOperationBeingAccessOn1DVariableWithBitOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOn1DVariableWithBitrangeOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOn1DVariableWithValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithSameBitOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOf1DVariableWithOtherBitOfOtherVariable,

                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfOtherDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithNotOverlappingBitOfValueOfSameDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithOverlappingBitOfValueOfOtherDimensionOfSameVariable,

                            SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartSmallerThanEnd,
                            SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartLargerThanEnd,
                            SwapWithLeftOperationBeingAccessOnBitrangeOf1DVariableWithStartEqualToEnd,
                            SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnValueOfDimensionOfVariableWithValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitOfValueOfDimensionOfVariableWithBitOfValueOfDimensionOfOtherVariable,

                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartSmallerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartLargerThanEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithBitrangeOfValueOfDimensionOfOtherVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithOverlappingBitrangeOfValueOfOtherDimensionOfSameVariable,
                            SwapWithLeftOperationBeingAccessOnBitrangeWithStartEqualToEndOfValueOfDimensionOfVariableWithNotOverlappingBitrangeOfValueOfSameDimensionOfSameVariable,
                            // END of tests for production SwapStatement

                            // BEGIN of tests for production UncallStatement
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
                            SynthesisOfNestedModuleUncallHierarchy
                            // END of tests for production UncallStatement
);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
