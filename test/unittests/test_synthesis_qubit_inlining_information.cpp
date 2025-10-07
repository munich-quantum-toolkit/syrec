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
#include "algorithms/synthesis/syrec_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/qubit_inlining_stack.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/program.hpp"

#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace syrec;

namespace {
    template<typename T>
    class SynthesisQubitInlinineInformationTestsFixture: public testing::Test {
    public:
        void SetUp() override {
            static_assert(std::is_same_v<T, CostAwareSynthesis> || std::is_same_v<T, LineAwareSynthesis>);
        }

        [[nodiscard]] static bool performProgramSynthesis(const Program& program, AnnotatableQuantumComputation& annotatableQuantumComputation, const std::optional<Properties::ptr>& optionalSynthesisSettings = std::nullopt) {
            const Properties::ptr synthesisSettings = optionalSynthesisSettings.value_or(std::make_shared<Properties>());
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

        static void parseAndSynthesisProgramFromString(const std::string_view& stringifiedSyrecProgram, Program& containerForGeneratedIr, AnnotatableQuantumComputation& annotatableQuantumComputation, const std::optional<Properties::ptr>& optionalSynthesisSettings = std::nullopt) {
            ASSERT_NO_FATAL_FAILURE(parseInputCircuitFromString(stringifiedSyrecProgram, containerForGeneratedIr));
            ASSERT_TRUE(performProgramSynthesis(containerForGeneratedIr, annotatableQuantumComputation, optionalSynthesisSettings)) << "Failed to synthesize SyReC program: " << stringifiedSyrecProgram;
        }

        static void buildFullQubitLabel(const std::string& variableIdentifier, const std::vector<unsigned int>& accessedValuePerDimension, unsigned int accessedBitOfVariable, std::string& generatedQubitLabel) {
            ASSERT_FALSE(accessedValuePerDimension.empty()) << "Qubit label can only be built if at least one accessed value of dimension is defined";
            generatedQubitLabel = variableIdentifier;
            for (const auto valueOfDimension: accessedValuePerDimension) {
                generatedQubitLabel += "[" + std::to_string(valueOfDimension) + "]";
            }
            generatedQubitLabel += "." + std::to_string(accessedBitOfVariable);
        }

        static void assertInlineStacksOfVariablesReferenceSameInstance(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& firstQubitIdentifier, const std::string& secondQubitIdentifier) {
            ASSERT_NO_FATAL_FAILURE(assertInlineStacksOfVariablesReferenceConditionalEquivalence(annotatableQuantumComputation, firstQubitIdentifier, secondQubitIdentifier, true));
        }

        static void assertInlineStacksOfVariablesDoNotReferenceSameInstance(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& firstQubitIdentifier, const std::string& secondQubitIdentifier) {
            ASSERT_NO_FATAL_FAILURE(assertInlineStacksOfVariablesReferenceConditionalEquivalence(annotatableQuantumComputation, firstQubitIdentifier, secondQubitIdentifier, false));
        }

        static void assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& fullQubitLabel, const AnnotatableQuantumComputation::InlinedQubitInformation* expectedQubitInlineInformation) {
            const AnnotatableQuantumComputation::InlinedQubitInformation* actualQubitInlineInformation = annotatableQuantumComputation.getInliningInformationOfQubit(fullQubitLabel);
            if (expectedQubitInlineInformation == nullptr) {
                ASSERT_THAT(actualQubitInlineInformation, testing::IsNull()) << "No inline information for qubit " << fullQubitLabel << " should exist";
            } else {
                ASSERT_THAT(actualQubitInlineInformation, testing::NotNull()) << "Expected inline information for qubit " << fullQubitLabel << " to exist";
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatches(*expectedQubitInlineInformation, *actualQubitInlineInformation)) << "Actual qubit inline information of qubit " << fullQubitLabel << " did not match expected one";
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
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatches(*expectedQubitInlineInformation, *actualQubitInlineInformation)) << "Actual qubit inline information of qubit " << ancillaryQubitLabel << " did not match expected one";
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

        static void assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& variableIdentifier, const std::vector<unsigned int>& accessedValuePerDimension, unsigned int variableBitwidth, unsigned int indexOfFirstQubitToCheckInQuantumComputation, AnnotatableQuantumComputation::InlinedQubitInformation sharedQubitInlineInformation) {
            ASSERT_TRUE(sharedQubitInlineInformation.userDeclaredQubitLabel.has_value()) << "Inline information of non-ancillary qubit will have a user declared qubit label stored but container did not initialize the value";

            const std::string& sharedInternalQubitLabelPrefix = InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitToCheckInQuantumComputation);
            for (unsigned int qubitIndex = 0; qubitIndex < variableBitwidth; ++qubitIndex) {
                std::string internalQubitLabel;
                ASSERT_NO_FATAL_FAILURE(buildFullQubitLabel(sharedInternalQubitLabelPrefix, accessedValuePerDimension, qubitIndex, internalQubitLabel));
                ASSERT_NO_FATAL_FAILURE(buildFullQubitLabel(variableIdentifier, accessedValuePerDimension, qubitIndex, *sharedQubitInlineInformation.userDeclaredQubitLabel));
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(annotatableQuantumComputation, internalQubitLabel, &sharedQubitInlineInformation));

                // All qubits of a value of a dimension of a variable should share the same inline stack
                if (qubitIndex > 0) {
                    std::string internalQubitLabelAtPrevQubitIndex;
                    ASSERT_NO_FATAL_FAILURE(buildFullQubitLabel(sharedInternalQubitLabelPrefix, accessedValuePerDimension, qubitIndex - 1U, internalQubitLabelAtPrevQubitIndex));
                    ASSERT_NO_FATAL_FAILURE(assertInlineStacksOfVariablesReferenceSameInstance(annotatableQuantumComputation, internalQubitLabel, internalQubitLabelAtPrevQubitIndex));
                }
            }
        }

        static void assertInlineStacksOfVariablesReferenceConditionalEquivalence(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& firstQubitIdentifier, const std::string& secondQubitIdentifier, bool shouldReferencesBeEqual) {
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

                if (shouldReferencesBeEqual) {
                    ASSERT_EQ(firstQubitInlineStackReference, secondQubitInlineStackReference) << "Qubit stacks did not reference same instance";
                } else {
                    ASSERT_NE(firstQubitInlineStackReference, secondQubitInlineStackReference) << "Qubit stacks did reference same instance";
                }
            } else {
                ASSERT_FALSE(secondQubitInlineInformation->inlineStack.has_value()) << "Expected inline stack for qubit " << firstQubitIdentifier << " to not have a value";
            }
        }
    };
} // namespace

TYPED_TEST_SUITE_P(SynthesisQubitInlinineInformationTestsFixture);

// BEGIN tests for inlined qubit information behaviour with feature activated in synthesis settings
TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfMainModuleParameters) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalMainModuleVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main() wire a(4), b(4) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    const Module::ptr& sharedTargetModule = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(sharedTargetModule, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, sharedTargetModule})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    constexpr unsigned int mainModuleLocalVariableBitwidth = 4U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "a", {0U}, mainModuleLocalVariableBitwidth, 0U, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "b", {0U}, mainModuleLocalVariableBitwidth, 4U, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(0U), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(4U), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfCalledModuleParameters) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfCalledModuleLocalVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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
    constexpr unsigned int mainModuleLocalVariableBitwidth             = 2U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 8U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables + mainModuleLocalVariableBitwidth, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables + mainModuleLocalVariableBitwidth), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto* firstInlineStackEntry                              = sharedInlineStack->getStackEntryAt(0);
    firstInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    firstInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;

    const auto calledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(calledModuleInlineStackEntry));

    constexpr unsigned int calledModuleLocalVariablesBitwidth            = 3U;
    constexpr unsigned int indexOfFirstQubitOfCalledModuleLocalVariables = indexOfFirstQubitOfMainModuleLocalVariables + (2 * mainModuleLocalVariableBitwidth);

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfCalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfCalledModuleLocalVariables + calledModuleLocalVariablesBitwidth, qubitInlineInformation));

    std::string firstQubitOfFirstCalledModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondCalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModuleLocalVariables), {0U}, 0U, firstQubitOfFirstCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModuleLocalVariables + calledModuleLocalVariablesBitwidth), {0U}, 0U, firstQubitOfSecondCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstCalledModuleLocalVariableQubitLabel, firstQubitOfSecondCalledModuleLocalVariableQubitLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfUncalledModuleParameters) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfUncalledLocalModuleVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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
    constexpr unsigned int mainModuleLocalVariableBitwidth             = 2U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 8U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables + mainModuleLocalVariableBitwidth, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables + mainModuleLocalVariableBitwidth), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));

    // Check inline information of local variables of called module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto* firstInlineStackEntry                              = sharedInlineStack->getStackEntryAt(0);
    firstInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    firstInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;

    const auto uncalledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(uncalledModuleInlineStackEntry));

    constexpr unsigned int uncalledModuleLocalVariablesBitwidth            = 3U;
    constexpr unsigned int indexOfFirstQubitOfUncalledModuleLocalVariables = indexOfFirstQubitOfMainModuleLocalVariables + (2 * mainModuleLocalVariableBitwidth);

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfUncalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfUncalledModuleLocalVariables + uncalledModuleLocalVariablesBitwidth, qubitInlineInformation));

    std::string firstQubitOfFirstUncalledModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondUncalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModuleLocalVariables), {0U}, 0U, firstQubitOfFirstUncalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModuleLocalVariables + uncalledModuleLocalVariablesBitwidth), {0U}, 0U, firstQubitOfSecondUncalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstUncalledModuleLocalVariableQubitLabel, firstQubitOfSecondUncalledModuleLocalVariableQubitLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b; a += 2", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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

    constexpr unsigned int indexOfFirstAncillaryQubit                = 8U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string&     firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = true;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(2), out b(4)) a += (b.0:1 & b.2:3)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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

    constexpr unsigned int indexOfFirstAncillaryQubit                = 6U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string&     firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = false;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) call addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("addWithConst");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    mainModuleCallStackEntry.targetModule                      = mainModuleReference;
    mainModuleCallStackEntry.isTargetModuleAccessedViaCallStmt = true;
    mainModuleCallStackEntry.lineNumberOfCallOfTargetModule    = 1U;

    auto calledModuleCallStackEntry         = QubitInliningStack::QubitInliningStackEntry();
    calledModuleCallStackEntry.targetModule = calledModuleReference;

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 4U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string&     firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = true;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    mainModuleCallStackEntry.targetModule                      = mainModuleReference;
    mainModuleCallStackEntry.isTargetModuleAccessedViaCallStmt = true;
    mainModuleCallStackEntry.lineNumberOfCallOfTargetModule    = 1U;

    auto calledModuleCallStackEntry         = QubitInliningStack::QubitInliningStackEntry();
    calledModuleCallStackEntry.targetModule = calledModuleReference;

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 6U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string&     firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = false;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) uncall addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("addWithConst");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    mainModuleCallStackEntry.targetModule                      = mainModuleReference;
    mainModuleCallStackEntry.isTargetModuleAccessedViaCallStmt = false;
    mainModuleCallStackEntry.lineNumberOfCallOfTargetModule    = 1U;

    auto calledModuleCallStackEntry         = QubitInliningStack::QubitInliningStackEntry();
    calledModuleCallStackEntry.targetModule = calledModuleReference;

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 4U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string&     firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = true;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    mainModuleCallStackEntry.targetModule                      = mainModuleReference;
    mainModuleCallStackEntry.isTargetModuleAccessedViaCallStmt = false;
    mainModuleCallStackEntry.lineNumberOfCallOfTargetModule    = 1U;

    auto calledModuleCallStackEntry         = QubitInliningStack::QubitInliningStackEntry();
    calledModuleCallStackEntry.targetModule = calledModuleReference;

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 6U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    const std::string&     firstAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubit = false;
    const std::string& secondAncillaryQubitInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitInternalLabel, secondAncillaryQubitInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());

    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;

    const auto calledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(calledModuleInlineStackEntry));

    constexpr unsigned int indexOfFirstQubitOfCalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfCalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables + bitwidthOfCalledModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstCalledModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondCalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModuleLocalVariables), {0U}, 0U, firstQubitOfFirstCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModuleLocalVariables + bitwidthOfCalledModuleLocalVariables), {0U}, 0U, firstQubitOfSecondCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstCalledModuleLocalVariableQubitLabel, firstQubitOfSecondCalledModuleLocalVariableQubitLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInUncalledModule) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());

    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;

    const auto uncalledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(uncalledModuleInlineStackEntry));

    constexpr unsigned int indexOfFirstQubitOfUncalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfUncalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables + bitwidthOfUncalledModuleLocalVariables, qubitInlineInformation));

    std::string qubitLabelOfFirstLocalVariableOfUncalledModule;
    std::string qubitLabelOfSecondLocalVariableOfUncalledModule;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModuleLocalVariables), {0U}, 0U, qubitLabelOfFirstLocalVariableOfUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModuleLocalVariables + bitwidthOfUncalledModuleLocalVariables), {0U}, 0U, qubitLabelOfSecondLocalVariableOfUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, qubitLabelOfFirstLocalVariableOfUncalledModule, qubitLabelOfSecondLocalVariableOfUncalledModule));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedLocalModuleVariablesAndAncillaryQubitsOfCalledModuleOnSameDepthOfInlineStackShareSameInlineStack) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(in a(2)) wire s(3) s += 3 module main() wire x(2) x += 2; call add(x); x += 3", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables and ancillary qubits of main module
    constexpr unsigned int mainModuleLocalVariableBitwidth = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, mainModuleLocalVariableBitwidth, 0U, qubitInlineInformation));

    constexpr unsigned int indexOfFirstAncillaryQubitInMainModule = mainModuleLocalVariableBitwidth;

    qubitInlineInformation.userDeclaredQubitLabel                            = std::nullopt;
    constexpr bool     expectedInitialStateOfFirstAncillaryQubitOfMainModule = false;
    const std::string& firstAncillaryQubitOfMainModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInMainModule, expectedInitialStateOfFirstAncillaryQubitOfMainModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInMainModule, expectedInitialStateOfFirstAncillaryQubitOfMainModule, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubitOfMainModule = true;
    const std::string& secondAncillaryQubitOfMainModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInMainModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfMainModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInMainModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfMainModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfMainModuleInternalLabel, secondAncillaryQubitOfMainModuleInternalLabel));

    // Local variable qubits and ancillary qubits of main module should shared inline stack
    std::string firstQubitOfMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(0U), {0U}, 0U, firstQubitOfMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfMainModuleInternalLabel, firstQubitOfMainModuleLocalVariableQubitLabel));

    const AnnotatableQuantumComputation::InlinedQubitInformation* inlineInformationOfFirstQubitOfLocalVariableOfMainModule = this->annotatableQuantumComputation.getInliningInformationOfQubit(firstQubitOfMainModuleLocalVariableQubitLabel);
    ASSERT_THAT(inlineInformationOfFirstQubitOfLocalVariableOfMainModule, testing::NotNull());
    ASSERT_TRUE(inlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack.has_value());
    const QubitInliningStack::ptr& backupOfInlineStackOfFirstQubitOfLocalVariableOfMainModule = *inlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack;

    // Check inline information of local variables and ancillary qubits of called module
    auto calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, calledModuleReference})));

    auto* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;

    // Since we are again checking the inline information of local variable qubits, we need to set a default value for the user declared qubit value so it can later be updated per reference.
    qubitInlineInformation.userDeclaredQubitLabel = "";

    constexpr unsigned int indexOfFirstQubitOfCalledModule        = mainModuleLocalVariableBitwidth + 2U;
    constexpr unsigned int bitwidthOfLocalVariablesOfCalledModule = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, bitwidthOfLocalVariablesOfCalledModule, indexOfFirstQubitOfCalledModule, qubitInlineInformation));

    constexpr unsigned int indexOfFirstAncillaryQubitInCalledModule = indexOfFirstQubitOfCalledModule + bitwidthOfLocalVariablesOfCalledModule;

    qubitInlineInformation.userDeclaredQubitLabel                              = std::nullopt;
    constexpr bool     expectedInitialStateOfFirstAncillaryQubitOfCalledModule = true;
    const std::string& firstAncillaryQubitOfCalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInCalledModule, expectedInitialStateOfFirstAncillaryQubitOfCalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInCalledModule, expectedInitialStateOfFirstAncillaryQubitOfCalledModule, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubitOfCalledModule = true;
    const std::string& secondAncillaryQubitOfCalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInCalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfCalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInCalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfCalledModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfCalledModuleInternalLabel, secondAncillaryQubitOfCalledModuleInternalLabel));

    constexpr bool     expectedInitialStateOfThirdAncillaryQubitOfCalledModule = false;
    const std::string& thirdAncillaryQubitOfCalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInCalledModule + 2U, expectedInitialStateOfThirdAncillaryQubitOfCalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInCalledModule + 2U, expectedInitialStateOfThirdAncillaryQubitOfCalledModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, secondAncillaryQubitOfCalledModuleInternalLabel, thirdAncillaryQubitOfCalledModuleInternalLabel));

    // Local variable qubits and ancillary qubits of called module should shared inline stack
    std::string firstQubitOfCalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModule), {0U}, 0U, firstQubitOfCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, thirdAncillaryQubitOfCalledModuleInternalLabel, firstQubitOfCalledModuleLocalVariableQubitLabel));

    // While the inline stacks on each level of the call stack should be shared, the inline stack of the main module should not be modified during the creation of the inline stack of the called module
    const AnnotatableQuantumComputation::InlinedQubitInformation* refetchedInlineInformationOfFirstQubitOfLocalVariableOfMainModule = this->annotatableQuantumComputation.getInliningInformationOfQubit(firstQubitOfMainModuleLocalVariableQubitLabel);
    ASSERT_THAT(refetchedInlineInformationOfFirstQubitOfLocalVariableOfMainModule, testing::NotNull());
    ASSERT_TRUE(refetchedInlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack.has_value());
    const QubitInliningStack::ptr& refetchedInlineStackOfFirstQubitOfLocalVariableOfMainModule = *inlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack;
    ASSERT_EQ(backupOfInlineStackOfFirstQubitOfLocalVariableOfMainModule, refetchedInlineStackOfFirstQubitOfLocalVariableOfMainModule) << "Expected inline stack of qubits of main module to not be modify by creation of inline information of qubits of called module";

    // Since we are reusing the same inline stack instance to define our expected data for both the main as well as the called module and since we are now validating the data of ancillary qubits of the main module
    // a pop to remove the inline stack entry for the called module is needed.
    ASSERT_TRUE(qubitInlineInformation.inlineStack.value()->pop());

    mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());

    // Only when a Call-/UncallStatement is synthesis will this information be set in the parent of the last added inline stack entry, since only one element is on the stack - no such information should be set.
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = std::nullopt;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubitAfterCalledModule                                  = indexOfFirstAncillaryQubitInCalledModule + 3U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubitCreatedAfterSynthesisOfCalledModule = true;
    const std::string&     firstAncillaryQubitCreatedAfterSynthesisOfCalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitAfterCalledModule, expectedInitialStateOfFirstAncillaryQubitCreatedAfterSynthesisOfCalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitAfterCalledModule, expectedInitialStateOfFirstAncillaryQubitCreatedAfterSynthesisOfCalledModule, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubitCreatedAfterSynthesisOfCalledModule = true;
    const std::string& secondAncillaryQubitCreatedAfterSynthesisOfCalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitAfterCalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitCreatedAfterSynthesisOfCalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitAfterCalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitCreatedAfterSynthesisOfCalledModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitCreatedAfterSynthesisOfCalledModuleInternalLabel, secondAncillaryQubitCreatedAfterSynthesisOfCalledModuleInternalLabel));

    // The ancillary qubits created after the called module was synthesized should use the inline stack instance as the other qubits created for the main modules local variables and previously created ancillary qubits
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfMainModuleInternalLabel, firstAncillaryQubitCreatedAfterSynthesisOfCalledModuleInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureActivatedLocalModuleVariablesAndAncillaryQubitsOfUncalledModuleOnSameDepthOfInlineStackShareSameInlineStack) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(in a(2)) wire s(3) s += 3 module main() wire x(2) x += 2; uncall add(x); x += 3", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables and ancillary qubits of main module
    constexpr unsigned int mainModuleLocalVariableBitwidth = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, mainModuleLocalVariableBitwidth, 0U, qubitInlineInformation));

    constexpr unsigned int indexOfFirstAncillaryQubitInMainModule = mainModuleLocalVariableBitwidth;

    qubitInlineInformation.userDeclaredQubitLabel                            = std::nullopt;
    constexpr bool     expectedInitialStateOfFirstAncillaryQubitOfMainModule = false;
    const std::string& firstAncillaryQubitOfMainModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInMainModule, expectedInitialStateOfFirstAncillaryQubitOfMainModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInMainModule, expectedInitialStateOfFirstAncillaryQubitOfMainModule, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubitOfMainModule = true;
    const std::string& secondAncillaryQubitOfMainModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInMainModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfMainModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInMainModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfMainModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfMainModuleInternalLabel, secondAncillaryQubitOfMainModuleInternalLabel));

    // Local variable qubits and ancillary qubits of main module should shared inline stack
    std::string firstQubitOfMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(0U), {0U}, 0U, firstQubitOfMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfMainModuleInternalLabel, firstQubitOfMainModuleLocalVariableQubitLabel));

    const AnnotatableQuantumComputation::InlinedQubitInformation* inlineInformationOfFirstQubitOfLocalVariableOfMainModule = this->annotatableQuantumComputation.getInliningInformationOfQubit(firstQubitOfMainModuleLocalVariableQubitLabel);
    ASSERT_THAT(inlineInformationOfFirstQubitOfLocalVariableOfMainModule, testing::NotNull());
    ASSERT_TRUE(inlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack.has_value());
    const QubitInliningStack::ptr& backupOfInlineStackOfFirstQubitOfLocalVariableOfMainModule = *inlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack;

    // Check inline information of local variables and ancillary qubits of called module
    auto uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, uncalledModuleReference})));

    auto* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;

    // Since we are again checking the inline information of local variable qubits, we need to set a default value for the user declared qubit value so it can later be updated per reference.
    qubitInlineInformation.userDeclaredQubitLabel = "";

    constexpr unsigned int indexOfFirstQubitOfUncalledModule        = mainModuleLocalVariableBitwidth + 2U;
    constexpr unsigned int bitwidthOfLocalVariablesOfUncalledModule = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, bitwidthOfLocalVariablesOfUncalledModule, indexOfFirstQubitOfUncalledModule, qubitInlineInformation));

    constexpr unsigned int indexOfFirstAncillaryQubitInCalledModule = indexOfFirstQubitOfUncalledModule + bitwidthOfLocalVariablesOfUncalledModule;

    qubitInlineInformation.userDeclaredQubitLabel                                = std::nullopt;
    constexpr bool     expectedInitialStateOfFirstAncillaryQubitOfUncalledModule = true;
    const std::string& firstAncillaryQubitOfUncalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInCalledModule, expectedInitialStateOfFirstAncillaryQubitOfUncalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInCalledModule, expectedInitialStateOfFirstAncillaryQubitOfUncalledModule, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubitOfUncalledModule = true;
    const std::string& secondAncillaryQubitOfUncalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInCalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfUncalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInCalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitOfUncalledModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfUncalledModuleInternalLabel, secondAncillaryQubitOfUncalledModuleInternalLabel));

    constexpr bool     expectedInitialStateOfThirdAncillaryQubitOfUncalledModule = false;
    const std::string& thirdAncillaryQubitOfUncalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitInCalledModule + 2U, expectedInitialStateOfThirdAncillaryQubitOfUncalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitInCalledModule + 2U, expectedInitialStateOfThirdAncillaryQubitOfUncalledModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, secondAncillaryQubitOfUncalledModuleInternalLabel, thirdAncillaryQubitOfUncalledModuleInternalLabel));

    // Local variable qubits and ancillary qubits of called module should shared inline stack
    std::string firstQubitOfUncalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModule), {0U}, 0U, firstQubitOfUncalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, thirdAncillaryQubitOfUncalledModuleInternalLabel, firstQubitOfUncalledModuleLocalVariableQubitLabel));

    // While the inline stacks on each level of the call stack should be shared, the inline stack of the main module should not be modified during the creation of the inline stack of the called module
    const AnnotatableQuantumComputation::InlinedQubitInformation* refetchedInlineInformationOfFirstQubitOfLocalVariableOfMainModule = this->annotatableQuantumComputation.getInliningInformationOfQubit(firstQubitOfMainModuleLocalVariableQubitLabel);
    ASSERT_THAT(refetchedInlineInformationOfFirstQubitOfLocalVariableOfMainModule, testing::NotNull());
    ASSERT_TRUE(refetchedInlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack.has_value());
    const QubitInliningStack::ptr& refetchedInlineStackOfFirstQubitOfLocalVariableOfMainModule = *inlineInformationOfFirstQubitOfLocalVariableOfMainModule->inlineStack;
    ASSERT_EQ(backupOfInlineStackOfFirstQubitOfLocalVariableOfMainModule, refetchedInlineStackOfFirstQubitOfLocalVariableOfMainModule) << "Expected inline stack of qubits of main module to not be modify by creation of inline information of qubits of called module";

    // Since we are reusing the same inline stack instance to define our expected data for both the main as well as the called module and since we are now validating the data of ancillary qubits of the main module
    // a pop to remove the inline stack entry for the called module is needed.
    ASSERT_TRUE(qubitInlineInformation.inlineStack.value()->pop());

    mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());

    // Only when a Call-/UncallStatement is synthesis will this information be set in the parent of the last added inline stack entry, since only one element is on the stack - no such information should be set.
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = std::nullopt;
    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubitAfterUncalledModule                                  = indexOfFirstAncillaryQubitInCalledModule + 3U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubitCreatedAfterSynthesisOfUncalledModule = true;
    const std::string&     firstAncillaryQubitCreatedAfterSynthesisOfUncalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitAfterUncalledModule, expectedInitialStateOfFirstAncillaryQubitCreatedAfterSynthesisOfUncalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitAfterUncalledModule, expectedInitialStateOfFirstAncillaryQubitCreatedAfterSynthesisOfUncalledModule, &qubitInlineInformation));

    constexpr bool     expectedInitialStateOfSecondAncillaryQubitCreatedAfterSynthesisOfUncalledModule = true;
    const std::string& secondAncillaryQubitCreatedAfterSynthesisOfUncalledModuleInternalLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(indexOfFirstAncillaryQubitAfterUncalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitCreatedAfterSynthesisOfUncalledModule);
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubitAfterUncalledModule + 1U, expectedInitialStateOfSecondAncillaryQubitCreatedAfterSynthesisOfUncalledModule, &qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitCreatedAfterSynthesisOfUncalledModuleInternalLabel, secondAncillaryQubitCreatedAfterSynthesisOfUncalledModuleInternalLabel));

    // The ancillary qubits created after the called module was synthesized should use the inline stack instance as the other qubits created for the main modules local variables and previously created ancillary qubits
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubitOfMainModuleInternalLabel, firstAncillaryQubitCreatedAfterSynthesisOfUncalledModuleInternalLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitsInformationFeatureActivatedForLargerThan1DVariable) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a[2](4), out b[1][2](2)) wire x[2][2](2), z(2) x[0][1] += x[1][0]", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 1U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 1U}, 1U, nullptr));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.inlineStack            = sharedInlineStack;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int indexOfFirstLocalVariableQubit = 12U;
    constexpr unsigned int variableXBitwidth              = 2U;

    constexpr unsigned int indexOfFirstQubitOfDimension00 = indexOfFirstLocalVariableQubit;
    constexpr unsigned int indexOfFirstQubitOfDimension01 = indexOfFirstQubitOfDimension00 + variableXBitwidth;
    constexpr unsigned int indexOfFirstQubitOfDimension10 = indexOfFirstQubitOfDimension01 + variableXBitwidth;
    constexpr unsigned int indexOfFirstQubitOfDimension11 = indexOfFirstQubitOfDimension10 + variableXBitwidth;

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U, 0U}, variableXBitwidth, indexOfFirstQubitOfDimension00, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U, 1U}, variableXBitwidth, indexOfFirstQubitOfDimension01, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {1U, 0U}, variableXBitwidth, indexOfFirstQubitOfDimension10, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {1U, 1U}, variableXBitwidth, indexOfFirstQubitOfDimension11, qubitInlineInformation));

    std::string internalQubitLabelForFirstQubitOfDimension00;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfDimension00), {0U, 0U}, 0U, internalQubitLabelForFirstQubitOfDimension00));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("x", {0U, 0U}, 0U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelForFirstQubitOfDimension00, &qubitInlineInformation));

    std::string internalQubitLabelForFirstQubitOfDimension01;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfDimension01), {0U, 1U}, 0U, internalQubitLabelForFirstQubitOfDimension01));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("x", {0U, 1U}, 0U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelForFirstQubitOfDimension01, &qubitInlineInformation));

    std::string internalQubitLabelForFirstQubitOfDimension10;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfDimension10), {1U, 0U}, 0U, internalQubitLabelForFirstQubitOfDimension10));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("x", {1U, 0U}, 0U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelForFirstQubitOfDimension10, &qubitInlineInformation));

    std::string internalQubitLabelForFirstQubitOfDimension11;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfDimension11), {1U, 1U}, 0U, internalQubitLabelForFirstQubitOfDimension11));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("x", {1U, 1U}, 0U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelForFirstQubitOfDimension11, &qubitInlineInformation));

    constexpr unsigned int indexOfFirstQubitOfLocal1DVariable = indexOfFirstQubitOfDimension11 + variableXBitwidth;
    std::string            internalQubitLabelOfFirstQubitOf1DVariable;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfLocal1DVariable), {0U}, 0U, internalQubitLabelOfFirstQubitOf1DVariable));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("z", {0U}, 0U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelOfFirstQubitOf1DVariable, &qubitInlineInformation));

    std::string internalQubitLabelOfSecondQubitOf1DVariable;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfLocal1DVariable), {0U}, 1U, internalQubitLabelOfSecondQubitOf1DVariable));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("z", {0U}, 1U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelOfSecondQubitOf1DVariable, &qubitInlineInformation));

    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabelOfFirstQubitOf1DVariable, internalQubitLabelOfSecondQubitOf1DVariable));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, internalQubitLabelOfFirstQubitOf1DVariable, internalQubitLabelForFirstQubitOfDimension00));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitsInformationFeatureActivatedDoesRecordDifferentInlineStacksForNameClashBetweenModuleLocalVariablesAndCalledModuleLocalVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());

    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = true;

    const auto calledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(calledModuleInlineStackEntry));

    constexpr unsigned int indexOfFirstQubitOfCalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfCalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables + bitwidthOfCalledModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstCalledModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondCalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModuleLocalVariables), {0U}, 0U, firstQubitOfFirstCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfCalledModuleLocalVariables + bitwidthOfCalledModuleLocalVariables), {0U}, 0U, firstQubitOfSecondCalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstCalledModuleLocalVariableQubitLabel, firstQubitOfSecondCalledModuleLocalVariableQubitLabel));

    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfFirstCalledModuleLocalVariableQubitLabel));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitsInformationFeatureActivatedDoesRecordDifferentInlineStacksForNameClashBetweenModuleLocalVariablesAndUncalledModuleLocalVariables) {
    Properties::ptr synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

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
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstMainModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondMainModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfFirstMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables), {0U}, 0U, firstQubitOfSecondMainModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfSecondMainModuleLocalVariableQubitLabel));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto* mainModuleInlineStackEntry = sharedInlineStack->getStackEntryAt(0);
    ASSERT_THAT(mainModuleInlineStackEntry, testing::NotNull());

    mainModuleInlineStackEntry->lineNumberOfCallOfTargetModule    = 1U;
    mainModuleInlineStackEntry->isTargetModuleAccessedViaCallStmt = false;

    const auto uncalledModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(uncalledModuleInlineStackEntry));

    constexpr unsigned int indexOfFirstQubitOfUncalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfUncalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables + bitwidthOfUncalledModuleLocalVariables, qubitInlineInformation));

    std::string firstQubitOfFirstUncalledModuleLocalVariableQubitLabel;
    std::string firstQubitOfSecondUncalledModuleLocalVariableQubitLabel;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModuleLocalVariables), {0U}, 0U, firstQubitOfFirstUncalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfUncalledModuleLocalVariables + bitwidthOfUncalledModuleLocalVariables), {0U}, 0U, firstQubitOfSecondUncalledModuleLocalVariableQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstUncalledModuleLocalVariableQubitLabel, firstQubitOfSecondUncalledModuleLocalVariableQubitLabel));

    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfFirstMainModuleLocalVariableQubitLabel, firstQubitOfFirstUncalledModuleLocalVariableQubitLabel));
}
// END tests for inlined qubit information behaviour with feature activated in synthesis settings

// BEGIN tests for inlined qubit information behaviour with feature deactivated in synthesis settings
TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfMainModuleParameters) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalMainModuleVariables) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main() wire a(4), b(4) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    constexpr unsigned int mainModuleLocalVariableBitwidth = 4U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "a", {0U}, mainModuleLocalVariableBitwidth, 0U, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "b", {0U}, mainModuleLocalVariableBitwidth, 4U, qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleParameters) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleVariables) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleLocalVariableBitwidth             = 2U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 8U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables + mainModuleLocalVariableBitwidth, qubitInlineInformation));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    constexpr unsigned int calledModuleLocalVariablesBitwidth            = 3U;
    constexpr unsigned int indexOfFirstQubitOfCalledModuleLocalVariables = indexOfFirstQubitOfMainModuleLocalVariables + (2 * mainModuleLocalVariableBitwidth);

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfCalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfCalledModuleLocalVariables + calledModuleLocalVariablesBitwidth, qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleParameters) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleVariables) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleLocalVariableBitwidth             = 2U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 8U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfMainModuleLocalVariables + mainModuleLocalVariableBitwidth, qubitInlineInformation));

    // Check inline information of local variables of called module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    constexpr unsigned int uncalledModuleLocalVariablesBitwidth            = 3U;
    constexpr unsigned int indexOfFirstQubitOfUncalledModuleLocalVariables = indexOfFirstQubitOfMainModuleLocalVariables + (2 * mainModuleLocalVariableBitwidth);

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfUncalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, mainModuleLocalVariableBitwidth, indexOfFirstQubitOfUncalledModuleLocalVariables + uncalledModuleLocalVariablesBitwidth, qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b; a += 2", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 8U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool expectedInitialStateOfSecondAncillaryQubit = true;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(2), out b(4)) a += (b.0:1 & b.2:3)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 6U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool expectedInitialStateOfSecondAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) call addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 4U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool expectedInitialStateOfSecondAncillaryQubit = true;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 6U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool expectedInitialStateOfSecondAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) uncall addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 4U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool expectedInitialStateOfSecondAncillaryQubit = true;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = std::nullopt;

    constexpr unsigned int indexOfFirstAncillaryQubit                = 6U;
    constexpr bool         expectedInitialStateOfFirstAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit, expectedInitialStateOfFirstAncillaryQubit, &qubitInlineInformation));

    constexpr bool expectedInitialStateOfSecondAncillaryQubit = false;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfAncillaryQubitMatches(this->annotatableQuantumComputation, indexOfFirstAncillaryQubit + 1U, expectedInitialStateOfSecondAncillaryQubit, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfModuleParametersUsedAsParametersInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    // Check inline information of local variables of called module in main module
    constexpr unsigned int indexOfFirstQubitOfCalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfCalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables + bitwidthOfCalledModuleLocalVariables, qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    // Check inline information of local variables of uncalled module in main module
    constexpr unsigned int indexOfFirstQubitOfUncalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfUncalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "s", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "t", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables + bitwidthOfUncalledModuleLocalVariables, qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitsInformationFeatureDeactivatedForLargerThan1DVariable) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a[2](4), out b[1][2](2)) wire x[2][2](2), z(2) x[0][1] += x[1][0]", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {1U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 1U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U, 1U}, 1U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int indexOfFirstLocalVariableQubit = 12U;
    constexpr unsigned int variableXBitwidth              = 2U;

    constexpr unsigned int indexOfFirstQubitOfDimension00 = indexOfFirstLocalVariableQubit;
    constexpr unsigned int indexOfFirstQubitOfDimension01 = indexOfFirstQubitOfDimension00 + variableXBitwidth;
    constexpr unsigned int indexOfFirstQubitOfDimension10 = indexOfFirstQubitOfDimension01 + variableXBitwidth;
    constexpr unsigned int indexOfFirstQubitOfDimension11 = indexOfFirstQubitOfDimension10 + variableXBitwidth;

    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U, 0U}, variableXBitwidth, indexOfFirstQubitOfDimension00, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U, 1U}, variableXBitwidth, indexOfFirstQubitOfDimension01, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {1U, 0U}, variableXBitwidth, indexOfFirstQubitOfDimension10, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {1U, 1U}, variableXBitwidth, indexOfFirstQubitOfDimension11, qubitInlineInformation));

    constexpr unsigned int indexOfFirstQubitOfLocal1DVariable = indexOfFirstQubitOfDimension11 + variableXBitwidth;
    std::string            internalQubitLabelOfFirstQubitOf1DVariable;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfLocal1DVariable), {0U}, 0U, internalQubitLabelOfFirstQubitOf1DVariable));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("z", {0U}, 0U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelOfFirstQubitOf1DVariable, &qubitInlineInformation));

    std::string internalQubitLabelOfSecondQubitOf1DVariable;
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(indexOfFirstQubitOfLocal1DVariable), {0U}, 1U, internalQubitLabelOfSecondQubitOf1DVariable));
    ASSERT_NO_FATAL_FAILURE(this->buildFullQubitLabel("z", {0U}, 1U, *qubitInlineInformation.userDeclaredQubitLabel));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, internalQubitLabelOfSecondQubitOf1DVariable, &qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitsInformationFeatureDeactivatedDoesHandleNameClashBetweenModuleLocalVariablesAndCalledModuleLocalVariablesCorrectly) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    // Check inline information of local variables of called module in main module
    constexpr unsigned int indexOfFirstQubitOfCalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfCalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfCalledModuleLocalVariables, indexOfFirstQubitOfCalledModuleLocalVariables + bitwidthOfCalledModuleLocalVariables, qubitInlineInformation));
}

TYPED_TEST_P(SynthesisQubitInlinineInformationTestsFixture, InlineQubitsInformationFeatureDeactivatedDoesHandleNameClashBetweenModuleLocalVariablesAndUncalledModuleLocalVariablesCorrectly) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "a", {0U}, 3U, nullptr));

    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 0U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 1U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 2U, nullptr));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationOfModuleParameterOrLocalVariableMatches(this->annotatableQuantumComputation, "b", {0U}, 3U, nullptr));

    AnnotatableQuantumComputation::InlinedQubitInformation qubitInlineInformation;
    qubitInlineInformation.userDeclaredQubitLabel = "";

    // Check inline information of local variables of main module
    constexpr unsigned int mainModuleParametersBitwidth                = 4U;
    constexpr unsigned int indexOfFirstQubitOfMainModuleLocalVariables = 2 * mainModuleParametersBitwidth;
    constexpr unsigned int bitwidthOfMainModuleLocalVariables          = 2;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfMainModuleLocalVariables, indexOfFirstQubitOfMainModuleLocalVariables + bitwidthOfMainModuleLocalVariables, qubitInlineInformation));

    // Check inline information of local variables of uncalled module in main module
    constexpr unsigned int indexOfFirstQubitOfUncalledModuleLocalVariables = (2 * mainModuleParametersBitwidth) + (2 * bitwidthOfMainModuleLocalVariables);
    constexpr unsigned int bitwidthOfUncalledModuleLocalVariables          = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "x", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables, qubitInlineInformation));
    ASSERT_NO_FATAL_FAILURE(this->assertNonAncillaryQubitInlineInformationForQubitsOfValueOfDimension(this->annotatableQuantumComputation, "y", {0U}, bitwidthOfUncalledModuleLocalVariables, indexOfFirstQubitOfUncalledModuleLocalVariables + bitwidthOfUncalledModuleLocalVariables, qubitInlineInformation));
}
// END tests for inlined qubit information behaviour with feature deactivated in synthesis settings

REGISTER_TYPED_TEST_SUITE_P(SynthesisQubitInlinineInformationTestsFixture,
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
                            InlineQubitInformationFeatureActivatedLocalModuleVariablesAndAncillaryQubitsOfCalledModuleOnSameDepthOfInlineStackShareSameInlineStack,
                            InlineQubitInformationFeatureActivatedLocalModuleVariablesAndAncillaryQubitsOfUncalledModuleOnSameDepthOfInlineStackShareSameInlineStack,
                            InlineQubitsInformationFeatureActivatedForLargerThan1DVariable,
                            InlineQubitsInformationFeatureActivatedDoesRecordDifferentInlineStacksForNameClashBetweenModuleLocalVariablesAndCalledModuleLocalVariables,
                            InlineQubitsInformationFeatureActivatedDoesRecordDifferentInlineStacksForNameClashBetweenModuleLocalVariablesAndUncalledModuleLocalVariables,

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
                            InlineQubitsInformationFeatureDeactivatedForLargerThan1DVariable,
                            InlineQubitsInformationFeatureDeactivatedDoesHandleNameClashBetweenModuleLocalVariablesAndCalledModuleLocalVariablesCorrectly,
                            InlineQubitsInformationFeatureDeactivatedDoesHandleNameClashBetweenModuleLocalVariablesAndUncalledModuleLocalVariablesCorrectly);

using SynthesizerTypes = testing::Types<CostAwareSynthesis, LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, SynthesisQubitInlinineInformationTestsFixture, SynthesizerTypes, );
