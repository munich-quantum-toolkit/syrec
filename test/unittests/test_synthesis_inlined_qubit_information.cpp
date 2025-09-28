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
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/qubit_inlining_stack.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/program.hpp"
#include "ir/Definitions.hpp"

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
    class SynthesisInlinedQubitInformationTestsFixture: public testing::Test {
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

        [[nodiscard]] static std::string buildExpectedFullQubitLabel(const std::string& variableIdentifier, const std::vector<unsigned int>& accessedValuePerDimension, const unsigned int accessedBitOfVariable) {
            std::string generatedQubitLabel = variableIdentifier;
            for (const auto valueOfDimension: accessedValuePerDimension) {
                generatedQubitLabel += "[" + std::to_string(valueOfDimension) + "]";
            }
            generatedQubitLabel += "." + std::to_string(accessedBitOfVariable);
            return generatedQubitLabel;
        }

        static void assertInlineStacksOfVariablesReferenceSameInstance(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit firstQubitToCheck, const qc::Qubit secondQubitToCheck) {
            assertConditionalInlineStackReferencesEqualityOfQubits(annotatableQuantumComputation, firstQubitToCheck, secondQubitToCheck, true);
        }

        static void assertInlineStacksOfVariablesDoNotReferenceSameInstance(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit firstQubitToCheck, const qc::Qubit secondQubitToCheck) {
            assertConditionalInlineStackReferencesEqualityOfQubits(annotatableQuantumComputation, firstQubitToCheck, secondQubitToCheck, false);
        }

        static void assertQubitInlineInformationMatchesExpectedOne(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit qubit, const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation>& expectedQubitInlineInformation) {
            const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> actualInlinedQubitInformation = annotatableQuantumComputation.getInlinedQubitInformation(qubit);
            if (expectedQubitInlineInformation.has_value()) {
                ASSERT_TRUE(actualInlinedQubitInformation.has_value()) << "Expected inline information for qubit " << std::to_string(qubit) << " to exist";
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatches(*expectedQubitInlineInformation, *actualInlinedQubitInformation)) << "Actual qubit inline information of qubit " << std::to_string(qubit) << " did not match expected one";
            } else {
                ASSERT_FALSE(actualInlinedQubitInformation.has_value()) << "No inline information for qubit " << std::to_string(qubit) << " should exist";
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

        static void assertConditionalInlineStackReferencesEqualityOfQubits(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit firstQubitToCheck, const qc::Qubit secondQubitToCheck, bool shouldReferencesBeEqual) {
            const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> inlinedQubitInformationOfFirstQubit = annotatableQuantumComputation.getInlinedQubitInformation(firstQubitToCheck);
            ASSERT_TRUE(inlinedQubitInformationOfFirstQubit.has_value()) << "Failed to fetch inlined qubit information of qubit " << std::to_string(firstQubitToCheck);

            const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> inlinedQubitInformationOfSecondQubit = annotatableQuantumComputation.getInlinedQubitInformation(secondQubitToCheck);
            ASSERT_TRUE(inlinedQubitInformationOfSecondQubit.has_value()) << "Failed to fetch inlined qubit information of qubit " << std::to_string(secondQubitToCheck);

            const std::optional<QubitInliningStack::ptr> firstQubitInlineStackInstance  = inlinedQubitInformationOfFirstQubit->inlineStack;
            const std::optional<QubitInliningStack::ptr> secondQubitInlineStackInstance = inlinedQubitInformationOfSecondQubit->inlineStack;

            if (firstQubitInlineStackInstance.has_value()) {
                ASSERT_TRUE(secondQubitInlineStackInstance.has_value()) << "Expected inline stack of qubit " << std::to_string(secondQubitToCheck) << " to have a value";
                if (shouldReferencesBeEqual) {
                    ASSERT_EQ(*firstQubitInlineStackInstance, *secondQubitInlineStackInstance) << "Expected inline stack instances of compared qubits to reference same instance";
                } else {
                    ASSERT_NE(*firstQubitInlineStackInstance, *secondQubitInlineStackInstance) << "Expected inline stack instances of compared qubits to not reference same instance";
                }

            } else {
                ASSERT_FALSE(secondQubitInlineStackInstance.has_value()) << "Expected inline stack of qubit " << std::to_string(secondQubitToCheck) << " not not have a value";
            }
        }

        static void assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit firstQubitOfElementOfVariable, const std::string& userDeclaredVariableIdentifier, const std::vector<unsigned>& accessedValuePerDimension, const std::size_t numQubitsToCheck, const std::optional<QubitInliningStack::ptr>& sharedQubitInlineStack) {
            for (qc::Qubit i = firstQubitOfElementOfVariable; i < firstQubitOfElementOfVariable + numQubitsToCheck; ++i) {
                const qc::Qubit relativeQubitIndexInElement        = i - firstQubitOfElementOfVariable;
                const auto      inlineInformationOfQubitOfVariable = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = buildExpectedFullQubitLabel(userDeclaredVariableIdentifier, accessedValuePerDimension, relativeQubitIndexInElement), .inlineStack = sharedQubitInlineStack});
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatchesExpectedOne(annotatableQuantumComputation, i, inlineInformationOfQubitOfVariable)) << "Qubit inline information for qubit at index " << std::to_string(relativeQubitIndexInElement) << " in element mismatch!";
                if (sharedQubitInlineStack.has_value() && i > firstQubitOfElementOfVariable) {
                    ASSERT_NO_FATAL_FAILURE(assertInlineStacksOfVariablesReferenceSameInstance(annotatableQuantumComputation, i, i - 1U));
                }
            }
        }
    };
} // namespace

TYPED_TEST_SUITE_P(SynthesisInlinedQubitInformationTestsFixture);

// BEGIN tests for inlined qubit information behaviour with feature activated in synthesis settings
TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfMainModuleParameters) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalMainModuleVariables) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main() wire a(4), b(4) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    const Module::ptr& sharedTargetModule = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(sharedTargetModule, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = sharedTargetModule})));

    constexpr qc::Qubit firstQubitOfLocalVariableX = 0U;
    constexpr unsigned  bitwidthOfLocalVariableX   = 4U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableX, "a", {0U}, bitwidthOfLocalVariableX, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableY = 4U;
    constexpr unsigned  bitwidthOfLocalVariableY   = 4U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableY, "b", {0U}, bitwidthOfLocalVariableY, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableX, firstQubitOfLocalVariableY));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfCalledModuleParameters) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0U; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfCalledModuleLocalVariables) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack          = std::make_shared<QubitInliningStack>();
    auto mainModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(mainModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariableYOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariableYOfMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableYOfMainModule));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto       sharedCallStackOfLocalVariablesInCalledModule = std::make_shared<QubitInliningStack>();
    const auto mainModuleInlineStackEntryInCalledModule      = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = mainModuleReference});
    const auto calledModuleInlineStackEntry                  = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInCalledModule->push(mainModuleInlineStackEntryInCalledModule));
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInCalledModule->push(calledModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfCalledModule = 12U;
    constexpr unsigned  bitwidthOfLocalVariableSOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, "s", {0U}, bitwidthOfLocalVariableSOfCalledModule, sharedCallStackOfLocalVariablesInCalledModule));

    constexpr qc::Qubit firstQubitOfLocalVariableTOfCalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariableTOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfCalledModule, "t", {0U}, bitwidthOfLocalVariableTOfCalledModule, sharedCallStackOfLocalVariablesInCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, firstQubitOfLocalVariableTOfCalledModule));

    // Inline stacks of local variables of called module should be equal
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, firstQubitOfLocalVariableTOfCalledModule));
    // But inline stack of local variables of main module and called module should not reference the same inline stack instance
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableSOfCalledModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesNotRecordInlineStackOfUncalledModuleParameters) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }
}
//
TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfUncalledLocalModuleVariables) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack          = std::make_shared<QubitInliningStack>();
    auto mainModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(mainModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariableYOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariableYOfMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableYOfMainModule));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto       sharedCallStackOfLocalVariablesInCalledModule = std::make_shared<QubitInliningStack>();
    const auto mainModuleInlineStackEntryInCalledModule      = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = mainModuleReference});
    const auto uncalledModuleInlineStackEntry                = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference});
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInCalledModule->push(mainModuleInlineStackEntryInCalledModule));
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInCalledModule->push(uncalledModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfUncalledModule = 12U;
    constexpr unsigned  bitwidthOfLocalVariableSOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, "s", {0U}, bitwidthOfLocalVariableSOfUncalledModule, sharedCallStackOfLocalVariablesInCalledModule));

    constexpr qc::Qubit firstQubitOfLocalVariableTOfUncalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariableTOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfUncalledModule, "t", {0U}, bitwidthOfLocalVariableTOfUncalledModule, sharedCallStackOfLocalVariablesInCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, firstQubitOfLocalVariableTOfUncalledModule));

    // Inline stacks of local variables of uncalled module should be equal
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, firstQubitOfLocalVariableTOfUncalledModule));
    // But inline stack of local variables of main module and uncalled module should not reference the same inline stack instance
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableSOfUncalledModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b; a += 2", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    constexpr qc::Qubit firstAncillaryQubit                           = 8U;
    constexpr qc::Qubit secondAncillaryQubit                          = 9U;
    const auto          sharedInlineQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubit, secondAncillaryQubit));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(2), out b(4)) a += (b.0:1 & b.2:3)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 6U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, mainModuleReference})));

    constexpr qc::Qubit firstAncillaryQubit                           = 6U;
    constexpr qc::Qubit secondAncillaryQubit                          = 7U;
    const auto          sharedInlineQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubit, secondAncillaryQubit));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) call addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("addWithConst");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry   = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = mainModuleReference});
    auto calledModuleCallStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});

    const auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    constexpr qc::Qubit firstAncillaryQubit                           = 4U;
    constexpr qc::Qubit secondAncillaryQubit                          = 5U;
    const auto          sharedInlineQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubit, secondAncillaryQubit));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 6U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry   = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = mainModuleReference});
    auto calledModuleCallStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});

    const auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    constexpr qc::Qubit firstAncillaryQubit                           = 6U;
    constexpr qc::Qubit secondAncillaryQubit                          = 7U;
    const auto          sharedInlineQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubit, secondAncillaryQubit));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) uncall addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("addWithConst");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry   = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = mainModuleReference});
    auto calledModuleCallStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});

    const auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(calledModuleCallStackEntry));

    constexpr qc::Qubit firstAncillaryQubit                           = 4U;
    constexpr qc::Qubit secondAncillaryQubit                          = 5U;
    const auto          sharedInlineQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubit, secondAncillaryQubit));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 6U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto mainModuleCallStackEntry     = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = mainModuleReference});
    auto uncalledModuleCallStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference});

    const auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(mainModuleCallStackEntry));
    ASSERT_TRUE(sharedInlineStack->push(uncalledModuleCallStackEntry));

    constexpr qc::Qubit firstAncillaryQubit                           = 6U;
    constexpr qc::Qubit secondAncillaryQubit                          = 7U;
    const auto          sharedInlineQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, sharedInlineQubitInformationOfAncillaryQubits));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstAncillaryQubit, secondAncillaryQubit));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack          = std::make_shared<QubitInliningStack>();
    auto mainModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(mainModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariableYOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariableYOfMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableYOfMainModule));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto       sharedCallStackOfLocalVariablesInUncalledModule = std::make_shared<QubitInliningStack>();
    const auto mainModuleInlineStackEntryInUncalledModule      = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = mainModuleReference});
    const auto uncalledModuleInlineStackEntry                  = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInUncalledModule->push(mainModuleInlineStackEntryInUncalledModule));
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInUncalledModule->push(uncalledModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfCalledModule = 12U;
    constexpr unsigned  bitwidthOfLocalVariableSOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, "s", {0U}, bitwidthOfLocalVariableSOfCalledModule, sharedCallStackOfLocalVariablesInUncalledModule));

    constexpr qc::Qubit firstQubitOfLocalVariableTOfCalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariableTOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfCalledModule, "t", {0U}, bitwidthOfLocalVariableTOfCalledModule, sharedCallStackOfLocalVariablesInUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, firstQubitOfLocalVariableTOfCalledModule));

    // Inline stacks of local variables of called module should be equal
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, firstQubitOfLocalVariableTOfCalledModule));
    // But inline stack of local variables of main module and called module should not reference the same inline stack instance
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableSOfCalledModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedDoesRecordInlineStackOfLocalModuleVariablesUsedAsParametersInUncalledModule) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack          = std::make_shared<QubitInliningStack>();
    auto mainModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(mainModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariableYOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariableYOfMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableYOfMainModule));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    auto       sharedCallStackOfLocalVariablesInUncalledModule = std::make_shared<QubitInliningStack>();
    const auto mainModuleInlineStackEntryInUncalledModule      = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = mainModuleReference});
    const auto uncalledModuleInlineStackEntry                  = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference});
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInUncalledModule->push(mainModuleInlineStackEntryInUncalledModule));
    ASSERT_TRUE(sharedCallStackOfLocalVariablesInUncalledModule->push(uncalledModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfUncalledModule = 12U;
    constexpr unsigned  bitwidthOfLocalVariableSOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, "s", {0U}, bitwidthOfLocalVariableSOfUncalledModule, sharedCallStackOfLocalVariablesInUncalledModule));

    constexpr qc::Qubit firstQubitOfLocalVariableTOfUncalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariableTOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfUncalledModule, "t", {0U}, bitwidthOfLocalVariableTOfUncalledModule, sharedCallStackOfLocalVariablesInUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, firstQubitOfLocalVariableTOfUncalledModule));

    // Inline stacks of local variables of uncalled module should be equal
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, firstQubitOfLocalVariableTOfUncalledModule));
    // But inline stack of local variables of main module and uncalled module should not reference the same inline stack instance
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableSOfUncalledModule));
}
//
TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedLocalModuleVariablesAndAncillaryQubitsOfCalledModuleOnSameDepthOfInlineStackShareSameInlineStack) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(in a(2)) wire s(3) s += 3 module main() wire x(2) x += 2; call add(x); x += 3", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack          = std::make_shared<QubitInliningStack>();
    auto mainModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(mainModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 0U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstAncillaryQubitInMainModule                     = 2U;
    const auto          qubitInlineInformationOfAncillaryQubitsOfMainModule = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInMainModule, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInMainModule + 1U, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstAncillaryQubitInMainModule));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    auto       sharedCallStackOfAncillaryQubitsInCalledModule = std::make_shared<QubitInliningStack>();
    const auto mainModuleInlineStackEntryInCalledModule       = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = mainModuleReference});
    const auto calledModuleInlineStackEntry                   = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference});
    ASSERT_TRUE(sharedCallStackOfAncillaryQubitsInCalledModule->push(mainModuleInlineStackEntryInCalledModule));
    ASSERT_TRUE(sharedCallStackOfAncillaryQubitsInCalledModule->push(calledModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfCalledModule = 4U;
    constexpr unsigned  bitwidthOfLocalVariableSOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, "s", {0U}, bitwidthOfLocalVariableSOfCalledModule, sharedCallStackOfAncillaryQubitsInCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableSOfCalledModule));

    constexpr qc::Qubit firstAncillaryQubitInCalledModule                     = 7U;
    const auto          qubitInlineInformationOfAncillaryQubitsOfCalledModule = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedCallStackOfAncillaryQubitsInCalledModule});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInCalledModule, qubitInlineInformationOfAncillaryQubitsOfCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInCalledModule + 1U, qubitInlineInformationOfAncillaryQubitsOfCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInCalledModule + 2U, qubitInlineInformationOfAncillaryQubitsOfCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, firstAncillaryQubitInCalledModule));

    constexpr qc::Qubit secondAncillaryQubitRangeInMainModule = 10U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubitRangeInMainModule, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubitRangeInMainModule + 1U, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, secondAncillaryQubitRangeInMainModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureActivatedLocalModuleVariablesAndAncillaryQubitsOfUncalledModuleOnSameDepthOfInlineStackShareSameInlineStack) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(in a(2)) wire s(3) s += 3 module main() wire x(2) x += 2; uncall add(x); x += 3", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack          = std::make_shared<QubitInliningStack>();
    auto mainModuleInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference});
    ASSERT_TRUE(sharedInlineStack->push(mainModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 0U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstAncillaryQubitInMainModule                     = 2U;
    const auto          qubitInlineInformationOfAncillaryQubitsOfMainModule = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInMainModule, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInMainModule + 1U, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstAncillaryQubitInMainModule));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& unccalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(unccalledModuleReference, testing::NotNull());

    auto       sharedCallStackOfAncillaryQubitsInUncalledModule = std::make_shared<QubitInliningStack>();
    const auto mainModuleInlineStackEntryInUncalledModule       = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = mainModuleReference});
    const auto uncalledModuleInlineStackEntry                   = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = unccalledModuleReference});
    ASSERT_TRUE(sharedCallStackOfAncillaryQubitsInUncalledModule->push(mainModuleInlineStackEntryInUncalledModule));
    ASSERT_TRUE(sharedCallStackOfAncillaryQubitsInUncalledModule->push(uncalledModuleInlineStackEntry));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfUncalledModule = 4U;
    constexpr unsigned  bitwidthOfLocalVariableSOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, "s", {0U}, bitwidthOfLocalVariableSOfUncalledModule, sharedCallStackOfAncillaryQubitsInUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableSOfUncalledModule));

    constexpr qc::Qubit firstAncillaryQubitInUncalledModule                     = 7U;
    const auto          qubitInlineInformationOfAncillaryQubitsOfUncalledModule = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedCallStackOfAncillaryQubitsInUncalledModule});
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInUncalledModule, qubitInlineInformationOfAncillaryQubitsOfUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInUncalledModule + 1U, qubitInlineInformationOfAncillaryQubitsOfUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubitInUncalledModule + 2U, qubitInlineInformationOfAncillaryQubitsOfUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, firstAncillaryQubitInUncalledModule));

    constexpr qc::Qubit secondAncillaryQubitRangeInMainModule = 10U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubitRangeInMainModule, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubitRangeInMainModule + 1U, qubitInlineInformationOfAncillaryQubitsOfMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, secondAncillaryQubitRangeInMainModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitsInformationFeatureActivatedForLargerThan1DVariable) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a[2](4), out b[1][2](2)) wire x[2][2](2), z(2) x[0][1] += x[1][0]", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    constexpr qc::Qubit numQubitsInParameterAOfMainModule = 8U;
    constexpr qc::Qubit numQubitsinParameterBOfMainModule = 4U;
    for (qc::Qubit qubit = 0; qubit < numQubitsInParameterAOfMainModule + numQubitsinParameterBOfMainModule; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference})));

    // Check inline information of local variables of main module
    constexpr qc::Qubit localVariableXBitwidth                                = 2U;
    constexpr qc::Qubit firstQubitOfOfDimension00InLocalVariableXInMainModule = 12U;
    constexpr qc::Qubit firstQubitOfOfDimension01InLocalVariableXInMainModule = firstQubitOfOfDimension00InLocalVariableXInMainModule + localVariableXBitwidth;
    constexpr qc::Qubit firstQubitOfOfDimension10InLocalVariableXInMainModule = firstQubitOfOfDimension01InLocalVariableXInMainModule + localVariableXBitwidth;
    constexpr qc::Qubit firstQubitOfOfDimension11InLocalVariableXInMainModule = firstQubitOfOfDimension10InLocalVariableXInMainModule + localVariableXBitwidth;

    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfOfDimension00InLocalVariableXInMainModule, "x", {0U, 0U}, localVariableXBitwidth, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfOfDimension01InLocalVariableXInMainModule, "x", {0U, 1U}, localVariableXBitwidth, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfOfDimension10InLocalVariableXInMainModule, "x", {1U, 0U}, localVariableXBitwidth, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfOfDimension11InLocalVariableXInMainModule, "x", {1U, 1U}, localVariableXBitwidth, sharedInlineStack));

    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfOfDimension00InLocalVariableXInMainModule, firstQubitOfOfDimension01InLocalVariableXInMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfOfDimension01InLocalVariableXInMainModule, firstQubitOfOfDimension10InLocalVariableXInMainModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfOfDimension10InLocalVariableXInMainModule, firstQubitOfOfDimension11InLocalVariableXInMainModule));

    constexpr qc::Qubit firstQubitOfLocalVariableZInMainModule = 20U;
    constexpr unsigned  bitwidthOfLocalVariableZInMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableZInMainModule, "z", {0U}, bitwidthOfLocalVariableZInMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfOfDimension00InLocalVariableXInMainModule, firstQubitOfLocalVariableZInMainModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitsInformationFeatureActivatedDoesRecordDifferentInlineStacksForNameClashBetweenModuleLocalVariablesAndCalledModuleLocalVariables) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    constexpr qc::Qubit numQubitsInParameterAOfMainModule = 4U;
    constexpr qc::Qubit numQubitsinParameterBOfMainModule = 4U;
    for (qc::Qubit qubit = 0; qubit < numQubitsInParameterAOfMainModule + numQubitsinParameterBOfMainModule; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference})));

    // Check inline information of local variables of main module
    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariableYOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariableYOfMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableYOfMainModule));

    // Check inline information of local variables of called module in main module
    const Module::ptr& calledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(calledModuleReference, testing::NotNull());

    const auto inlineStackOfLocalVariablesOfCalledModule = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackOfLocalVariablesOfCalledModule->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = mainModuleReference})));
    ASSERT_TRUE(inlineStackOfLocalVariablesOfCalledModule->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = calledModuleReference})));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfCalledModule = 12U;
    constexpr unsigned  bitwidthOfLocalVariableXOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfCalledModule, "x", {0U}, bitwidthOfLocalVariableXOfCalledModule, inlineStackOfLocalVariablesOfCalledModule));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfCalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariableYOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfCalledModule, "y", {0U}, bitwidthOfLocalVariableYOfCalledModule, inlineStackOfLocalVariablesOfCalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfCalledModule, firstQubitOfLocalVariableYOfCalledModule));

    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableXOfCalledModule));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitsInformationFeatureActivatedDoesRecordDifferentInlineStacksForNameClashBetweenModuleLocalVariablesAndUncalledModuleLocalVariables) {
    const auto synthesisSettings = std::make_shared<Properties>();
    synthesisSettings->set(SyrecSynthesis::GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, true);
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation, synthesisSettings));

    constexpr qc::Qubit numQubitsInParameterAOfMainModule = 4U;
    constexpr qc::Qubit numQubitsinParameterBOfMainModule = 4U;
    for (qc::Qubit qubit = 0; qubit < numQubitsInParameterAOfMainModule + numQubitsinParameterBOfMainModule; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    const Module::ptr& mainModuleReference = this->syrecProgramInstance.findModule("main");
    ASSERT_THAT(mainModuleReference, testing::NotNull());

    auto sharedInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(sharedInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = mainModuleReference})));

    // Check inline information of local variables of main module
    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr unsigned  bitwidthOfLocalVariableXOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariableXOfMainModule, sharedInlineStack));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariableYOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariableYOfMainModule, sharedInlineStack));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableYOfMainModule));

    // Check inline information of local variables of uncalled module in main module
    const Module::ptr& uncalledModuleReference = this->syrecProgramInstance.findModule("add");
    ASSERT_THAT(uncalledModuleReference, testing::NotNull());

    const auto inlineStackOfLocalVariablesOfUncalledModule = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackOfLocalVariablesOfUncalledModule->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = mainModuleReference})));
    ASSERT_TRUE(inlineStackOfLocalVariablesOfUncalledModule->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncalledModuleReference})));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfUncalledModule = 12U;
    constexpr unsigned  bitwidthOfLocalVariableXOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfUncalledModule, "x", {0U}, bitwidthOfLocalVariableXOfUncalledModule, inlineStackOfLocalVariablesOfUncalledModule));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfUncalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariableYOfUncalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfUncalledModule, "y", {0U}, bitwidthOfLocalVariableYOfUncalledModule, inlineStackOfLocalVariablesOfUncalledModule));
    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfUncalledModule, firstQubitOfLocalVariableYOfUncalledModule));

    ASSERT_NO_FATAL_FAILURE(this->assertInlineStacksOfVariablesDoNotReferenceSameInstance(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, firstQubitOfLocalVariableXOfUncalledModule));
}
// END tests for inlined qubit information behaviour with feature activated in synthesis settings

// // BEGIN tests for inlined qubit information behaviour with feature deactivated in synthesis settings
TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfMainModuleParameters) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalMainModuleVariables) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main() wire a(4), b(4) a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    constexpr qc::Qubit    firstQubitOfParameterAOfMainModule = 0U;
    constexpr qc::Qubit    firstQubitOfParameterBOfMainModule = 4U;
    constexpr unsigned int mainModuleLocalVariableBitwidth    = 4U;

    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfParameterAOfMainModule, "a", {0U}, mainModuleLocalVariableBitwidth, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfParameterBOfMainModule, "b", {0U}, mainModuleLocalVariableBitwidth, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleParameters) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfCalledModuleVariables) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  localVariableBitwidthOfMainModule      = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, localVariableBitwidthOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, localVariableBitwidthOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfCalledModule = 12U;
    constexpr qc::Qubit firstQubitOfLocalVariableTOfCalledModule = 15U;
    constexpr unsigned  localVariableBitwidthOfCalledModule      = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, "s", {0U}, localVariableBitwidthOfCalledModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfCalledModule, "t", {0U}, localVariableBitwidthOfCalledModule, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleParameters) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) a += b module main(inout a(4), out b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfUncalledModuleVariables) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(4), in b(4)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  localVariableBitwidthOfMainModule      = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, localVariableBitwidthOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, localVariableBitwidthOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfUncalledModule = 12U;
    constexpr qc::Qubit firstQubitOfLocalVariableTOfUncalledModule = 15U;
    constexpr unsigned  localVariableBitwidthOfUncalledModule      = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfUncalledModule, "s", {0U}, localVariableBitwidthOfUncalledModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfUncalledModule, "t", {0U}, localVariableBitwidthOfUncalledModule, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInMainModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(4), out b(4)) a += b; a += 2", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstAncillaryQubit  = 8U;
    constexpr qc::Qubit secondAncillaryQubit = 9U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInMainModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a(2), out b(4)) a += (b.0:1 & b.2:3)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 6U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstAncillaryQubit  = 6U;
    constexpr qc::Qubit secondAncillaryQubit = 7U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) call addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstAncillaryQubit  = 4U;
    constexpr qc::Qubit secondAncillaryQubit = 5U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) call add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 6U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstAncillaryQubit  = 6U;
    constexpr qc::Qubit secondAncillaryQubit = 7U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntegerConstantsInUncalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module addWithConst(inout a(2)) a += 2 module main(inout a(2), in b(2)) uncall addWithConst(a); a += b", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstAncillaryQubit  = 4U;
    constexpr qc::Qubit secondAncillaryQubit = 5U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfAncillaryQubitsCreatedForIntermediateResultsInUncalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(4)) a += (b.0:1 & b.2:3) module main(inout a(2), in b(4)) uncall add(a, b)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 6U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstAncillaryQubit  = 6U;
    constexpr qc::Qubit secondAncillaryQubit = 7U;
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, firstAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, secondAncillaryQubit, AnnotatableQuantumComputation::InlinedQubitInformation()));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfModuleParametersUsedAsParametersInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariablesOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariablesOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariablesOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfCalledModule = 12U;
    constexpr qc::Qubit firstQubitOfLocalVariableTOfCalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariablesOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, "s", {0U}, bitwidthOfLocalVariablesOfCalledModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfCalledModule, "t", {0U}, bitwidthOfLocalVariablesOfCalledModule, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitInformationFeatureDeactivatedDoesNotRecordInlineStackOfLocalModuleVariablesUsedAsParametersInCalledModule) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire s(3), t(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    for (qc::Qubit qubit = 0; qubit < 8U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule = 8U;
    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule = 10U;
    constexpr unsigned  bitwidthOfLocalVariablesOfMainModule   = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfLocalVariablesOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfLocalVariablesOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableSOfCalledModule = 12U;
    constexpr qc::Qubit firstQubitOfLocalVariableTOfCalledModule = 15U;
    constexpr unsigned  bitwidthOfLocalVariablesOfCalledModule   = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableSOfCalledModule, "s", {0U}, bitwidthOfLocalVariablesOfCalledModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableTOfCalledModule, "t", {0U}, bitwidthOfLocalVariablesOfCalledModule, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitsInformationFeatureDeactivatedForLargerThan1DVariable) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module main(inout a[2](4), out b[1][2](2)) wire x[2][2](2), z(2) x[0][1] += x[1][0]", this->syrecProgramInstance, this->annotatableQuantumComputation));

    constexpr unsigned numQubitsInParameterAOfMainModule = 8U;
    constexpr unsigned numQubitsInParameterBOfMainModule = 4U;
    for (qc::Qubit qubit = 0; qubit < numQubitsInParameterAOfMainModule + numQubitsInParameterBOfMainModule; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr unsigned  bitwidthOfElementsInLocalVariableXOfMainModule      = 2U;
    constexpr qc::Qubit firstQubitOfDimension00OfLocalVariableXOfMainModule = 12U;
    constexpr qc::Qubit firstQubitOfDimension01OfLocalVariableXOfMainModule = 14U;
    constexpr qc::Qubit firstQubitOfDimension10OfLocalVariableXOfMainModule = 16U;
    constexpr qc::Qubit firstQubitOfDimension11OfLocalVariableXOfMainModule = 18U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfDimension00OfLocalVariableXOfMainModule, "x", {0U, 0U}, bitwidthOfElementsInLocalVariableXOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfDimension01OfLocalVariableXOfMainModule, "x", {0U, 1U}, bitwidthOfElementsInLocalVariableXOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfDimension10OfLocalVariableXOfMainModule, "x", {1U, 0U}, bitwidthOfElementsInLocalVariableXOfMainModule, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfDimension11OfLocalVariableXOfMainModule, "x", {1U, 1U}, bitwidthOfElementsInLocalVariableXOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableZOfMainModule        = 20U;
    constexpr unsigned  bitwidthOfElementInLocalVariableZOfMainModule = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableZOfMainModule, "z", {0U}, bitwidthOfElementInLocalVariableZOfMainModule, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitsInformationFeatureDeactivatedDoesHandleNameClashBetweenModuleLocalVariablesAndCalledModuleLocalVariablesCorrectly) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) call add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    constexpr unsigned numQubitsInParameterAOfMainModule = 4U;
    constexpr unsigned numQubitsInParameterBOfMainModule = 4U;
    for (qc::Qubit qubit = 0; qubit < numQubitsInParameterAOfMainModule + numQubitsInParameterBOfMainModule; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule        = 8U;
    constexpr unsigned  bitwidthOfElementInLocalVariableXOfMainModule = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfElementInLocalVariableXOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule        = 10U;
    constexpr unsigned  bitwidthOfElementInLocalVariableYOfMainModule = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfElementInLocalVariableYOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfCalledModule        = 12U;
    constexpr unsigned  bitwidthOfElementInLocalVariableXOfCalledModule = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfCalledModule, "x", {0U}, bitwidthOfElementInLocalVariableXOfCalledModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfCalledModule        = 15U;
    constexpr unsigned  bitwidthOfElementInLocalVariableYOfCalledModule = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfCalledModule, "y", {0U}, bitwidthOfElementInLocalVariableYOfCalledModule, std::nullopt));
}

TYPED_TEST_P(SynthesisInlinedQubitInformationTestsFixture, InlineQubitsInformationFeatureDeactivatedDoesHandleNameClashBetweenModuleLocalVariablesAndUncalledModuleLocalVariablesCorrectly) {
    ASSERT_NO_FATAL_FAILURE(this->parseAndSynthesisProgramFromString("module add(inout a(2), in b(2)) wire x(3), y(3) a += b module main(inout a(4), out b(4)) wire x(2), y(2) uncall add(x, y)", this->syrecProgramInstance, this->annotatableQuantumComputation));

    constexpr unsigned numQubitsInParameterAOfMainModule = 4U;
    constexpr unsigned numQubitsInParameterBOfMainModule = 4U;
    for (qc::Qubit qubit = 0; qubit < numQubitsInParameterAOfMainModule + numQubitsInParameterBOfMainModule; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(this->assertQubitInlineInformationMatchesExpectedOne(this->annotatableQuantumComputation, qubit, std::nullopt));
    }

    constexpr qc::Qubit firstQubitOfLocalVariableXOfMainModule        = 8U;
    constexpr unsigned  bitwidthOfElementInLocalVariableXOfMainModule = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfMainModule, "x", {0U}, bitwidthOfElementInLocalVariableXOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfMainModule        = 10U;
    constexpr unsigned  bitwidthOfElementInLocalVariableYOfMainModule = 2U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfMainModule, "y", {0U}, bitwidthOfElementInLocalVariableYOfMainModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableXOfUncalledModule        = 12U;
    constexpr unsigned  bitwidthOfElementInLocalVariableXOfUncalledModule = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableXOfUncalledModule, "x", {0U}, bitwidthOfElementInLocalVariableXOfUncalledModule, std::nullopt));

    constexpr qc::Qubit firstQubitOfLocalVariableYOfCUncalledModule       = 15U;
    constexpr unsigned  bitwidthOfElementInLocalVariableYOfUncalledModule = 3U;
    ASSERT_NO_FATAL_FAILURE(this->assertInlinedQubitInformationOfQubitsOfElementOfVariableAreTheSameExceptForUserDeclaredQubitLabel(this->annotatableQuantumComputation, firstQubitOfLocalVariableYOfCUncalledModule, "y", {0U}, bitwidthOfElementInLocalVariableYOfUncalledModule, std::nullopt));
}
// END tests for inlined qubit information behaviour with feature deactivated in synthesis settings

REGISTER_TYPED_TEST_SUITE_P(SynthesisInlinedQubitInformationTestsFixture,
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
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, SynthesisInlinedQubitInformationTestsFixture, SynthesizerTypes, );
