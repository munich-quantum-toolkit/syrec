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
                            UserDefinedModuleIdentifierInSynthesisSettingsOnlyMatchingModulesWithSameIdentifierCharacterCasing);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
