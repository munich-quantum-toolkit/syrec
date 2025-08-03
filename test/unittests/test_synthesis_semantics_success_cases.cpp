/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/internal_qubit_label_builder.hpp"
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/syrec/program.hpp"

#include "gmock/gmock-matchers.h"
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace syrec;

namespace {
    template<typename T>
    class SynthesisSemanticsSuccessCasesTestsFixture: public testing::Test {
    public:
        void SetUp() override {
            static_assert(std::is_same_v<T, CostAwareSynthesis> || std::is_same_v<T, LineAwareSynthesis>);
        }

        [[nodiscard]] static bool performProgramSynthesis(const Program& program, AnnotatableQuantumComputation& annotatableQuantumComputation, const std::optional<Properties::ptr>& optionalSynthesisSettings = std::nullopt) {
            Properties::ptr synthesisSettings = optionalSynthesisSettings.value_or(std::make_shared<Properties>());
            if constexpr (std::is_same_v<T, CostAwareSynthesis>) {
                return CostAwareSynthesis::synthesize(annotatableQuantumComputation, program, synthesisSettings);
            } else {
                return LineAwareSynthesis::synthesize(annotatableQuantumComputation, program, synthesisSettings);
            }
        }

        static void parseInputCircuitFromString(const std::string_view& stringifiedSyrecProgram, Program& parserInstance, const std::optional<ReadProgramSettings>& optionalParserConfiguration = std::nullopt) {
            std::string errorsOfReadInputCircuit;
            ASSERT_NO_FATAL_FAILURE(errorsOfReadInputCircuit = parserInstance.readFromString(stringifiedSyrecProgram, optionalParserConfiguration.value_or(syrec::ReadProgramSettings())));
            ASSERT_TRUE(errorsOfReadInputCircuit.empty()) << "Expected no errors in input circuits but actually found the following: " << errorsOfReadInputCircuit;
        }

        static void buildFullQubitLabel(const std::string& variableIdentifier, const std::vector<unsigned int>& accessedValuePerDimension, unsigned int bitOfBitwidth, std::string& generatedQubitLabel) {
            ASSERT_FALSE(accessedValuePerDimension.empty()) << "Qubit label can only be built if at least one accessed value of dimension is defined";
            generatedQubitLabel = variableIdentifier;
            for (const auto valueOfDimension: accessedValuePerDimension) {
                generatedQubitLabel += "[" + std::to_string(valueOfDimension) + "]";
            }
            generatedQubitLabel += "." + std::to_string(bitOfBitwidth);
        }

        static void assertInlineStacksOfVariablesReferenceSameInstance(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& firstQubitIdentifier, const std::string& secondQubitIdentifier) {
            const AnnotatableQuantumComputation::InlinedQubitInformation* firstQubitInlineInformation = annotatableQuantumComputation.getInliningInformationOfQubit(firstQubitIdentifier);
            ASSERT_THAT(firstQubitInlineInformation, testing::NotNull()) << "Could not fetch inline information for qubit " << firstQubitIdentifier;

            const AnnotatableQuantumComputation::InlinedQubitInformation* secondQubitInlineInformation = annotatableQuantumComputation.getInliningInformationOfQubit(secondQubitIdentifier);
            ASSERT_THAT(secondQubitInlineInformation, testing::NotNull()) << "Could not fetch inline information for qubit " << secondQubitIdentifier;

            if (firstQubitInlineInformation->inlineStack.has_value()) {
                ASSERT_TRUE(secondQubitInlineInformation->inlineStack.has_value()) << "Expected inline stack for qubit " << firstQubitIdentifier << " to have a value";

                const QubitInliningStack::ptr& firstQubitInlineStackReference  = *firstQubitInlineInformation->inlineStack;
                const QubitInliningStack::ptr& secondQubitInlineStackReference = *secondQubitInlineInformation->inlineStack;
                ASSERT_THAT(firstQubitInlineStackReference, testing::NotNull()) << "Expected inline stack for qubit " << firstQubitIdentifier << " not be be NULL";
                ASSERT_THAT(secondQubitInlineStackReference, testing::NotNull()) << "Expected inline stack for qubit " << secondQubitIdentifier << " not be be NULL";
                ASSERT_EQ(firstQubitInlineStackReference, secondQubitInlineStackReference) << "Qubit stacks did not reference same instance";
            } else {
                ASSERT_FALSE(secondQubitInlineInformation->inlineStack.has_value()) << "Expected inline stack for qubit " << firstQubitIdentifier << " to not have a value";
            }
        }

        static void assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& fullQubitLabel, const AnnotatableQuantumComputation::InlinedQubitInformation* expectedQubitInlineInformation) {
            const AnnotatableQuantumComputation::InlinedQubitInformation* actualQubitInlineInformation = annotatableQuantumComputation.getInliningInformationOfQubit(fullQubitLabel);
            if (expectedQubitInlineInformation == nullptr) {
                ASSERT_THAT(actualQubitInlineInformation, testing::IsNull()) << "No inline information for qubit " << fullQubitLabel << " should exist";
            } else {
                ASSERT_THAT(actualQubitInlineInformation, testing::NotNull()) << "Expected inline information for qubit " << actualQubitInlineInformation << " to exist";
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatches(*expectedQubitInlineInformation, *actualQubitInlineInformation));
            }
        }

        static void assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& variableIdentifier, const std::vector<unsigned int>& accessedValuePerDimension, unsigned int accessedBit, const AnnotatableQuantumComputation::InlinedQubitInformation* expectedQubitInlineInformation) {
            std::string fullQubitLabel;
            ASSERT_NO_FATAL_FAILURE(buildFullQubitLabel(variableIdentifier, accessedValuePerDimension, accessedBit, fullQubitLabel));
            ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(annotatableQuantumComputation, fullQubitLabel, expectedQubitInlineInformation));
        }

        static void assertQubitInlineInformationOfAncillaryQubitMatches(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::size_t numQubitsInQuantumComputationPriorToCreationOfAncillaryQubit, bool expectedInitialStateOfAncillaryQubit, const AnnotatableQuantumComputation::InlinedQubitInformation* expectedQubitInlineInformation) {
            const std::string ancillaryQubitLabel = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationPriorToCreationOfAncillaryQubit, expectedInitialStateOfAncillaryQubit);

            const AnnotatableQuantumComputation::InlinedQubitInformation* actualQubitInlineInformation = annotatableQuantumComputation.getInliningInformationOfQubit(ancillaryQubitLabel);
            if (expectedQubitInlineInformation == nullptr) {
                ASSERT_THAT(actualQubitInlineInformation, testing::IsNull()) << "No inline information for qubit " << ancillaryQubitLabel << " should exist";
            } else {
                ASSERT_THAT(actualQubitInlineInformation, testing::NotNull()) << "Expected inline information for qubit " << ancillaryQubitLabel << " to exist";
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatches(*expectedQubitInlineInformation, *actualQubitInlineInformation));
            }
        }

    protected:
        AnnotatableQuantumComputation annotatableQuantumComputation;
        Program                       syrecProgramInstance;

        static void assertQubitInlineInformationMatches(const AnnotatableQuantumComputation::InlinedQubitInformation& expected, const AnnotatableQuantumComputation::InlinedQubitInformation& actual) {
            if (expected.inlineStack.has_value()) {
                ASSERT_TRUE(actual.inlineStack.has_value()) << "Actual inline stack was expected to have a value";
                const QubitInliningStack::ptr& expectedInlineStack = expected.inlineStack.value();
                const QubitInliningStack::ptr& actualInlineStack   = actual.inlineStack.value();

                ASSERT_THAT(expectedInlineStack, testing::NotNull()) << "Expected inline stack was NULL";
                ASSERT_THAT(actualInlineStack, testing::NotNull()) << "Actual inline stack was NULL";
                ASSERT_NO_FATAL_FAILURE(assertInlineStacksMatch(*expectedInlineStack, *actualInlineStack));
            } else {
                ASSERT_FALSE(actual.inlineStack.has_value()) << "Actual inline stack was not expected to have a value";
            }

            if (expected.userDeclaredQubitLabel.has_value()) {
                ASSERT_TRUE(actual.userDeclaredQubitLabel.has_value()) << "Actual user declared qubit label was expected to have a value";
                ASSERT_EQ(*expected.userDeclaredQubitLabel, *actual.userDeclaredQubitLabel) << "User declared qubit label mismatch";
            } else {
                ASSERT_FALSE(actual.userDeclaredQubitLabel.has_value()) << "Actual user declared qubit label was not expected to have a value";
            }
        }

        static void assertInlineStacksMatch(QubitInliningStack& expected, QubitInliningStack& actual) {
            ASSERT_EQ(expected.size(), actual.size()) << "Inline stack size mismatch";
            for (std::size_t i = 0; i < expected.size(); ++i) {
                const QubitInliningStack::QubitInliningStackEntry* expectedStackEntry = expected.getStackEntryAt(i);
                const QubitInliningStack::QubitInliningStackEntry* actualStackEntry   = actual.getStackEntryAt(i);
                ASSERT_THAT(expectedStackEntry, testing::NotNull()) << "Expected stack entry at index" << std::to_string(i) << " was NULL";
                ASSERT_THAT(actualStackEntry, testing::NotNull()) << "Actual stack entry at index" << std::to_string(i) << " was NULL";
                ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(*expectedStackEntry, *actualStackEntry)) << "Stack entry mismatch at index " << std::to_string(i);
            }
        }

        static void assertInlineStackEntriesMatch(const QubitInliningStack::QubitInliningStackEntry& expected, const QubitInliningStack::QubitInliningStackEntry& actual) {
            ASSERT_THAT(expected.targetModule, testing::NotNull()) << "Expected target module not to be NULL";
            ASSERT_THAT(actual.targetModule, testing::NotNull()) << "Actual target module was expected not be be NULL";
            ASSERT_EQ(expected.targetModule, actual.targetModule) << "Target module mismatch";

            if (expected.isTargetModuleAccessedViaCallStmt.has_value()) {
                ASSERT_TRUE(actual.isTargetModuleAccessedViaCallStmt.has_value()) << "Call type of target module in actual stack entry should be known";
                ASSERT_EQ(*expected.isTargetModuleAccessedViaCallStmt, *actual.isTargetModuleAccessedViaCallStmt) << "Call type of target module mismatch";
            } else {
                ASSERT_FALSE(actual.isTargetModuleAccessedViaCallStmt.has_value()) << "Call type of target module in actual stack entry should not be known";
            }

            if (expected.lineNumberOfCallOfTargetModule.has_value()) {
                ASSERT_TRUE(actual.lineNumberOfCallOfTargetModule.has_value()) << "Line number in source code of call of target module in actual stack entry should be known";
                ASSERT_EQ(*expected.lineNumberOfCallOfTargetModule, *actual.lineNumberOfCallOfTargetModule) << "Line number in source code of call of target module mismatch";
            } else {
                ASSERT_FALSE(actual.lineNumberOfCallOfTargetModule.has_value()) << "Line number in source code of call of target module in actual stack entry should not be known";
            }
        }
    };
} // namespace

TYPED_TEST_SUITE_P(SynthesisSemanticsSuccessCasesTestsFixture);

// BEGIN tests for inlined qubit information behaviour with feature activated in synthesis settings
TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfMainModuleParameters) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module main(inout a(4), out b(4)) a += b";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalMainModuleVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module main() wire a(4), b(4) a += b";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    const Module::ptr& sharedTargetModule = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(sharedTargetModule, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, sharedTargetModule})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    unsigned int currNumQubitsInQuantumComputation = 0;
    for (const auto& variableIdentifier: {"a", "b"}) {
        constexpr unsigned int moduleVariableBitwidth = 4;
        for (unsigned int i = 0; i < moduleVariableBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(variableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += moduleVariableBitwidth;
    }
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfCalledModuleParameters) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) call add(a, b)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfCalledModuleLocalVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(a, b)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth      = 4;
    unsigned int           currNumQubitsInQuantumComputation = 2 * mainModuleParametersBitwidth;
    for (const auto& mainModuleLocalVariableIdentifier: {"x", "y"}) {
        constexpr unsigned int mainModuleLocalVariableBitwidth = 2;
        for (unsigned int i = 0; i < mainModuleLocalVariableBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += mainModuleLocalVariableBitwidth;
    }

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto* firstInlineStackEntry                              = sharedInlineStack->getStackEntryAt(0);
    firstInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    firstInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;

    const auto calledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, calledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(calledModuleInlineStackEntry));

    for (const auto& mainModuleLocalVariableIdentifier: {"s", "t"}) {
        constexpr unsigned int calledModuleLocalVariablesBitwidth = 3;
        for (unsigned int i = 0; i < calledModuleLocalVariablesBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += calledModuleLocalVariablesBitwidth;
    }
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfUncalledModuleParameters) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) uncall add(a, b)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfUncalledLocalModuleVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(a, b)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth      = 4;
    unsigned int           currNumQubitsInQuantumComputation = 2 * mainModuleParametersBitwidth;
    for (const auto& mainModuleLocalVariableIdentifier: {"x", "y"}) {
        constexpr unsigned int mainModuleLocalVariableBitwidth = 2;
        for (unsigned int i = 0; i < mainModuleLocalVariableBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += mainModuleLocalVariableBitwidth;
    }

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto* firstInlineStackEntry                              = sharedInlineStack->getStackEntryAt(0);
    firstInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    firstInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;

    const auto uncalledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, uncalledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(uncalledModuleInlineStackEntry));

    for (const auto& mainModuleLocalVariableIdentifier: {"s", "t"}) {
        constexpr unsigned int uncalledModuleLocalVariablesBitwidth = 3;
        for (unsigned int i = 0; i < uncalledModuleLocalVariablesBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += uncalledModuleLocalVariablesBitwidth;
    }
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module main(inout a(4), out b(4)) a += b; a += 2";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int moduleParametersBitwidth                             = 4;
    constexpr unsigned int numQubitsInQuantumComputationBasedOnModuleParameters = 2 * moduleParametersBitwidth;

    constexpr bool     expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string& firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = true;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module main(inout a(2), out b(4)) a += (b.0:1 & b.2:3)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int numQubitsInQuantumComputationBasedOnModuleParameters = 6;

    constexpr bool     expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string& firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = false;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) call addWithConst(a); a += b";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());
    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("addWithConst");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    QubitInliningStack::QubitInliningStackEntry* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, calledModuleReference})));

    constexpr unsigned int moduleParametersBitwidth                             = 2;
    constexpr unsigned int numQubitsInQuantumComputationBasedOnModuleParameters = 2 * moduleParametersBitwidth;

    constexpr bool     expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string& firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = true;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) call add(a, b)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());
    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    QubitInliningStack::QubitInliningStackEntry* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, calledModuleReference})));

    constexpr unsigned int numQubitsInQuantumComputationBasedOnModuleParameters = 6;

    constexpr bool     expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string& firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = false;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) uncall addWithConst(a); a += b";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());
    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("addWithConst");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    QubitInliningStack::QubitInliningStackEntry* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, uncalledModuleReference})));

    constexpr unsigned int moduleParametersBitwidth                             = 2;
    constexpr unsigned int numQubitsInQuantumComputationBasedOnModuleParameters = 2 * moduleParametersBitwidth;

    constexpr bool     expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string& firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = true;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) uncall add(a, b)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());
    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    QubitInliningStack::QubitInliningStackEntry* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, uncalledModuleReference})));

    constexpr unsigned int numQubitsInQuantumComputationBasedOnModuleParameters = 6;

    constexpr bool     expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string& firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = false;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, numQubitsInQuantumComputationBasedOnModuleParameters + 1, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth      = 4;
    unsigned int           currNumQubitsInQuantumComputation = 2 * mainModuleParametersBitwidth;
    for (const auto& mainModuleLocalVariableIdentifier: {"x", "y"}) {
        constexpr unsigned int mainModuleLocalVariableBitwidth = 2;
        for (unsigned int i = 0; i < mainModuleLocalVariableBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += mainModuleLocalVariableBitwidth;
    }

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto* firstInlineStackEntry                              = sharedInlineStack->getStackEntryAt(0);
    firstInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    firstInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;

    const auto calledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, calledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(calledModuleInlineStackEntry));

    for (const auto& mainModuleLocalVariableIdentifier: {"s", "t"}) {
        constexpr unsigned int uncalledModuleLocalVariablesBitwidth = 3;
        for (unsigned int i = 0; i < uncalledModuleLocalVariablesBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += uncalledModuleLocalVariablesBitwidth;
    }
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInUncalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);

    constexpr std::string_view stringifiedSyrecProgram = "module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)";
    ASSERT_NO_FATAL_FAILURE(this->parseInputCircuitFromString(stringifiedSyrecProgram, this->syrecProgramInstance));
    ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth      = 4;
    unsigned int           currNumQubitsInQuantumComputation = 2 * mainModuleParametersBitwidth;
    for (const auto& mainModuleLocalVariableIdentifier: {"x", "y"}) {
        constexpr unsigned int mainModuleLocalVariableBitwidth = 2;
        for (unsigned int i = 0; i < mainModuleLocalVariableBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += mainModuleLocalVariableBitwidth;
    }

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto* firstInlineStackEntry                              = sharedInlineStack->getStackEntryAt(0);
    firstInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    firstInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;

    const auto uncalledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, uncalledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(uncalledModuleInlineStackEntry));

    for (const auto& mainModuleLocalVariableIdentifier: {"s", "t"}) {
        constexpr unsigned int uncalledModuleLocalVariablesBitwidth = 3;
        for (unsigned int i = 0; i < uncalledModuleLocalVariablesBitwidth; ++i) {
            std::string internalQubitLabel;
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i, internalQubitLabel));
            ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(mainModuleLocalVariableIdentifier, {0U}, i, *qubitInlineInformation.userDeclaredQubitLabel));

            ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabel, &qubitInlineInformation));

            if (i > 0) {
                std::string internalQubitLabelOfPrevQubitOfVariable;
                ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubitsInQuantumComputation), {0U}, i - 1, internalQubitLabelOfPrevQubitOfVariable));
                ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabel, internalQubitLabelOfPrevQubitOfVariable));
            }
        }
        currNumQubitsInQuantumComputation += uncalledModuleLocalVariablesBitwidth;
    }
}
// END tests for inlined qubit information behaviour with feature activated in synthesis settings

// BEGIN tests for inlined qubit information behaviour with feature deactivated in synthesis settings
TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfMainModuleParameters) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalMainModuleVariables) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleParameters) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleVariables) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleParameters) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleVariables) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfModuleParametersUsedAsParametersInCalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfModuleParametersUsedAsParametersInUncalledModule) {
    GTEST_SKIP();
}

TYPED_TEST_P(SynthesisSemanticsSuccessCasesTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalModuleVariablesUsedAsParametersInUncalledModule) {
    GTEST_SKIP();
}
// END tests for inlined qubit information behaviour with feature deactivated in synthesis settings

REGISTER_TYPED_TEST_SUITE_P(SynthesisSemanticsSuccessCasesTestsFixture,
                            InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfMainModuleParameters,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalMainModuleVariables,
                            InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfCalledModuleParameters,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfCalledModuleLocalVariables,
                            InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfUncalledModuleParameters,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfUncalledLocalModuleVariables,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule,
                            InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInUncalledModule,

                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfMainModuleParameters,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalMainModuleVariables,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleParameters,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleVariables,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleParameters,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleVariables,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfModuleParametersUsedAsParametersInCalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfModuleParametersUsedAsParametersInUncalledModule,
                            InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalModuleVariablesUsedAsParametersInUncalledModule);

using SynthesizerTypes = testing::Types<CostAwareSynthesis, LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, SynthesisSemanticsSuccessCasesTestsFixture, SynthesizerTypes, );
