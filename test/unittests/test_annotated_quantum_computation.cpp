/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/annotatable_quantum_computation.hpp"
#include "core/qubit_inlining_stack.hpp"
#include "core/syrec/module.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/Register.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// The current tests do not cover the following functionality:
// * set- and get constant/garbage/input/output lines
// * adding and getting lines of the circuit
// * the stringification of the supported gate types
// ** (Gate::toQasm() will generate outputs that are not supported by the QASM standard without extra definitions and only supported by MQT::Core)
// * the stringification of the whole circuit to either a string or file

using namespace syrec;

const static std::string DEFAULT_QUANTUM_REGISTER_LABEL = "__defaultReg";

class AnnotatableQuantumComputationTestsFixture: public testing::Test {
protected:
    std::unique_ptr<AnnotatableQuantumComputation> annotatedQuantumComputation;

    enum ExpectedQubitFlags : std::uint8_t {
        QubitShouldBeDataQubit                  = 0,
        QubitShouldBeGarbage                    = 1,
        QubitShouldBeAncillary                  = 2,
        InlineQubitInformationShouldBeFetchable = 4
    };

    [[nodiscard]] constexpr friend bool operator&(const ExpectedQubitFlags aggregateQubitFlags, const ExpectedQubitFlags flagValueToExtract) noexcept {
        return (static_cast<std::uint8_t>(aggregateQubitFlags) & static_cast<std::uint8_t>(flagValueToExtract)) != 0;
    }

    [[nodiscard]] constexpr friend ExpectedQubitFlags operator|(const ExpectedQubitFlags aggregateQubitFlags, const ExpectedQubitFlags flagToAddToAggregateState) noexcept {
        return static_cast<ExpectedQubitFlags>(static_cast<std::uint8_t>(aggregateQubitFlags) | static_cast<std::uint8_t>(flagToAddToAggregateState));
    }

    void SetUp() override {
        annotatedQuantumComputation = std::make_unique<AnnotatableQuantumComputation>(true);
    }

    [[nodiscard]] static constexpr ExpectedQubitFlags getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(const AnnotatableQuantumComputation::QubitType qubitType) {
        switch (qubitType) {
            case AnnotatableQuantumComputation::QubitType::Data:
                return QubitShouldBeDataQubit;
            case AnnotatableQuantumComputation::QubitType::Ancillary:
            case AnnotatableQuantumComputation::QubitType::Garbage:
                return QubitShouldBeGarbage;
            default:
                // This assert should help to catch unhandled qubit types (in debug builds) but will not be triggered in release builds.
                assert(false && "Unhandled qubit type detected");
                // We add a return value so that the compiler is happy.
                return QubitShouldBeDataQubit;
        }
    }

    static void assertExpectedQubitFlagsMatchForQubitRange(const AnnotatableQuantumComputation& annotatedQuantumComputation, const AnnotatableQuantumComputation::QubitIndexRange qubitIndexRangeToCheck, const ExpectedQubitFlags expectedSharedQubitFlags) {
        const bool shouldQubitBeGarbage               = expectedSharedQubitFlags & QubitShouldBeGarbage;
        const bool shouldQubitBeAncillary             = expectedSharedQubitFlags & QubitShouldBeAncillary;
        const bool shouldInlineInformationBeFetchable = expectedSharedQubitFlags & InlineQubitInformationShouldBeFetchable;

        for (qc::Qubit qubit = qubitIndexRangeToCheck.firstQubitIndex; qubit <= qubitIndexRangeToCheck.lastQubitIndex; ++qubit) {
            ASSERT_EQ(shouldQubitBeGarbage, annotatedQuantumComputation.logicalQubitIsGarbage(qubit)) << "Expected qubit " << std::to_string(qubit) << " to be marked as garbage qubit: " << shouldQubitBeGarbage;
            ASSERT_EQ(shouldQubitBeAncillary, annotatedQuantumComputation.logicalQubitIsAncillary(qubit)) << "Expected qubit " << std::to_string(qubit) << " to be marked as ancillary qubit: " << shouldQubitBeAncillary;
            ASSERT_EQ(shouldInlineInformationBeFetchable, annotatedQuantumComputation.getInlinedQubitInformation(qubit).has_value()) << "Expected inline information of qubit " << std::to_string(qubit) << " to be fetchable: " << shouldInlineInformationBeFetchable;
        }
    }

    static void assertThatOperationsOfQuantumComputationAreEqualToSequence(const AnnotatableQuantumComputation& annotatedQuantumComputation, const std::vector<std::unique_ptr<qc::Operation>>& expectedQuantumOperations) {
        const std::size_t expectedNumOperations      = expectedQuantumOperations.size();
        const std::size_t actualNumQuantumOperations = annotatedQuantumComputation.getNindividualOps();
        ASSERT_EQ(expectedNumOperations, actualNumQuantumOperations) << "Expected that annotated quantum computation contains " << std::to_string(expectedNumOperations) << " quantum operations but actually contained " << std::to_string(actualNumQuantumOperations) << " quantum operations";

        auto expectedQuantumOperationsIterator = expectedQuantumOperations.begin();
        for (std::size_t i = 0; i < expectedNumOperations; ++i) {
            auto const* actualQuantumOperation = annotatedQuantumComputation.getQuantumOperation(i);
            ASSERT_THAT(actualQuantumOperation, testing::NotNull());
            const auto& expectedQuantumOperation = *expectedQuantumOperationsIterator;
            ASSERT_THAT(expectedQuantumOperation, testing::NotNull());
            ASSERT_TRUE(expectedQuantumOperation->equals(*actualQuantumOperation));
            ++expectedQuantumOperationsIterator; // NOLINT (cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

    static void assertThatAnnotationsOfQuantumOperationAreEqualTo(const AnnotatableQuantumComputation& annotatedQuantumComputation, const std::size_t indexOfQuantumOperationInQuantumComputation, const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup& expectedAnnotationsOfQuantumComputation) {
        ASSERT_TRUE(indexOfQuantumOperationInQuantumComputation < annotatedQuantumComputation.getNindividualOps());
        const auto& actualAnnotationsOfQuantumOperation = annotatedQuantumComputation.getAnnotationsOfQuantumOperation(indexOfQuantumOperationInQuantumComputation);
        ASSERT_EQ(expectedAnnotationsOfQuantumComputation.size(), actualAnnotationsOfQuantumOperation.size()) << "Mismatch between the number of annotations of the quantum operation at index " << std::to_string(indexOfQuantumOperationInQuantumComputation) << " of the quantum computation";

        for (const auto& [expectedAnnotationKey, expectedAnnotationValue]: expectedAnnotationsOfQuantumComputation) {
            const auto& actualMatchingEntryForAnnotationKey = actualAnnotationsOfQuantumOperation.find(expectedAnnotationKey);
            ASSERT_TRUE(actualMatchingEntryForAnnotationKey != actualAnnotationsOfQuantumOperation.cend()) << "Expected annotation with key '" << expectedAnnotationKey << "' was not found";

            const auto& actualAnnotationValue = actualMatchingEntryForAnnotationKey->second;
            ASSERT_EQ(expectedAnnotationValue, actualAnnotationValue) << "Value for annotation with key '" << expectedAnnotationKey << "' did not match, expected: " << expectedAnnotationValue << " but was actually " << actualAnnotationValue;
        }
    }

    static void assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(AnnotatableQuantumComputation& annotatableQuantumComputation, const AnnotatableQuantumComputation::QubitType typeOfQubitsToBeGeneratedForVariable, const std::string& expectedQuantumRegisterLabel, const AnnotatableQuantumComputation::QubitIndexRange expectedQubitRangeOfRegister, const AnnotatableQuantumComputation::AssociatedVariableLayoutInformation& associatedVariableLayoutInformation, const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation>& optionalSharedInlinedQubitInformation = std::nullopt, const bool forceRecordingOfQubitInlineInformation = false) {
        ASSERT_NE(expectedQuantumRegisterLabel, DEFAULT_QUANTUM_REGISTER_LABEL) << "Please do not use the default quantum register label set in the annotatable quantum computation tests";

        const auto numQubitsPriorToAdditionOfQuantumRegister       = annotatableQuantumComputation.getNqubits();
        const auto numQubitsInFutureQuantumComputation             = (expectedQubitRangeOfRegister.lastQubitIndex - expectedQubitRangeOfRegister.firstQubitIndex) + 1U;
        const auto expectedNumQubitsAfterAdditionOfQuantumRegister = numQubitsPriorToAdditionOfQuantumRegister + numQubitsInFutureQuantumComputation;

        std::optional<qc::Qubit> actualFirstQubitOfQuantumRegister;
        ASSERT_NO_FATAL_FAILURE(actualFirstQubitOfQuantumRegister = annotatableQuantumComputation.addQuantumRegisterForSyrecVariable(typeOfQubitsToBeGeneratedForVariable, expectedQuantumRegisterLabel, associatedVariableLayoutInformation, optionalSharedInlinedQubitInformation, forceRecordingOfQubitInlineInformation));
        ASSERT_TRUE(actualFirstQubitOfQuantumRegister.has_value()) << "Failed to create quantum register " << expectedQuantumRegisterLabel << " for variable";
        ASSERT_EQ(expectedQubitRangeOfRegister.firstQubitIndex, actualFirstQubitOfQuantumRegister.value()) << "Expected first qubit of quantum register " << expectedQuantumRegisterLabel << " should be equal to " << std::to_string(expectedQubitRangeOfRegister.firstQubitIndex) << " but was actually " << std::to_string(actualFirstQubitOfQuantumRegister.value());

        ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(annotatableQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfRegister));
        ASSERT_EQ(annotatableQuantumComputation.getNqubits(), expectedNumQubitsAfterAdditionOfQuantumRegister) << "Total number of qubits in quantum computation after addition of quantum register did not match";
    }

    static void assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& expectedQuantumRegisterLabel, const AnnotatableQuantumComputation::QubitIndexRange expectedQubitRangeOfRegister, const std::vector<bool>& expectedInitialValuesOfAncillaryQubits, const AnnotatableQuantumComputation::InlinedQubitInformation& sharedInlineQubitInformation) {
        ASSERT_NE(expectedQuantumRegisterLabel, DEFAULT_QUANTUM_REGISTER_LABEL) << "Please do not use the default quantum register label set in the annotatable quantum computation tests";

        const auto numQubitsPriorToAdditionOfQuantumRegister = annotatableQuantumComputation.getNqubits();
        const auto numQubitsInToBeAddedQuantumRegister       = (expectedQubitRangeOfRegister.lastQubitIndex - expectedQubitRangeOfRegister.firstQubitIndex) + 1U;
        ASSERT_EQ(numQubitsInToBeAddedQuantumRegister, expectedInitialValuesOfAncillaryQubits.size()) << "The number of initial states must match the number of qubits in the to be added ancillary quantum register";

        const auto        expectedNumQubitsAfterAdditionOfQuantumRegister            = numQubitsPriorToAdditionOfQuantumRegister + numQubitsInToBeAddedQuantumRegister;
        const std::size_t numQuantumOperationsPriorToAdditionOfQuantumRegister       = annotatableQuantumComputation.getNops();
        const std::size_t numAncillaryQubitsToBeInitializedToOne                     = static_cast<std::size_t>(std::ranges::count(expectedInitialValuesOfAncillaryQubits, true));
        const std::size_t expectedNumQuantumOperationsAfterAdditionOfQuantumRegister = annotatableQuantumComputation.getNops() + numAncillaryQubitsToBeInitializedToOne;

        std::optional<qc::Qubit> actualFirstQubitOfQuantumRegister;
        ASSERT_NO_FATAL_FAILURE(actualFirstQubitOfQuantumRegister = annotatableQuantumComputation.addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne(expectedQuantumRegisterLabel, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
        ASSERT_TRUE(actualFirstQubitOfQuantumRegister.has_value()) << "Failed to create quantum register " << expectedQuantumRegisterLabel << " for variable";
        ASSERT_EQ(expectedQubitRangeOfRegister.firstQubitIndex, actualFirstQubitOfQuantumRegister.value()) << "Expected first qubit of quantum register " << expectedQuantumRegisterLabel << " should be equal to " << std::to_string(expectedQubitRangeOfRegister.firstQubitIndex) << " but was actually " << std::to_string(actualFirstQubitOfQuantumRegister.value());

        ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(annotatableQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfRegister));
        ASSERT_EQ(annotatableQuantumComputation.getNqubits(), expectedNumQubitsAfterAdditionOfQuantumRegister) << "Total number of qubits in quantum computation after addition of quantum register did not match";

        ASSERT_EQ(annotatableQuantumComputation.getNops(), expectedNumQuantumOperationsAfterAdditionOfQuantumRegister) << "Expected number of quantum operations after appending ancillary qubits to ancillary quantum register did not match";
        if (numAncillaryQubitsToBeInitializedToOne > 0) {
            ASSERT_NO_FATAL_FAILURE(assertGatesForInitializationOfAncillaryQubitsSetToOneAddedToQuantumComputation(annotatableQuantumComputation, expectedQubitRangeOfRegister.firstQubitIndex, expectedInitialValuesOfAncillaryQubits, numQuantumOperationsPriorToAdditionOfQuantumRegister, numAncillaryQubitsToBeInitializedToOne));
        }
    }

    static void assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& labelOfAppendedToQuantumRegister, const std::vector<bool>& expectedInitialValuesOfAncillaryQubits, const AnnotatableQuantumComputation::InlinedQubitInformation& sharedInlineQubitInformation, const qc::Qubit expectedFirstGeneratedAncillaryQubit, const AnnotatableQuantumComputation::QubitIndexRange expectedQubitRangeOfRegisterAfterQubitsWereAppended) {
        const auto numQuantumRegistersPriorToAdditionOfRegister = annotatableQuantumComputation.getQuantumRegisters().size();

        const qc::QuantumRegisterMap& quantumRegisterMap = annotatableQuantumComputation.getQuantumRegisters();
        ASSERT_TRUE(quantumRegisterMap.contains(labelOfAppendedToQuantumRegister)) << "Quantum computation did not contain a quantum register with an identifier equal to " << labelOfAppendedToQuantumRegister;
        const qc::QuantumRegister& actualQuantumRegister = quantumRegisterMap.at(labelOfAppendedToQuantumRegister);
        ASSERT_EQ(actualQuantumRegister.getStartIndex(), expectedQubitRangeOfRegisterAfterQubitsWereAppended.firstQubitIndex) << "Expected first qubit of quantum register did not match its actual value";

        const auto numQubitsPriorToAdditionOfQuantumRegister = annotatableQuantumComputation.getNqubits();
        const auto numQubitsToBeAdded                        = (expectedQubitRangeOfRegisterAfterQubitsWereAppended.lastQubitIndex - expectedFirstGeneratedAncillaryQubit) + 1U;
        ASSERT_EQ(numQubitsToBeAdded, expectedInitialValuesOfAncillaryQubits.size()) << "The number of initial states must match the number of qubits in the to be added ancillary quantum register";

        const auto expectedNumQubitsAfterAdditionOfQuantumRegister = numQubitsPriorToAdditionOfQuantumRegister + numQubitsToBeAdded;

        const std::size_t numQuantumOperationsPriorToAdditionOfQuantumRegister       = annotatableQuantumComputation.getNops();
        const std::size_t numAncillaryQubitsToBeInitializedToOne                     = static_cast<std::size_t>(std::ranges::count(expectedInitialValuesOfAncillaryQubits, true));
        const std::size_t expectedNumQuantumOperationsAfterAdditionOfQuantumRegister = annotatableQuantumComputation.getNops() + numAncillaryQubitsToBeInitializedToOne;

        std::optional<qc::Qubit> actualFirstQubitOfQuantumRegister;
        ASSERT_NO_FATAL_FAILURE(actualFirstQubitOfQuantumRegister = annotatableQuantumComputation.addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne(DEFAULT_QUANTUM_REGISTER_LABEL, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
        ASSERT_TRUE(actualFirstQubitOfQuantumRegister.has_value()) << "Failed to append ancillary qubits to adjacent ancillary quantum register " << labelOfAppendedToQuantumRegister;
        ASSERT_EQ(expectedFirstGeneratedAncillaryQubit, actualFirstQubitOfQuantumRegister.value()) << "Expected first ancillary qubit index should be equal to " << std::to_string(expectedQubitRangeOfRegisterAfterQubitsWereAppended.firstQubitIndex) << " but was actually " << std::to_string(actualFirstQubitOfQuantumRegister.value());

        ASSERT_EQ(annotatableQuantumComputation.getQuantumRegisters().size(), numQuantumRegistersPriorToAdditionOfRegister) << "Expected ancillary qubits to be added to existing ancillary quantum register but new quantum register was created";
        ASSERT_EQ(annotatableQuantumComputation.getNqubits(), expectedNumQubitsAfterAdditionOfQuantumRegister) << "Total number of qubits in quantum computation after addition of quantum register did not match";
        ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(annotatableQuantumComputation, labelOfAppendedToQuantumRegister, expectedQubitRangeOfRegisterAfterQubitsWereAppended));

        ASSERT_EQ(annotatableQuantumComputation.getNops(), expectedNumQuantumOperationsAfterAdditionOfQuantumRegister) << "Expected number of quantum operations after appending ancillary qubits to ancillary quantum register did not match";
        if (numAncillaryQubitsToBeInitializedToOne > 0) {
            ASSERT_NO_FATAL_FAILURE(assertGatesForInitializationOfAncillaryQubitsSetToOneAddedToQuantumComputation(annotatableQuantumComputation, expectedFirstGeneratedAncillaryQubit, expectedInitialValuesOfAncillaryQubits, numQuantumOperationsPriorToAdditionOfQuantumRegister, numAncillaryQubitsToBeInitializedToOne));
        }
    }

    static void assertGatesForInitializationOfAncillaryQubitsSetToOneAddedToQuantumComputation(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit firstAncillaryQubit, const std::vector<bool>& ancillaryQubitsInitialValues, const std::size_t firstQuantumOperationToCheck, const std::size_t numQuantumOperationsToCheck) {
        if (ancillaryQubitsInitialValues.empty()) {
            return;
        }

        const qc::Qubit lastAncillaryQubitToCheck = firstAncillaryQubit + static_cast<qc::Qubit>(ancillaryQubitsInitialValues.size() - 1U);
        ASSERT_LT(lastAncillaryQubitToCheck, annotatableQuantumComputation.getNqubits()) << "Tried to check correct initialization of qubit " << std::to_string(lastAncillaryQubitToCheck) << " while the quantum computation only had " << std::to_string(annotatableQuantumComputation.getNqubits()) << " qubits";
        ASSERT_LT(firstQuantumOperationToCheck, annotatableQuantumComputation.getNops()) << "Index of first quantum operation to start search for initialization of ancillary qubit was larger than the number of operations in the quantum computation";

        const std::size_t truncatedNumOperationsToCheck = std::min(numQuantumOperationsToCheck, annotatableQuantumComputation.getNops());
        const std::size_t lastQuantumOperationToCheck   = firstQuantumOperationToCheck + truncatedNumOperationsToCheck;
        for (qc::Qubit qubit = firstAncillaryQubit; qubit <= lastAncillaryQubitToCheck; ++qubit) {
            if (!ancillaryQubitsInitialValues.at(qubit - firstAncillaryQubit)) {
                continue;
            }

            std::size_t numQuantumOperationsInitializingQubitToOne = 0;
            for (std::size_t i = firstQuantumOperationToCheck; i <= lastQuantumOperationToCheck; ++i) {
                const qc::Operation* op = annotatableQuantumComputation.getQuantumOperation(i);
                numQuantumOperationsInitializingQubitToOne += static_cast<std::size_t>(op != nullptr && op->isSingleQubitGate() && op->getNtargets() == 1 && op->getTargets().at(0) == qubit);
            }
            ASSERT_EQ(numQuantumOperationsInitializingQubitToOne, 1) << "Expected exactly one quantum operation (X gate with no controls and one target qubit) with the qubit " << std::to_string(qubit) << " to exist in the quantum computation but actually " << std::to_string(numQuantumOperationsInitializingQubitToOne) << " such quantum operation were found!";
        }
    }

    static void assertQuantumRegisterExists(const AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& expectedQuantumRegisterLabel, const AnnotatableQuantumComputation::QubitIndexRange& expectedQubitRangeOfQuantumRegister) {
        const qc::QuantumRegisterMap& quantumRegisterMap = annotatableQuantumComputation.getQuantumRegisters();
        ASSERT_TRUE(quantumRegisterMap.contains(expectedQuantumRegisterLabel)) << "Quantum computation did not contain a quantum register with an identifier equal to " << expectedQuantumRegisterLabel;

        const qc::QuantumRegister& actualQuantumRegister = quantumRegisterMap.at(expectedQuantumRegisterLabel);
        ASSERT_EQ(actualQuantumRegister.getStartIndex(), expectedQubitRangeOfQuantumRegister.firstQubitIndex) << "Expected first qubit of quantum register to be equal to " << std::to_string(expectedQubitRangeOfQuantumRegister.firstQubitIndex) << " but was actually " << std::to_string(actualQuantumRegister.getStartIndex());
        ASSERT_EQ(actualQuantumRegister.getEndIndex(), expectedQubitRangeOfQuantumRegister.lastQubitIndex) << "Expected last qubit of quantum register to be equal to " << std::to_string(expectedQubitRangeOfQuantumRegister.lastQubitIndex) << " but was actually " << std::to_string(actualQuantumRegister.getEndIndex());
    }

    static void create1DQuantumRegisterContainingNQubits(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::size_t numQubitsToCreate) {
        const auto expectedQubitRangeOfQuantumRegister         = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = static_cast<qc::Qubit>(numQubitsToCreate) - 1U});
        const auto variableLayoutAssociatedWithQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = static_cast<unsigned>(numQubitsToCreate)});
        ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(annotatableQuantumComputation, AnnotatableQuantumComputation::QubitType::Data, "1dNQubitReg", expectedQubitRangeOfQuantumRegister, variableLayoutAssociatedWithQuantumRegister, std::nullopt));

        ASSERT_EQ(annotatableQuantumComputation.getQuantumRegisters().size(), 1U);
        ASSERT_EQ(annotatableQuantumComputation.getNqubits(), numQubitsToCreate);
        ASSERT_EQ(annotatableQuantumComputation.getNops(), 0U);
        for (qc::Qubit qubit = 0U; qubit < numQubitsToCreate; ++qubit) {
            ASSERT_FALSE(annotatableQuantumComputation.logicalQubitIsGarbage(qubit));
            ASSERT_FALSE(annotatableQuantumComputation.logicalQubitIsAncillary(qubit));
            ASSERT_FALSE(annotatableQuantumComputation.getInlinedQubitInformation(qubit).has_value());
        }
    }

    static void assertInlineStackEntriesMatch(const QubitInliningStack::QubitInliningStackEntry& expected, const QubitInliningStack::QubitInliningStackEntry& actual) {
        if (expected.lineNumberOfCallOfTargetModule.has_value()) {
            ASSERT_TRUE(actual.lineNumberOfCallOfTargetModule.has_value()) << "Expected source code line number of called target module to not have a value";
            ASSERT_EQ(*expected.lineNumberOfCallOfTargetModule, *actual.lineNumberOfCallOfTargetModule) << "Source code line number of called target module mismatch";
        } else {
            ASSERT_FALSE(actual.lineNumberOfCallOfTargetModule.has_value()) << "Expected source code line number of called target module to not have a value";
        }

        if (expected.isTargetModuleAccessedViaCallStmt.has_value()) {
            ASSERT_TRUE(expected.isTargetModuleAccessedViaCallStmt.has_value()) << "Expected call type of target module to be specified";
            ASSERT_EQ(*expected.isTargetModuleAccessedViaCallStmt, actual.isTargetModuleAccessedViaCallStmt) << "Call type of target module mismatch";
        } else {
            ASSERT_FALSE(actual.isTargetModuleAccessedViaCallStmt.has_value()) << "Expected call type of target module not to be specified";
        }

        if (expected.targetModule != nullptr) {
            ASSERT_THAT(actual.targetModule, testing::NotNull()) << "Expected target module to be set";
            ASSERT_THAT(actual.targetModule, expected.targetModule) << "Target module reference mismatch";
        } else {
            ASSERT_THAT(actual.targetModule, testing::IsNull()) << "Expected target module to not be set";
        }
    }

    static void assertQubitInlineStacksMatch(QubitInliningStack& expected, QubitInliningStack& actual) {
        const std::size_t expectedInlineStackSize = expected.size();
        const std::size_t actualInlineStackSize   = actual.size();
        ASSERT_EQ(expectedInlineStackSize, actualInlineStackSize) << "Expected qubit inline stack had a size of " << std::to_string(expectedInlineStackSize) << " while the actual one had a size of " << std::to_string(actualInlineStackSize);

        for (std::size_t i = 0; i < expectedInlineStackSize; ++i) {
            const QubitInliningStack::QubitInliningStackEntry* expectedInlineStackEntry = expected.getStackEntryAt(i);
            const QubitInliningStack::QubitInliningStackEntry* actualInlineStackEntry   = actual.getStackEntryAt(i);
            ASSERT_THAT(expectedInlineStackEntry, testing::NotNull()) << "Failed to fetch inline stack entry at index " << std::to_string(i);
            ASSERT_THAT(actualInlineStackEntry, testing::NotNull()) << "Failed to fetch inline stack entry at index " << std::to_string(i);
            ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(*expectedInlineStackEntry, *actualInlineStackEntry));
        }
    }

    static void assertQubitInlineInformationMatches(const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation>& expectedInlinedQubitInformation, const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation>& actualInlinedQubitInformation) {
        if (!expectedInlinedQubitInformation.has_value()) {
            ASSERT_FALSE(actualInlinedQubitInformation.has_value());
        } else {
            ASSERT_TRUE(actualInlinedQubitInformation.has_value());
            if (expectedInlinedQubitInformation->userDeclaredQubitLabel.has_value()) {
                ASSERT_TRUE(actualInlinedQubitInformation->userDeclaredQubitLabel.has_value()) << "Expected that user declared qubit label to be set in qubit inline information";
                ASSERT_EQ(*expectedInlinedQubitInformation->userDeclaredQubitLabel, *actualInlinedQubitInformation->userDeclaredQubitLabel) << "User declared qubit label mismatch in qubit inline information";
            } else {
                ASSERT_FALSE(actualInlinedQubitInformation->userDeclaredQubitLabel.has_value()) << "Expected that user declared qubit label is not set in qubit inline information";
            }

            if (expectedInlinedQubitInformation->inlineStack.has_value()) {
                ASSERT_TRUE(actualInlinedQubitInformation->inlineStack.has_value()) << "Expected inline stack to have a value";
                ASSERT_THAT(*expectedInlinedQubitInformation->inlineStack, testing::NotNull()) << "Expected inline stack cannot be null";
                ASSERT_THAT(*actualInlinedQubitInformation->inlineStack, testing::NotNull()) << "Actual inline stack cannot be null";
                ASSERT_NO_FATAL_FAILURE(assertQubitInlineStacksMatch(**expectedInlinedQubitInformation->inlineStack, **actualInlinedQubitInformation->inlineStack));
            } else {
                ASSERT_FALSE(actualInlinedQubitInformation->inlineStack.has_value()) << "Expected inline stack to not be set";
            }
        }
    }

    [[nodiscard]] static std::string buildExpectedQubitLabel(const std::string& labelOfQuantumRegisterStoringQubit, const std::vector<unsigned>& accessedValuePerDimension, const unsigned bit) {
        std::string generatedLabel = labelOfQuantumRegisterStoringQubit;
        for (const auto accessedValueOfDimension: accessedValuePerDimension) {
            generatedLabel += "[" + std::to_string(accessedValueOfDimension) + "]";
        }
        generatedLabel += "." + std::to_string(bit);
        return generatedLabel;
    }

    static void assertExpectedAndActualQubitLabelMatch(const AnnotatableQuantumComputation& annotatableQuantumComputation, const AnnotatableQuantumComputation::QubitLabelType qubitLabelType, const qc::Qubit qubitToCheck, const std::string& expectedQubitIdentifier, const std::vector<unsigned>& expectedAccessedValuePerDimensionToAccessQubitToCheck, const unsigned expectedAccessedBitToAccessQubitToCheck) {
        std::string                expectedQubitLabel;
        std::optional<std::string> actualQubitLabel;

        ASSERT_NO_FATAL_FAILURE(expectedQubitLabel = buildExpectedQubitLabel(expectedQubitIdentifier, expectedAccessedValuePerDimensionToAccessQubitToCheck, expectedAccessedBitToAccessQubitToCheck));
        ASSERT_NO_FATAL_FAILURE(actualQubitLabel = annotatableQuantumComputation.getQubitLabel(qubitToCheck, qubitLabelType)) << "Failed to fetch qubit label of qubit " << std::to_string(qubitToCheck);
        ASSERT_TRUE(actualQubitLabel.has_value()) << "Expected qubit label of qubit " << std::to_string(qubitToCheck) << " to have a value";
        ASSERT_EQ(expectedQubitLabel, *actualQubitLabel) << "Mismatch between expected and actual qubit label of qubit " << std::to_string(qubitToCheck);
    }

    static void assertInlineQubitInformationMatchesExpectedOne(const AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit qubit, const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation>& expectedInlineQubitInformation) {
        std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> actualInlineQubitInformation;
        ASSERT_NO_FATAL_FAILURE(actualInlineQubitInformation = annotatableQuantumComputation.getInlinedQubitInformation(qubit)) << "Failed to fetch inline qubit information of qubit " << std::to_string(qubit);
        ASSERT_NO_FATAL_FAILURE(assertQubitInlineInformationMatches(expectedInlineQubitInformation, actualInlineQubitInformation)) << "Inline qubit information mismatch for qubit " << std::to_string(qubit);
    }

    static void assertInlineQubitInformationMatchesExpectedOne(const AnnotatableQuantumComputation& annotatableQuantumComputation, const AnnotatableQuantumComputation::QubitIndexRange& qubitIndexRangeToCheck,
                                                               const std::optional<std::string>& optionalExpectedVariableIdentifierInBuildQubitLabel, const std::vector<unsigned>& accessedValuePerDimensionToAccessCheckedQubitIndexRange, const QubitInliningStack::ptr& expectedSharedQubitInlineStack) {
        AnnotatableQuantumComputation::InlinedQubitInformation expectedInlineInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = expectedSharedQubitInlineStack});
        for (qc::Qubit qubit = qubitIndexRangeToCheck.firstQubitIndex; qubit <= qubitIndexRangeToCheck.lastQubitIndex; ++qubit) {
            expectedInlineInformation.userDeclaredQubitLabel = optionalExpectedVariableIdentifierInBuildQubitLabel.has_value() ? std::make_optional(buildExpectedQubitLabel(*optionalExpectedVariableIdentifierInBuildQubitLabel, accessedValuePerDimensionToAccessCheckedQubitIndexRange, qubit - qubitIndexRangeToCheck.firstQubitIndex)) : std::nullopt;
            ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(annotatableQuantumComputation, qubit, expectedInlineInformation));
        }
    }
};

// BEGIN Add quantum register for SyReC variable tests

class SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture: public AnnotatableQuantumComputationTestsFixture, public testing::WithParamInterface<AnnotatableQuantumComputation::QubitType> {
public:
    [[nodiscard]] static AnnotatableQuantumComputation::QubitType getOtherQubitType(const AnnotatableQuantumComputation::QubitType qubitTypeForWhichDifferentOneShouldBeFound) noexcept {
        switch (qubitTypeForWhichDifferentOneShouldBeFound) {
            case AnnotatableQuantumComputation::QubitType::Data:
                return AnnotatableQuantumComputation::QubitType::Garbage;
            case AnnotatableQuantumComputation::QubitType::Garbage:
                return AnnotatableQuantumComputation::QubitType::Data;
            case AnnotatableQuantumComputation::QubitType::Ancillary:
                return AnnotatableQuantumComputation::QubitType::Garbage;
            default:
                return AnnotatableQuantumComputation::QubitType::Data;
        }
    }
};

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQregFor1DSyrecVariable) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    // Qubit inline information should only be recorded for non-data qubits
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalSharedInlinedQubitInformation;
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
        optionalSharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});
    }

    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, optionalSharedInlinedQubitInformation));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    ExpectedQubitFlags expectedSharedQubitFlags = QubitShouldBeDataQubit;
    switch (GetParam()) {
        case AnnotatableQuantumComputation::QubitType::Data:
            break;
        case AnnotatableQuantumComputation::QubitType::Garbage:
        case AnnotatableQuantumComputation::QubitType::Ancillary:
            expectedSharedQubitFlags = QubitShouldBeGarbage | InlineQubitInformationShouldBeFetchable;
            break;
        default:
            FAIL();
    }

    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        ASSERT_TRUE(optionalSharedInlinedQubitInformation.has_value());
        ASSERT_TRUE(optionalSharedInlinedQubitInformation->userDeclaredQubitLabel.has_value());
        ASSERT_TRUE(optionalSharedInlinedQubitInformation->inlineStack.has_value());
        ASSERT_THAT(*optionalSharedInlinedQubitInformation->inlineStack, testing::NotNull());

        constexpr auto qubitRangeOfFirstValueOfDimension = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U});
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubitRangeOfFirstValueOfDimension, optionalSharedInlinedQubitInformation->userDeclaredQubitLabel, {0U}, *optionalSharedInlinedQubitInformation->inlineStack));

        constexpr auto qubitRangeOfSecondValueOfDimension = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U});
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubitRangeOfSecondValueOfDimension, optionalSharedInlinedQubitInformation->userDeclaredQubitLabel, {1U}, *optionalSharedInlinedQubitInformation->inlineStack));

        constexpr auto qubitRangeOfThirdValueOfDimension = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 11U});
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubitRangeOfThirdValueOfDimension, optionalSharedInlinedQubitInformation->userDeclaredQubitLabel, {2U}, *optionalSharedInlinedQubitInformation->inlineStack));
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQregForANDSyrecVariable) {
    const auto         associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInVariable               = 96;

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});

    // Qubit inline information should only be recorded for non-data qubits
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalSharedInlinedQubitInformation;
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
        optionalSharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});
    }

    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, optionalSharedInlinedQubitInformation));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInVariable);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    ExpectedQubitFlags expectedSharedQubitFlags = QubitShouldBeDataQubit;
    switch (GetParam()) {
        case AnnotatableQuantumComputation::QubitType::Data:
            break;
        case AnnotatableQuantumComputation::QubitType::Garbage:
        case AnnotatableQuantumComputation::QubitType::Ancillary:
            expectedSharedQubitFlags = QubitShouldBeGarbage | InlineQubitInformationShouldBeFetchable;
            break;
        default:
            FAIL();
    }

    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        ASSERT_TRUE(optionalSharedInlinedQubitInformation.has_value());
        ASSERT_TRUE(optionalSharedInlinedQubitInformation->userDeclaredQubitLabel.has_value());
        ASSERT_TRUE(optionalSharedInlinedQubitInformation->inlineStack.has_value());
        ASSERT_THAT(*optionalSharedInlinedQubitInformation->inlineStack, testing::NotNull());

        std::vector accessedValuePerDimension            = {0U, 0U, 0U};
        auto        qubitRangeOfAccessedValueOfDimension = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U});
        for (unsigned firstDimIdx = 0; firstDimIdx < 3; ++firstDimIdx) {
            accessedValuePerDimension[0] = firstDimIdx;
            for (unsigned secDimIdx = 0; secDimIdx < 4; ++secDimIdx) {
                accessedValuePerDimension[1] = secDimIdx;
                for (unsigned thirdDimIdx = 0; thirdDimIdx < 3; ++thirdDimIdx) {
                    accessedValuePerDimension[2] = thirdDimIdx;
                    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubitRangeOfAccessedValueOfDimension, optionalSharedInlinedQubitInformation->userDeclaredQubitLabel, accessedValuePerDimension, *optionalSharedInlinedQubitInformation->inlineStack));
                    qubitRangeOfAccessedValueOfDimension.lastQubitIndex = qubitRangeOfAccessedValueOfDimension.firstQubitIndex;
                    qubitRangeOfAccessedValueOfDimension.firstQubitIndex += associatedVariableLayoutOfQuantumRegister.bitwidth;
                }
            }
        }
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegFor1DSyrecVariableWithoutInlineInformation) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForANDSyrecVariableWithoutInlineInformation) {
    const auto         associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInVariable               = 96;

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInVariable);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableIsNotFusedWithExistingQRegOfOtherSyrecVariable) {
    const auto moduleDefiningCalls = std::make_shared<Module>("main");

    const AnnotatableQuantumComputation::QubitType                        qubitTypeOfExistingQReg   = getOtherQubitType(GetParam());
    const std::string                                                     expectedExistingQRegLabel = "first_qreg";
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalinlineInformationOfExistingQReg;
    if (qubitTypeOfExistingQReg != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStackOfExistingQReg = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStackOfExistingQReg->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = moduleDefiningCalls})));
        optionalinlineInformationOfExistingQReg = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = expectedExistingQRegLabel, .inlineStack = qubitInlineStackOfExistingQReg});
    }
    const AnnotatableQuantumComputation::QubitType                        qubitTypeOfToBeAddedQReg         = GetParam();
    const std::string                                                     expectedQRegLabelOfToBeAddedQReg = "second_qreg";
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalInlineInformationOfToBeAddedQReg;
    if (qubitTypeOfToBeAddedQReg != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStackOfToBeAddedQReg = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStackOfToBeAddedQReg->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 4U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = moduleDefiningCalls})));
        optionalInlineInformationOfToBeAddedQReg = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = expectedQRegLabelOfToBeAddedQReg, .inlineStack = qubitInlineStackOfToBeAddedQReg});
    }

    const auto         associatedVariableLayoutOfExistingQReg = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U, 3U}, .bitwidth = 2U});
    constexpr unsigned expectedNumQubitsInExistingQReg        = 6U;
    constexpr auto     expectedQubitRangeOfExistingQReg       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, qubitTypeOfExistingQReg, expectedExistingQRegLabel, expectedQubitRangeOfExistingQReg, associatedVariableLayoutOfExistingQReg, optionalinlineInformationOfExistingQReg));

    const auto         associatedVariableLayoutOfToBeAddedQReg = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInToBeAddedQReg        = 16U;
    constexpr auto     expectedQubitRangeOfToBeAddedQReg       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 6U, .lastQubitIndex = 21U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQRegLabelOfToBeAddedQReg, expectedQubitRangeOfToBeAddedQReg, associatedVariableLayoutOfToBeAddedQReg, optionalInlineInformationOfToBeAddedQReg));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInExistingQReg + expectedNumQubitsInToBeAddedQReg);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    ExpectedQubitFlags expectedSharedQubitFlagsOfExistingQReg = QubitShouldBeDataQubit;
    switch (qubitTypeOfExistingQReg) {
        case AnnotatableQuantumComputation::QubitType::Data:
            break;
        case AnnotatableQuantumComputation::QubitType::Garbage:
        case AnnotatableQuantumComputation::QubitType::Ancillary:
            expectedSharedQubitFlagsOfExistingQReg = QubitShouldBeGarbage | InlineQubitInformationShouldBeFetchable;
            break;
        default:
            FAIL();
    }
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfExistingQReg, expectedSharedQubitFlagsOfExistingQReg));
    if (expectedSharedQubitFlagsOfExistingQReg & InlineQubitInformationShouldBeFetchable) {
        ASSERT_TRUE(optionalinlineInformationOfExistingQReg.has_value());
        ASSERT_TRUE(optionalinlineInformationOfExistingQReg->userDeclaredQubitLabel.has_value());
        ASSERT_TRUE(optionalinlineInformationOfExistingQReg->inlineStack.has_value());
        ASSERT_THAT(*optionalinlineInformationOfExistingQReg->inlineStack, testing::NotNull());

        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 1U}), optionalinlineInformationOfExistingQReg->userDeclaredQubitLabel, {0U, 0U}, *optionalinlineInformationOfExistingQReg->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 2U, .lastQubitIndex = 3U}), optionalinlineInformationOfExistingQReg->userDeclaredQubitLabel, {0U, 1U}, *optionalinlineInformationOfExistingQReg->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 5U}), optionalinlineInformationOfExistingQReg->userDeclaredQubitLabel, {0U, 2U}, *optionalinlineInformationOfExistingQReg->inlineStack));
    }

    ExpectedQubitFlags expectedSharedQubitFlagsOfToBeAddedQReg = QubitShouldBeDataQubit;
    switch (qubitTypeOfToBeAddedQReg) {
        case AnnotatableQuantumComputation::QubitType::Data:
            break;
        case AnnotatableQuantumComputation::QubitType::Garbage:
        case AnnotatableQuantumComputation::QubitType::Ancillary:
            expectedSharedQubitFlagsOfToBeAddedQReg = QubitShouldBeGarbage | InlineQubitInformationShouldBeFetchable;
            break;
        default:
            FAIL();
    }
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfToBeAddedQReg, expectedSharedQubitFlagsOfToBeAddedQReg));
    if (expectedSharedQubitFlagsOfToBeAddedQReg & InlineQubitInformationShouldBeFetchable) {
        ASSERT_TRUE(optionalInlineInformationOfToBeAddedQReg.has_value());
        ASSERT_TRUE(optionalInlineInformationOfToBeAddedQReg->userDeclaredQubitLabel.has_value());
        ASSERT_TRUE(optionalInlineInformationOfToBeAddedQReg->inlineStack.has_value());
        ASSERT_THAT(*optionalInlineInformationOfToBeAddedQReg->inlineStack, testing::NotNull());

        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 6U, .lastQubitIndex = 9U}), optionalInlineInformationOfToBeAddedQReg->userDeclaredQubitLabel, {0U, 0U}, *optionalInlineInformationOfToBeAddedQReg->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 10U, .lastQubitIndex = 13U}), optionalInlineInformationOfToBeAddedQReg->userDeclaredQubitLabel, {0U, 1U}, *optionalInlineInformationOfToBeAddedQReg->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 14U, .lastQubitIndex = 17U}), optionalInlineInformationOfToBeAddedQReg->userDeclaredQubitLabel, {1U, 0U}, *optionalInlineInformationOfToBeAddedQReg->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 18U, .lastQubitIndex = 21U}), optionalInlineInformationOfToBeAddedQReg->userDeclaredQubitLabel, {1U, 1U}, *optionalInlineInformationOfToBeAddedQReg->inlineStack));
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableIsNotFusedWithExistingAggregateOfAncillaryQubitsQReg) {
    constexpr auto     expectedQubitRangeOfAggregateAncillaryQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0, .lastQubitIndex = 4U});
    constexpr unsigned expectedNumQubitsInAggregateAncillaryQubitsQReg  = 5;
    const auto         expectedInitialStateOfAncillaryQubits            = std::vector({false, false, true, false, false});
    const std::string  expectedLabelOfAggregateAncillaryQubitsQReg      = "ancReg";

    const auto aggregatedAncillaryQubitsInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(aggregatedAncillaryQubitsInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformationOfAggregatedAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = aggregatedAncillaryQubitsInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedLabelOfAggregateAncillaryQubitsQReg, expectedQubitRangeOfAggregateAncillaryQubitsQReg, expectedInitialStateOfAncillaryQubits, sharedInlinedQubitInformationOfAggregatedAncillaryQubits));

    // Qubit inline information should only be recorded for non-data qubits
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalSharedInlinedQubitInformationOfQregOfVariable;
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
        optionalSharedInlinedQubitInformationOfQregOfVariable = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});
    }

    const auto         associatedVariableLayoutInfoOfQRegOfVariable = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {4U}, .bitwidth = 3U});
    constexpr unsigned expectedNumQubitsInQRegOfVariable            = 12U;
    constexpr auto     expectedQubitRangeOfQRegOfVariable           = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 16U});
    const std::string  expectedLabelOfQRegOfVariable                = "nonAnc_qReg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedLabelOfQRegOfVariable, expectedQubitRangeOfQRegOfVariable, associatedVariableLayoutInfoOfQRegOfVariable, optionalSharedInlinedQubitInformationOfQregOfVariable));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInAggregateAncillaryQubitsQReg + expectedNumQubitsInQRegOfVariable);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAggregateAncillaryQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U}), std::nullopt, {0U}, aggregatedAncillaryQubitsInlineStack));

    ExpectedQubitFlags expectedSharedQubitFlags = QubitShouldBeDataQubit;
    switch (GetParam()) {
        case AnnotatableQuantumComputation::QubitType::Data:
            break;
        case AnnotatableQuantumComputation::QubitType::Garbage:
        case AnnotatableQuantumComputation::QubitType::Ancillary:
            expectedSharedQubitFlags = QubitShouldBeGarbage | InlineQubitInformationShouldBeFetchable;
            break;
        default:
            FAIL();
    }

    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfQRegOfVariable, expectedSharedQubitFlags));
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        ASSERT_TRUE(optionalSharedInlinedQubitInformationOfQregOfVariable.has_value());
        ASSERT_TRUE(optionalSharedInlinedQubitInformationOfQregOfVariable->userDeclaredQubitLabel.has_value());
        ASSERT_TRUE(optionalSharedInlinedQubitInformationOfQregOfVariable->inlineStack.has_value());
        ASSERT_THAT(*optionalSharedInlinedQubitInformationOfQregOfVariable->inlineStack, testing::NotNull());

        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 7U}), optionalSharedInlinedQubitInformationOfQregOfVariable->userDeclaredQubitLabel, {0U}, *optionalSharedInlinedQubitInformationOfQregOfVariable->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 10U}), optionalSharedInlinedQubitInformationOfQregOfVariable->userDeclaredQubitLabel, {1U}, *optionalSharedInlinedQubitInformationOfQregOfVariable->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 11U, .lastQubitIndex = 13U}), optionalSharedInlinedQubitInformationOfQregOfVariable->userDeclaredQubitLabel, {2U}, *optionalSharedInlinedQubitInformationOfQregOfVariable->inlineStack));
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 14U, .lastQubitIndex = 16U}), optionalSharedInlinedQubitInformationOfQregOfVariable->userDeclaredQubitLabel, {3U}, *optionalSharedInlinedQubitInformationOfQregOfVariable->inlineStack));
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithVariableBitwidthEqualToZeroNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));

    const auto associatedVariableLayoutWithInvalidBitwidth = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 4U}, .bitwidth = 0U});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "aLabel", associatedVariableLayoutWithInvalidBitwidth, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithNumberOfValuesOfAnyDimensionEqualToZeroNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));

    const auto associatedVariableLayoutWithInvalidNumberOfValuesOfDimension = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 0U, 4U}, .bitwidth = 2U});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "aLabel", associatedVariableLayoutWithInvalidNumberOfValuesOfDimension, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithTotalNumberOfDimensionEqualToZeroNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));

    const auto associatedVariableLayoutWithInvalidNumberOfDimensions = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {}, .bitwidth = 2U});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "aLabel", associatedVariableLayoutWithInvalidNumberOfDimensions, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithEmptyQuantumRegisterLabelNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "", associatedVariableLayoutOfQuantumRegister, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithDuplicateQuantumRegisterLabelNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithQuantumRegisterLabelEqualToAncillaryQuantumRegisterNotPossible) {
    const std::string expectedQuantumRegisterLabel               = "qReg";
    constexpr auto    expectedAncillaryQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U});
    const auto        initialValuesOfAncillaryQubits             = {false, true, true, false};

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    const auto ancillaryQubitsSharedInlineInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedAncillaryQuantumRegisterQubitRange, initialValuesOfAncillaryQubits, ancillaryQubitsSharedInlineInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), expectedQuantumRegisterLabel, AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U}, .bitwidth = 3U})));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promoteQuantumRegistersPreliminaryMarkedAsStoringAncillaryQubitsToDefinitivelyStoringAncillaryQubits());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 4U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 2U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedAncillaryQuantumRegisterQubitRange, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithInvalidQubitStackInInlinedQubitInformationNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));

    const auto inlinedQubitInformationWithNullptrInlineStack = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = nullptr});
    const auto inlinedQubitInformationWithEmptyInlineStack   = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = std::make_shared<QubitInliningStack>()});

    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "aLabel", associatedVariableLayoutOfQuantumRegister, inlinedQubitInformationWithNullptrInlineStack).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "aLabel", associatedVariableLayoutOfQuantumRegister, inlinedQubitInformationWithEmptyInlineStack).has_value());

    const auto inlineStackWithInvalidEntry = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackWithInvalidEntry->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    QubitInliningStack::QubitInliningStackEntry* inlineStackEntry = inlineStackWithInvalidEntry->getStackEntryAt(0);
    ASSERT_THAT(inlineStackEntry, testing::NotNull());
    inlineStackEntry->targetModule = nullptr;

    const auto inlinedQubitInformationWithInvalidInlineStackEntry = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = inlineStackWithInvalidEntry});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "aLabel", associatedVariableLayoutOfQuantumRegister, inlinedQubitInformationWithInvalidInlineStackEntry).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableWithInvalidUserDeclaredQubitLabelInInlinedQubitInformationNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));

    const auto validInlineQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "test", .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, validInlineQubitInformation, true));

    const auto inlineQubitInformationWithNotSetUserDeclaredQubitLabel = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, inlineQubitInformationWithNotSetUserDeclaredQubitLabel, true).has_value());

    const auto inlineQubitInformationWithEmptyUserDeclaredQubitLabel = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "", .inlineStack = inlineStack});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, inlineQubitInformationWithEmptyUserDeclaredQubitLabel, true).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    const ExpectedQubitFlags expectedSharedQubitFlags = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam()) | InlineQubitInformationShouldBeFetchable;
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, expectedSharedQubitFlags));
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddQRegForSyrecVariableAfterPreliminaryAncillaryQubitsWerePromotedToActualOnesNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promoteQuantumRegistersPreliminaryMarkedAsStoringAncillaryQubitsToDefinitivelyStoringAncillaryQubits());
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(GetParam(), "anotherQReg", associatedVariableLayoutOfQuantumRegister, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);

    switch (GetParam()) {
        case AnnotatableQuantumComputation::QubitType::Data:
            ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, ExpectedQubitFlags::QubitShouldBeDataQubit));
            break;
        case AnnotatableQuantumComputation::QubitType::Garbage:
            ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, ExpectedQubitFlags::QubitShouldBeGarbage));
            break;
        case AnnotatableQuantumComputation::QubitType::Ancillary:
            ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQuantumRegisterQubitRange, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary));
            break;
        default:
            FAIL();
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfQubitsOf1DVariable) {
    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, qubit, expectedQuantumRegisterLabel, {0U}, qubit));
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfQubitsOfNDimensionalVariable) {
    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 29U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 3U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister));

    qc::Qubit currentlyCheckedQubit = 0U;
    for (const auto& expectedAccessedValuePerDimensionCombination: std::vector<std::vector<unsigned>>({{0U, 0U}, {0U, 1U}, {0U, 2U}, {1U, 0U}, {1U, 1U}})) {
        for (qc::Qubit relativeQubitIndexInAccessedElementOfVariableInQuantumRegister = 0; relativeQubitIndexInAccessedElementOfVariableInQuantumRegister <= 4U; ++relativeQubitIndexInAccessedElementOfVariableInQuantumRegister) {
            ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, currentlyCheckedQubit, expectedQuantumRegisterLabel, expectedAccessedValuePerDimensionCombination, relativeQubitIndexInAccessedElementOfVariableInQuantumRegister));
            ++currentlyCheckedQubit;
        }
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfQubitsOf1DVariable) {
    const std::string associatedVariableIdentifier = "varName";

    // Qubit inline information should only be recorded for non-data qubits
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalSharedInlinedQubitInformationOfQregOfVariable;
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
        optionalSharedInlinedQubitInformationOfQregOfVariable = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = associatedVariableIdentifier, .inlineStack = qubitInlineStack});
    }

    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, optionalSharedInlinedQubitInformationOfQregOfVariable));

    if (GetParam() == AnnotatableQuantumComputation::QubitType::Data) {
        for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
            ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(qubit, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value());
        }
    } else {
        for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
            ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::UserDeclared, qubit, associatedVariableIdentifier, {0U}, qubit));
        }
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfQubitsOfNDimensionalVariable) {
    const std::string associatedVariableIdentifier = "varName";

    // Qubit inline information should only be recorded for non-data qubits
    std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalSharedInlinedQubitInformationOfQregOfVariable;
    if (GetParam() != AnnotatableQuantumComputation::QubitType::Data) {
        const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
        ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
        optionalSharedInlinedQubitInformationOfQregOfVariable = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = associatedVariableIdentifier, .inlineStack = qubitInlineStack});
    }

    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 29U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 3U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, optionalSharedInlinedQubitInformationOfQregOfVariable));

    if (GetParam() == AnnotatableQuantumComputation::QubitType::Data) {
        for (qc::Qubit qubit = expectedQubitRangeOfQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfQuantumRegister.lastQubitIndex; ++qubit) {
            ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(qubit, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value());
        }
    } else {
        qc::Qubit currentlyCheckedQubit = 0U;
        for (const auto& expectedAccessedValuePerDimensionCombination: std::vector<std::vector<unsigned>>({{0U, 0U}, {0U, 1U}, {0U, 2U}, {1U, 0U}, {1U, 1U}})) {
            for (qc::Qubit relativeQubitIndexInAccessedElementOfVariableInQuantumRegister = 0; relativeQubitIndexInAccessedElementOfVariableInQuantumRegister <= 4U; ++relativeQubitIndexInAccessedElementOfVariableInQuantumRegister) {
                ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::UserDeclared, currentlyCheckedQubit, associatedVariableIdentifier, expectedAccessedValuePerDimensionCombination, relativeQubitIndexInAccessedElementOfVariableInQuantumRegister));
                ++currentlyCheckedQubit;
            }
        }
    }
}

TEST_P(SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegAndCheckAdjacentQRegForSyrecVariableIsNotMerged) {
    const auto         associatedVariableLayoutOfNonAncillaryQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U}, .bitwidth = 3U});
    constexpr unsigned expectedNumQubitsInVariable                           = 36;

    const std::string nonAggregateQRegLabel                  = "qReg";
    constexpr auto    nonAggregateQRegCoveredQubitIndexRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, GetParam(), nonAggregateQRegLabel, nonAggregateQRegCoveredQubitIndexRange, associatedVariableLayoutOfNonAncillaryQuantumRegister));

    const std::string aggregateOfAncillaryQubitsQRegLabel                = "aReg";
    constexpr auto    expectedQubitRangeOfAggregateOfAncillaryQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 36U, .lastQubitIndex = 41U});
    const auto        expectedInitialValuesOfAncillaryQubits             = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                       = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, aggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfAggregateOfAncillaryQubitsQReg, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    constexpr unsigned expectedTotalNumQubits = 42;
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedTotalNumQubits);
    const ExpectedQubitFlags expectedQubitFlagsSharedByQubitsOfNonAggregateQReg = getExpectedQubitFlagsForQubitTypePriorToAncillaryQubitPromotion(GetParam());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, nonAggregateQRegCoveredQubitIndexRange, expectedQubitFlagsSharedByQubitsOfNonAggregateQReg));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAggregateOfAncillaryQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

INSTANTIATE_TEST_SUITE_P(
        SingleQuantumRegisterTests,
        SingleQregForSyrecVariableAnnotatableQuantumComputationTestsFixture,
        testing::Values(
                AnnotatableQuantumComputation::QubitType::Data,
                AnnotatableQuantumComputation::QubitType::Garbage,
                AnnotatableQuantumComputation::QubitType::Ancillary));

// BEGIN Add preliminary ancillary quantum register tests
TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegOfLengthOne) {
    const auto targetModule = std::make_shared<Module>("firstModule");
    const auto inlineStack  = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 2U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = targetModule})));

    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 0U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 1U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, 0U, sharedInlineQubitInformation));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegOfLengthN) {
    const auto targetModule = std::make_shared<Module>("firstModule");
    const auto inlineStack  = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 2U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = targetModule})));

    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    for (qc::Qubit qubit = 0U; qubit <= 5U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubit, sharedInlineQubitInformation));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegAndCheckAdjacentAggregateAncillaryQubitsQRegIsMerged) {
    const auto firstTargetModule = std::make_shared<Module>("firstModule");
    const auto firstInlineStack  = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(firstInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 2U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = firstTargetModule})));

    const auto secondTargetModule = std::make_shared<Module>("secondModule");
    const auto secondInlineStack  = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(secondInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 4U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = secondTargetModule})));

    const std::string ancillaryQubitQuantumRegisterLabel                  = "aReg";
    constexpr auto    expectedInitialQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits              = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformationOfFirstQReg             = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = firstInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformationOfFirstQReg));

    const auto          expectedInitialValuesOfOtherAncillaryQubits       = std::vector({true, false, true});
    constexpr auto      expectedFinalQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 8U});
    constexpr qc::Qubit expectedFirstAppendedAncillaryQubit               = 6U;
    const auto          sharedInlineQubitInformationOfSecondQReg          = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = secondInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedInitialValuesOfOtherAncillaryQubits, sharedInlineQubitInformationOfSecondQReg, expectedFirstAppendedAncillaryQubit, expectedFinalQubitRangeOfAncillaryQuantumRegister));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 9U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedFinalQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U}), std::nullopt, {0U}, firstInlineStack));
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 6U, .lastQubitIndex = 8U}), std::nullopt, {0U}, secondInlineStack));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegWithEmptyQuantumRegisterLabelNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("", expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegWithQuantumRegisterLabelMatchingExistingQuantumRegisterLabelNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne(ancillaryQubitQuantumRegisterLabel, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegWithEmptyAncillaryQubitInitialStatesNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("anotherLabel", {}, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegWithInvalidInlineStackNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    const auto inlinedQubitInformationWithNullptrInlineStack = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = nullptr});
    const auto inlinedQubitInformationWithEmptyInlineStack   = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = std::make_shared<QubitInliningStack>()});

    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("aLabel", expectedInitialValuesOfAncillaryQubits, inlinedQubitInformationWithNullptrInlineStack).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("aLabel", expectedInitialValuesOfAncillaryQubits, inlinedQubitInformationWithEmptyInlineStack).has_value());

    const auto inlineStackWithInvalidEntry = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackWithInvalidEntry->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    QubitInliningStack::QubitInliningStackEntry* inlineStackEntry = inlineStackWithInvalidEntry->getStackEntryAt(0);
    ASSERT_THAT(inlineStackEntry, testing::NotNull());
    inlineStackEntry->targetModule = nullptr;

    const auto inlinedQubitInformationWithInvalidInlineStackEntry = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStackWithInvalidEntry});
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("aLabel", expectedInitialValuesOfAncillaryQubits, inlinedQubitInformationWithInvalidInlineStackEntry).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegWithUserDeclaredQubitLabelInInlinedInformationNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});

    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = std::make_shared<Module>("test")})));

    const auto validInlineQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = qubitInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, validInlineQubitInformation));

    const auto inlineQubitInformationWithEmptyUserDeclaredQubitLabel = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "", .inlineStack = qubitInlineStack});
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("anotherLabel", expectedInitialValuesOfAncillaryQubits, inlineQubitInformationWithEmptyUserDeclaredQubitLabel).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddAggregateOfAncillaryQubitsQRegAfterPromotionOfPreliminaryAncillaryQubitsNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promoteQuantumRegistersPreliminaryMarkedAsStoringAncillaryQubitsToDefinitivelyStoringAncillaryQubits());
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterAggregatingIntermediateResultsOrAppendToAdjacentOne("anotherLabel", expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQuantumRegister, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddMixtureOfDifferentQuantumRegisters) {
    const std::string firstQRegOfVariableLabel                = "nqReg_1";
    const auto        variableLayoutOfFirstQRegOfVariable     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 3U}, .bitwidth = 2U});
    constexpr auto    expectedQubitRangeOfFirstQRegOfVariable = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Data, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable, variableLayoutOfFirstQRegOfVariable, std::nullopt));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));

    const auto        variableLayoutOfSecondQRegOfVariable     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3}, .bitwidth = 3U});
    const std::string secondQRegOfVariableLabel                = "nqReg_2";
    constexpr auto    expectedQubitRangeOfSecondQRegOfVariable = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 20U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Ancillary, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable, variableLayoutOfSecondQRegOfVariable, std::nullopt));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable));

    const auto        initialStateOfFirstAggregateOfAncillaryQubitsQReg                 = std::vector({false, true, true, false});
    const auto        sharedInlineQubitInformationOfFirstAggregateOfAncillaryQubitsQReg = AnnotatableQuantumComputation::InlinedQubitInformation();
    const std::string firstAggregateOfAncillaryQubitsQRegLabel                          = "aReg_1";
    constexpr auto    expectedQubitRangeOfFirstAggregateOfAncillaryQubitsQReg           = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 21U, .lastQubitIndex = 24U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, firstAggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfFirstAggregateOfAncillaryQubitsQReg, initialStateOfFirstAggregateOfAncillaryQubitsQReg, sharedInlineQubitInformationOfFirstAggregateOfAncillaryQubitsQReg));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 3U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfFirstAggregateOfAncillaryQubitsQReg));

    const auto          initialStateOfSecondAggregateOfAncillaryQubitsQReg                 = std::vector({true, false, false});
    const auto          sharedInlineQubitInformationOfSecondAggregateOfAncillaryQubitsQReg = AnnotatableQuantumComputation::InlinedQubitInformation();
    constexpr qc::Qubit expectedFirstAppendedAncillaryQubit                                = 25U;
    constexpr auto      expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterFirstMerge  = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 21U, .lastQubitIndex = 27U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(
            *annotatedQuantumComputation,
            firstAggregateOfAncillaryQubitsQRegLabel,
            initialStateOfSecondAggregateOfAncillaryQubitsQReg,
            sharedInlineQubitInformationOfSecondAggregateOfAncillaryQubitsQReg,
            expectedFirstAppendedAncillaryQubit,
            expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterFirstMerge));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 3U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterFirstMerge));

    const auto          initialStateOfThirdAggregateOfAncillaryQubitsQReg                  = std::vector({false, true});
    const auto          sharedInlineQubitInformationOfThirdAggregateOfAncillaryQubitsQReg  = AnnotatableQuantumComputation::InlinedQubitInformation();
    constexpr qc::Qubit expectedSecondAppendedAncillaryQubit                               = 28U;
    constexpr auto      expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterSecondMerge = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 21U, .lastQubitIndex = 29U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(
            *annotatedQuantumComputation,
            firstAggregateOfAncillaryQubitsQRegLabel,
            initialStateOfThirdAggregateOfAncillaryQubitsQReg,
            sharedInlineQubitInformationOfThirdAggregateOfAncillaryQubitsQReg,
            expectedSecondAppendedAncillaryQubit,
            expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterSecondMerge));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 3U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterSecondMerge));

    const auto        variableLayoutOfThirdQRegOfVariable     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2}, .bitwidth = 2U});
    const std::string thirdQRegOfVariableLabel                = "nqReg_3";
    constexpr auto    expectedQubitRangeOfThirdQRegOfVariable = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 30U, .lastQubitIndex = 33U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Garbage, thirdQRegOfVariableLabel, expectedQubitRangeOfThirdQRegOfVariable, variableLayoutOfThirdQRegOfVariable, std::nullopt));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 4U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterSecondMerge));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, thirdQRegOfVariableLabel, expectedQubitRangeOfThirdQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promoteQuantumRegistersPreliminaryMarkedAsStoringAncillaryQubitsToDefinitivelyStoringAncillaryQubits());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 4U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstQRegOfVariableLabel, expectedQubitRangeOfFirstQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondQRegOfVariableLabel, expectedQubitRangeOfSecondQRegOfVariable));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAggregateOfAncillaryQubitsQRegLabel, expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterSecondMerge));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, thirdQRegOfVariableLabel, expectedQubitRangeOfThirdQRegOfVariable));

    constexpr ExpectedQubitFlags expectedQubitFlagsOfFirstQRegForVariable = QubitShouldBeDataQubit;
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfFirstQRegOfVariable, expectedQubitFlagsOfFirstQRegForVariable));

    constexpr ExpectedQubitFlags expectedQubitFlagsOfSecondQRegForVariable = QubitShouldBeGarbage | QubitShouldBeAncillary;
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfSecondQRegOfVariable, expectedQubitFlagsOfSecondQRegForVariable));

    constexpr ExpectedQubitFlags expectedQubitFlagsOfMergeAggregateOfAncillaryQubitsQReg = QubitShouldBeGarbage | QubitShouldBeAncillary | InlineQubitInformationShouldBeFetchable;
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAggregateOfAncillaryQubitsQRegAfterSecondMerge, expectedQubitFlagsOfMergeAggregateOfAncillaryQubitsQReg));

    constexpr ExpectedQubitFlags expectedQubitFlagsOfThirdQRegForVariable = QubitShouldBeGarbage;
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfThirdQRegOfVariable, expectedQubitFlagsOfThirdQRegForVariable));
}
// // END Add preliminary ancillary quantum register tests
//
// // BEGIN getQubitLabel tests
TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelInEmptyQuantumComputation) {
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(0U, AnnotatableQuantumComputation::QubitLabelType::Internal).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfUnknownQubitNotPossible) {
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(4U, AnnotatableQuantumComputation::QubitLabelType::Internal).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(6U, AnnotatableQuantumComputation::QubitLabelType::Internal).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfAncillaryQubit) {
    const std::string expectedQuantumRegisterLabel           = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister    = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQubits = std::vector({false, false, true, false, false});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineInformationOfAncillaryQubits));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, qubit, expectedQuantumRegisterLabel, {0U}, qubit));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfAncillaryQubitAfterMergeOfAdjacentAncillaryQubits) {
    const std::string ancillaryQuantumRegisterLabel                         = "ancReg_1";
    constexpr auto    expectedInitialQubitRangeOfAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQuantumRegisterQubits = std::vector({false, false, true, false, false});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQuantumRegisterQubits, sharedInlineInformationOfAncillaryQubits));

    constexpr auto qubitRangeAppendedToAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 10U});
    const auto     expectedInitialValuesOfAppendedAncillaryQubits = std::vector({true, true, false, false, true, true});

    constexpr auto expectedFinalQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 10U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialValuesOfAppendedAncillaryQubits, sharedInlineInformationOfAncillaryQubits, qubitRangeAppendedToAncillaryQuantumRegister.firstQubitIndex, expectedFinalQubitRangeOfAncillaryQuantumRegister));

    for (qc::Qubit qubit = 0; qubit <= 10U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, qubit, ancillaryQuantumRegisterLabel, {0U}, qubit));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelInEmptyQuantumComputation) {
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(0U, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfUnknownQubitNotPossible) {
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(4U, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(6U, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfAncillaryQubitNotPossible) {
    const std::string expectedQuantumRegisterLabel           = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister    = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQubits = std::vector({false, false, true, false, false});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineInformationOfAncillaryQubits));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(qubit, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value()) << "Expected not to be able to fetch user declared label of qubit " << std::to_string(qubit);
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfAncillaryQubitAfterMergeOfAdjacentAncillaryQubitsNotPossible) {
    const std::string ancillaryQuantumRegisterLabel                         = "ancReg_1";
    constexpr auto    expectedInitialQubitRangeOfAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQuantumRegisterQubits = std::vector({false, false, true, false, false});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQuantumRegisterQubits, sharedInlineInformationOfAncillaryQubits));

    constexpr auto qubitRangeAppendedToAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 10U});
    const auto     expectedInitialValuesOfAppendedAncillaryQubits = std::vector({true, true, false, false, true, true});

    constexpr auto expectedFinalQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 10U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialValuesOfAppendedAncillaryQubits, sharedInlineInformationOfAncillaryQubits, qubitRangeAppendedToAncillaryQuantumRegister.firstQubitIndex, expectedFinalQubitRangeOfAncillaryQuantumRegister));

    for (qc::Qubit qubit = 0; qubit <= 10U; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(qubit, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value()) << "Expected not to be able to fetch user declared label of qubit " << std::to_string(qubit);
    }
}
// END getQubitLabel tests

// BEGIN Uncategorized annotatable quantum computation tests
TEST_F(AnnotatableQuantumComputationTestsFixture, AddingQRegStoringDataQubitsWithQubitInlineInformationNotPossibleWhenRecordingIsNotForced) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel = "qReg";
    const auto        qubitInlineStack             = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(AnnotatableQuantumComputation::QubitType::Data, expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, sharedInlinedQubitInformation));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 0U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 0U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddingQRegStoringDataQubitsWithQubitInlineInformationPossibleWhenRecordingIsForced) {
    constexpr auto expectedQubitRangeOfQReg                  = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 7U});
    const auto     associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel = "qReg";
    const auto        qubitInlineStack             = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    ASSERT_TRUE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(AnnotatableQuantumComputation::QubitType::Data, expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, sharedInlinedQubitInformation, true));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 8U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQReg));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfQReg, ExpectedQubitFlags::QubitShouldBeDataQubit | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), "testLabel", {0U}, qubitInlineStack));
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U}), "testLabel", {1U}, qubitInlineStack));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddedAncillaryQRegForSyrecVariableIsNotFusedWithPreviouslyAddedQRegOfOtherSyrecVariable) {
    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    const auto         associatedVariableLayoutOfExistingQReg = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U}, .bitwidth = 2U});
    constexpr unsigned expectedNumQubitsInExistingQReg        = 24U;
    constexpr auto     expectedQubitRangeOfExistingQReg       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 23U});
    const std::string  expectedExistingQRegLabel              = "first_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Ancillary, expectedExistingQRegLabel, expectedQubitRangeOfExistingQReg, associatedVariableLayoutOfExistingQReg, sharedInlinedQubitInformation));

    const auto         associatedVariableLayoutOfToBeAddedQReg = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInToBeAddedQReg        = 16U;
    constexpr auto     expectedQubitRangeOfToBeAddedQReg       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 24U, .lastQubitIndex = 39U});
    const std::string  expectedQRegLabelOfToBeAddedQReg        = "second_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Ancillary, expectedQRegLabelOfToBeAddedQReg, expectedQubitRangeOfToBeAddedQReg, associatedVariableLayoutOfToBeAddedQReg, sharedInlinedQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInExistingQReg + expectedNumQubitsInToBeAddedQReg);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    constexpr ExpectedQubitFlags sharedQubitFlagsOfPreliminaryAncillaryQRegs = QubitShouldBeGarbage | InlineQubitInformationShouldBeFetchable;
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfExistingQReg, sharedQubitFlagsOfPreliminaryAncillaryQRegs));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfToBeAddedQReg, sharedQubitFlagsOfPreliminaryAncillaryQRegs));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, PromotionOfAggregateQRegsOrQRegsMarkedAsStoringAncillaryQubitsDoesNotEffectGarbageOrDataQubitQRegs) {
    const auto        variableLayoutOfGarbageQubitsQReg     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 2U}, .bitwidth = 2U});
    constexpr auto    expectedQubitRangeOfGarbageQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 7U});
    const std::string expectedLabelOfGarbageQubitsQReg      = "garbage_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Garbage, expectedLabelOfGarbageQubitsQReg, expectedQubitRangeOfGarbageQubitsQReg, variableLayoutOfGarbageQubitsQReg));

    const auto        variableLayoutOfAncillaryQubitsQReg     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 3U});
    constexpr auto    expectedQubitRangeOfAncillaryQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 10U});
    const std::string expectedLabelOfAncillaryQubitsQReg      = "ancillary_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Ancillary, expectedLabelOfAncillaryQubitsQReg, expectedQubitRangeOfAncillaryQubitsQReg, variableLayoutOfAncillaryQubitsQReg));

    const auto        initialStateOfFirstAggregateAncillaryQubitsCollection   = std::vector({true, false, true, false});
    constexpr auto    expectedQubitRangeOfFirstAggregateOfAncillaryQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 11U, .lastQubitIndex = 14U});
    const std::string expectedLabelOfFirstAggregateOfAncillaryQubitsQReg      = "aggregate_qreg_1";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedLabelOfFirstAggregateOfAncillaryQubitsQReg, expectedQubitRangeOfFirstAggregateOfAncillaryQubitsQReg, initialStateOfFirstAggregateAncillaryQubitsCollection, AnnotatableQuantumComputation::InlinedQubitInformation()));

    const auto        variableLayoutOfDataQubitsQReg     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 3U});
    constexpr auto    expectedQubitRangeOfDataQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 15U, .lastQubitIndex = 17U});
    const std::string expectedLabelOfDataQubitsQReg      = "data_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Data, expectedLabelOfDataQubitsQReg, expectedQubitRangeOfDataQubitsQReg, variableLayoutOfDataQubitsQReg));

    const auto        initialStateOfSecondAggregateAncillaryQubitsCollection   = std::vector({false, true, false});
    constexpr auto    expectedQubitRangeOfSecondAggregateOfAncillaryQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 18U, .lastQubitIndex = 20U});
    const std::string expectedLabelOfSecondAggregateOfAncillaryQubitsQReg      = "aggregate_qreg_2";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedLabelOfSecondAggregateOfAncillaryQubitsQReg, expectedQubitRangeOfSecondAggregateOfAncillaryQubitsQReg, initialStateOfSecondAggregateAncillaryQubitsCollection, AnnotatableQuantumComputation::InlinedQubitInformation()));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promoteQuantumRegistersPreliminaryMarkedAsStoringAncillaryQubitsToDefinitivelyStoringAncillaryQubits());

    ASSERT_EQ(5U, annotatedQuantumComputation->getQuantumRegisters().size());
    ASSERT_EQ(21U, annotatedQuantumComputation->getNqubits());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfGarbageQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfFirstAggregateOfAncillaryQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfDataQubitsQReg, ExpectedQubitFlags::QubitShouldBeDataQubit));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfSecondAggregateOfAncillaryQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AggregateOfAncillaryQubitsQRegIsAppendedToLastAdjacentAggregateOfAncillaryQubitsQRegIfNoSubsequentQRegExists) {
    const auto        variableLayoutOfAncillaryQubitsQReg     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 3U});
    constexpr auto    expectedQubitRangeOfAncillaryQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 2U});
    const std::string expectedLabelOfAncillaryQubitsQReg      = "ancillary_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Ancillary, expectedLabelOfAncillaryQubitsQReg, expectedQubitRangeOfAncillaryQubitsQReg, variableLayoutOfAncillaryQubitsQReg));

    const auto        initialStateOfAncillaryQubitsOfSkippedAggregateQReg = std::vector({true, false});
    constexpr auto    expectedQubitRangeOfSkippedAggregateQReg            = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 3U, .lastQubitIndex = 4U});
    const std::string expectedLabelOfSkippedAggregateQReg                 = "aggregate_qreg_1";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedLabelOfSkippedAggregateQReg, expectedQubitRangeOfSkippedAggregateQReg, initialStateOfAncillaryQubitsOfSkippedAggregateQReg, AnnotatableQuantumComputation::InlinedQubitInformation()));

    const auto        variableLayoutOfDataQubitsQReg     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 3U});
    constexpr auto    expectedQubitRangeOfDataQubitsQReg = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 7U});
    const std::string expectedLabelOfDataQubitsQReg      = "data_qreg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Data, expectedLabelOfDataQubitsQReg, expectedQubitRangeOfDataQubitsQReg, variableLayoutOfDataQubitsQReg));

    const auto        initialStatesOfAncillaryQubitsOfToBeAppendedToQReg = std::vector({true, false});
    constexpr auto    expectedQubitRangeOfToBeAppendedToQReg             = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 9U});
    const std::string expectedLabelOfToBeAppendedToQReg                  = "aggregate_qreg_2";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAggregateOfAncillaryQubitsQRegIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedLabelOfToBeAppendedToQReg, expectedQubitRangeOfToBeAppendedToQReg, initialStatesOfAncillaryQubitsOfToBeAppendedToQReg, AnnotatableQuantumComputation::InlinedQubitInformation()));

    const auto          initialStatesOfAncillaryQubitsToBeAppended   = std::vector({false, false, false});
    constexpr auto      expectedQubitRangeOfAppendedToQRegAfterMerge = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 12U});
    constexpr qc::Qubit firstAppendedQubit                           = 10U;
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, expectedLabelOfToBeAppendedToQReg, initialStatesOfAncillaryQubitsToBeAppended, AnnotatableQuantumComputation::InlinedQubitInformation(), firstAppendedQubit, expectedQubitRangeOfAppendedToQRegAfterMerge));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promoteQuantumRegistersPreliminaryMarkedAsStoringAncillaryQubitsToDefinitivelyStoringAncillaryQubits());

    ASSERT_EQ(4U, annotatedQuantumComputation->getQuantumRegisters().size());
    ASSERT_EQ(13U, annotatedQuantumComputation->getNqubits());
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAncillaryQubitsQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfSkippedAggregateQReg, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfDataQubitsQReg, ExpectedQubitFlags::QubitShouldBeDataQubit));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfAppendedToQRegAfterMerge, ExpectedQubitFlags::QubitShouldBeGarbage | ExpectedQubitFlags::QubitShouldBeAncillary | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, UserDeclaredQubitLabelFetchableForDataQubitsIfRecordingOfInlinedQubitInformationWasForced) {
    const auto        variableLayoutOfQReg           = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 3U});
    constexpr auto    expectedQubitRangeOfQReg       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 2U});
    const std::string expectedQuantumRegisterLabel   = "qReg";
    const std::string expectedUserDeclaredQubitLabel = "userLabel";

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    const auto qubitInlineInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = expectedUserDeclaredQubitLabel, .inlineStack = inlineStack});

    ASSERT_TRUE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(AnnotatableQuantumComputation::QubitType::Data, expectedQuantumRegisterLabel, variableLayoutOfQReg, qubitInlineInformation, true));
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 3U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);

    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQReg));
    ASSERT_NO_FATAL_FAILURE(assertExpectedQubitFlagsMatchForQubitRange(*annotatedQuantumComputation, expectedQubitRangeOfQReg, ExpectedQubitFlags::QubitShouldBeDataQubit | ExpectedQubitFlags::InlineQubitInformationShouldBeFetchable));

    for (qc::Qubit qubit = 0U; qubit <= 2U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::UserDeclared, qubit, expectedUserDeclaredQubitLabel, {0U}, qubit));
    }
}
// BEGIN Uncategorized annotatable quantum computation tests

// BEGIN AddXGate tests
TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGate) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 2;
    constexpr qc::Qubit expectedTargetQubitIndex     = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithUnknownControlQubit) {
    constexpr qc::Qubit expectedUnknownControlQubitIndex = 2;
    constexpr qc::Qubit expectedKnownControlQubitIndex   = 1;
    constexpr qc::Qubit expectedTargetQubitIndex         = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedUnknownControlQubitIndex, expectedKnownControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedKnownControlQubitIndex, expectedUnknownControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithDuplicateControlQubitPossible) {
    constexpr qc::Qubit expectedControlQubitIndex = 1;
    constexpr qc::Qubit expectedTargetQubitIndex  = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndex, expectedControlQubitIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithTargetLineBeingEqualToEitherControlQubitNotPossible) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexTwo));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithUnknownTargetLine) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    constexpr qc::Qubit unknownQubitIndex            = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, unknownQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 6U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = 3;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = 4;
    constexpr qc::Qubit expectedGateTargetQubitIndex     = 5;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithTargetLineMatchingActiveControlQubitInAnyParentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = 2;
    constexpr qc::Qubit expectedGateControlQubitTwo      = 3;
    constexpr qc::Qubit expectedTargetQubitIndex         = expectedControlQubitIndexTwo;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwo, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithControlQubitsBeingDisabledInCurrentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedGateControlQubitIndex = 2;
    constexpr qc::Qubit expectedTargetQubitIndex      = 3;

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));
    auto expectedOperationForToffoliGateWithBothControlQubitsDeregistered = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForToffoliGateWithBothControlQubitsDeregistered));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedGateControlQubitIndex, expectedTargetQubitIndex));
    auto expectedOperationForToffoliGateWithFirstControlQubitsDeregistered = std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex, expectedControlQubitIndexOne}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForToffoliGateWithFirstControlQubitsDeregistered));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedGateControlQubitIndex, expectedTargetQubitIndex));
    auto expectedOperationForToffoliGateWithSecondControlQubitsDeregistered = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedGateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForToffoliGateWithSecondControlQubitsDeregistered));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithScopeActivatingDeactivatedControlQubitOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedTargetQubitIndex = 2;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithDeactivationOfControlQubitPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    constexpr qc::Qubit expectedTargetQubitIndex = 2;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithTargetLineMatchingDeactivatedControlQubitOfPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = expectedControlQubitIndexThree;
    constexpr qc::Qubit expectedTargetQubitIndex         = expectedControlQubitIndexOne;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithCallerProvidedControlQubitsMatchingDeregisteredControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    constexpr qc::Qubit expectedTargetQubitIndex       = 4;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 5U));

    constexpr qc::Qubit propagatedControlQubit = expectedControlQubitIndexThree;
    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexFour));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubit));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(propagatedControlQubit));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitOneIndex = expectedControlQubitIndexOne;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = expectedControlQubitIndexTwo;

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedTargetQubitIndex));
    auto expectedOperationForFirstToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForFirstToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubit));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedTargetQubitIndex));
    auto expectedOperationForSecondToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({propagatedControlQubit, expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForSecondToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithQuantumOperationAnnotationsFeatureDisabledPossible) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit controlQubitOneIndex = 0U;
    constexpr qc::Qubit controlQubitTwoIndex = 1U;
    constexpr qc::Qubit targetQubitIndex     = 2U;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 3U));
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingToffoliGate(controlQubitOneIndex, controlQubitTwoIndex, targetQubitIndex));

    const auto expectedToffoliGateControlLines = qc::Controls({controlQubitOneIndex, controlQubitTwoIndex});
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(expectedToffoliGateControlLines, targetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGate) {
    constexpr qc::Qubit expectedControlQubitIndex = 0;
    constexpr qc::Qubit expectedTargetQubitIndex  = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithUnknownControlQubit) {
    constexpr qc::Qubit expectedControlQubitIndex = 1;
    constexpr qc::Qubit expectedTargetQubitIndex  = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithUnknownTargetLine) {
    constexpr qc::Qubit expectedControlQubitIndex = 0;
    constexpr qc::Qubit expectedTargetQubitIndex  = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithControlAndTargetLineBeingSameLine) {
    constexpr qc::Qubit expectedControlQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedControlQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    constexpr qc::Qubit expectedTargetQubitIndex       = 4;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 5U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndexFour, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithTargetLineMatchingActiveControlQubitInAnyParentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = expectedControlQubitIndexOne;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithControlQubitBeingDeactivatedInCurrentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = 2;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithDeactivationOfControlQubitPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = 2;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex, expectedControlQubitIndexOne}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithTargetLineMatchingDeactivatedControlQubitOfPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = expectedControlQubitIndexOne;

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithCallerProvidedControlQubitsMatchingDeregisteredControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexOne;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = expectedControlQubitIndexThree;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    auto                                        expectedOperationForFirstCnotGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForFirstCnotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    constexpr qc::Qubit propagatedControlQubitIndex = expectedControlQubitIndexTwo;
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    auto expectedOperationForSecondCnotGate = std::make_unique<qc::StandardOperation>(qc::Controls({propagatedControlQubitIndex, expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForSecondCnotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithQuantumOperationAnnotationsFeatureDisabledPossible) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit controlQubitIndex = 0U;
    constexpr qc::Qubit targetQubitIndex  = 1U;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 2U));
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingCnotGate(controlQubitIndex, targetQubitIndex));

    const auto expectedCnotGateControlLines = qc::Controls({controlQubitIndex});
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(expectedCnotGateControlLines, targetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingNotGate) {
    constexpr qc::Qubit expectedTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(0));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithUnknownTargetLine) {
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingNotGate(0));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 5U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexFour));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    constexpr qc::Qubit expectedTargetQubitIndex = 4;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithTargetLineMatchingActiveControlQubitInAnyParentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexOne;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithTargetLineMatchingDeactivatedControlQubitOfControlQubitPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexOne;

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithQuantumOperationAnnotationsFeatureDisabledPossible) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitIndex = 0U;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 1U));
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingNotGate(targetQubitIndex));

    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGate) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 2;
    constexpr qc::Qubit expectedControlQubitIndexThree = 3;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));

    const qc::Controls gateControlQubitsIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(gateControlQubitsIndices, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(gateControlQubitsIndices, expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithUnknownControlQubit) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit unknownControlQubit            = 3;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    const qc::Controls expectedGateControlQubitIndices({expectedControlQubitIndexOne, unknownControlQubit, expectedControlQubitIndexThree});
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithUnknownTargetLine) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedTargetQubitIndex       = 3;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    const qc::Controls expectedGateControlQubitIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithoutControlQubitsAndNoActiveLocalControlQubitScopes) {
    constexpr qc::Qubit expectedTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({}, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 5U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    constexpr qc::Qubit expectedGateControlQubitIndex = 3;
    constexpr qc::Qubit expectedTargetQubitIndex      = 4;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({expectedGateControlQubitIndex}, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedGateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithTargetLineMatchingActiveControlQubitsOfAnyParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    const qc::Controls  expectedGateControlQubitIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo});
    constexpr qc::Qubit targetQubit = expectedControlQubitIndexOne;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, targetQubit));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithTargetLineBeingEqualToUserProvidedControlQubit) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit targetQubit                    = expectedControlQubitIndexTwo;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    const qc::Controls expectedGateControlQubitIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, targetQubit));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithTargetLineMatchingDeactivatedControlQubitOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    // The multi control toffoli gate should be created due to the target line only overlapping a deactivated control line in the current control line propagation scope
    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexOne;
    const qc::Controls  expectedGateControlQubitIndices({expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(expectedGateControlQubitIndices, expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithCallerProvidedControlQubitsMatchingDeregisteredControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    constexpr qc::Qubit expectedTargetQubitIndex       = 4;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 5U));

    constexpr qc::Qubit propagatedControlQubitIndex = expectedControlQubitIndexThree;
    constexpr qc::Qubit notPropagatedControlQubit   = expectedControlQubitIndexFour;

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexFour));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(propagatedControlQubitIndex));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(notPropagatedControlQubit));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(notPropagatedControlQubit));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = propagatedControlQubitIndex;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = expectedControlQubitIndexTwo;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    auto                                        operationForFirstMultiControlToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo, expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(operationForFirstMultiControlToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({expectedGateControlQubitTwoIndex}, expectedTargetQubitIndex));

    auto operationForSecondMultiControlToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo, propagatedControlQubitIndex, expectedGateControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(operationForSecondMultiControlToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithQuantumOperationAnnotationsFeatureDisabledPossible) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit controlQubitOneIndex = 0U;
    constexpr qc::Qubit controlQubitTwoIndex = 1U;
    constexpr qc::Qubit targetQubitIndex     = 2U;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 3U));

    const auto actualMultiControlToffoliGateControlLines = qc::Controls({controlQubitOneIndex, controlQubitTwoIndex});
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingMultiControlToffoliGate(actualMultiControlToffoliGateControlLines, targetQubitIndex));

    const auto& expectedToffoliGateControlLines = actualMultiControlToffoliGateControlLines;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(expectedToffoliGateControlLines, targetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingFredkinGate) {
    constexpr qc::Qubit expectedTargetQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(expectedTargetQubitIndexOne, expectedTargetQubitIndexTwo));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), qc::Targets({expectedTargetQubitIndexOne, expectedTargetQubitIndexTwo}), qc::OpType::SWAP));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithUnknownTargetLine) {
    constexpr qc::Qubit knownTargetQubitIndex   = 0;
    constexpr qc::Qubit unknownTargetQubitIndex = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(knownTargetQubitIndex, unknownTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(unknownTargetQubitIndex, knownTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithTargetLinesTargetingSameLine) {
    constexpr qc::Qubit expectedTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(expectedTargetQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithTargetLineMatchingActiveControlQubitOfAnyParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit notOverlappingTargetQubitIndex = 2;
    constexpr qc::Qubit overlappingTargetQubitIndex    = expectedControlQubitIndexTwo;

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(notOverlappingTargetQubitIndex, overlappingTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(overlappingTargetQubitIndex, notOverlappingTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(overlappingTargetQubitIndex, overlappingTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithTargetLineMatchingDeactivatedControlQubitOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    // The fredkin gate should be created due to the target line only overlapping a deactivated control line in the current control line propagation scope
    constexpr qc::Qubit notOverlappingTargetQubitIndex = 2;
    constexpr qc::Qubit overlappingTargetQubitIndex    = expectedControlQubitIndexOne;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(notOverlappingTargetQubitIndex, overlappingTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    auto                                        operationImplementingFirstFredkinGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo}), qc::Targets({notOverlappingTargetQubitIndex, overlappingTargetQubitIndex}), qc::OpType::SWAP);
    expectedQuantumComputations.emplace_back(std::move(operationImplementingFirstFredkinGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(overlappingTargetQubitIndex, notOverlappingTargetQubitIndex));

    auto operationImplementingSecondFredkinGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo}), qc::Targets({overlappingTargetQubitIndex, notOverlappingTargetQubitIndex}), qc::OpType::SWAP);
    expectedQuantumComputations.emplace_back(std::move(operationImplementingSecondFredkinGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithQuantumOperationAnnotationsFeatureDisabledPossible) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOne = 0U;
    constexpr qc::Qubit targetQubitTwo = 1U;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 2U));
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingFredkinGate(targetQubitOne, targetQubitTwo));

    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), qc::Targets({targetQubitOne, targetQubitTwo}), qc::OpType::SWAP));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}
// END AddXGate tests

// BEGIN Control line propagation scopes tests
TEST_F(AnnotatableQuantumComputationTestsFixture, RegisterDuplicateControlQubitOfParentScopeInLocalControlQubitScope) {
    constexpr qc::Qubit parentScopeControlQubitIndex = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(parentScopeControlQubitIndex));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(parentScopeControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls(), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({parentScopeControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RegisterDuplicateControlQubitDeactivatedOfParentScopeInLocalScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls(), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RegisterControlQubitNotKnownInCircuit) {
    constexpr qc::Qubit expectedTargetQubitIndex         = 0;
    constexpr qc::Qubit expectedKnownControlQubitIndex   = 1;
    constexpr qc::Qubit expectedUnknownControlQubitIndex = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_FALSE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedUnknownControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RegisterControlQubitWithNoActivateControlQubitScopeWillCreateNewScope) {
    constexpr qc::Qubit expectedControlQubitOneIndex = 0;
    constexpr qc::Qubit expectedControlQubitTwoIndex = 1;
    constexpr qc::Qubit expectedTargetQubitIndex     = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    auto expectedOperationForFirstAddedNotGate = std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumComputations.emplace_back(std::move(expectedOperationForFirstAddedNotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitOneIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    auto expectedOperationForSecondAddedNotGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitOneIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumComputations.emplace_back(std::move(expectedOperationForSecondAddedNotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitOneIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitTwoIndex, expectedTargetQubitIndex));

    auto expectedOperationForThirdAddedCnotGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumComputations.emplace_back(std::move(expectedOperationForThirdAddedCnotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeregisterControlQubitOfLocalControlQubitScope) {
    constexpr qc::Qubit expectedTargetQubitIndex     = 0;
    constexpr qc::Qubit activateControlQubitIndex    = 1;
    constexpr qc::Qubit deactivatedControlQubitIndex = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(deactivatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(activateControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(deactivatedControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({activateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeregisterControlQubitOfParentScopeInLastActivateControlQubitScope) {
    constexpr qc::Qubit expectedTargetQubitIndex     = 0;
    constexpr qc::Qubit activateControlQubitIndex    = 1;
    constexpr qc::Qubit deactivatedControlQubitIndex = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(deactivatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(activateControlQubitIndex));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(deactivatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(deactivatedControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({activateControlQubitIndex}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({activateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeregisterControlQubitNotKnownInCircuit) {
    constexpr qc::Qubit expectedTargetQubitIndex         = 0;
    constexpr qc::Qubit expectedKnownControlQubitIndex   = 1;
    constexpr qc::Qubit expectedUnknownControlQubitIndex = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_FALSE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedUnknownControlQubitIndex));
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedUnknownControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeregisterControlQubitOfParentPropagationScopeNotRegisteredInCurrentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    constexpr qc::Qubit expectedTargetQubitIndex     = 2;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 3U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    // Deregistering a not registered control line should not modify the aggregate of all activate control lines
    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({expectedControlQubitIndexOne}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RegisteringLocalControlQubitDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    annotatedQuantumComputation->activateControlQubitPropagationScope();

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeactivatingLocalControlQubitDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ActivatingControlQubitPropagationScopeDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeactivatingControlQubitPropagationScopeDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedTargetQubitIndex       = 3;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexThree;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedGateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    annotatedQuantumComputation->deactivateControlQubitPropagationScope();
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeactivateControlQubitPropagationScopeRegisteringControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 2;
    constexpr qc::Qubit expectedControlQubitIndexThree = 3;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeactivateControlQubitPropagationScopeNotRegisteringControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 2;
    constexpr qc::Qubit expectedControlQubitIndexThree = 3;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, DeactivatingControlQubitPropagationScopeWithNoActivatePropagationScopesIsEqualToNoOp) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    annotatedQuantumComputation->deactivateControlQubitPropagationScope();
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}
// BEGIN Control line propagation scopes tests

// BEGIN Annotation tests
TEST_F(AnnotatableQuantumComputationTestsFixture, SetAnnotationsForQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string annotationKey   = "KEY";
    const std::string annotationValue = "InitialValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, annotationKey, annotationValue));

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{annotationKey, annotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, UpdateAnnotationsForQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string firstAnnotationKey          = "KEY_ONE";
    const std::string initialFirstAnnotationValue = "InitialValue";

    const std::string secondAnnotationKey          = "KEY_TWO";
    const std::string initialSecondAnnotationValue = "OtherValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, initialFirstAnnotationValue));
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, secondAnnotationKey, initialSecondAnnotationValue));

    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{firstAnnotationKey, initialFirstAnnotationValue}, {secondAnnotationKey, initialSecondAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string updatedAnnotationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, updatedAnnotationValue));

    expectedAnnotationsOfFirstQuantumOperation[firstAnnotationKey] = updatedAnnotationValue;
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SetAnnotationForUnknownQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string annotationKey   = "KEY";
    const std::string annotationValue = "VALUE";

    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(2, annotationKey, annotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, UpdateNotExistingAnnotationsForQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string firstAnnotationKey          = "KEY_ONE";
    const std::string initialFirstAnnotationValue = "InitialValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, initialFirstAnnotationValue));

    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsForFirstQuantumOperation = {{firstAnnotationKey, initialFirstAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsForFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string secondAnnotationKey          = "KEY_TWO";
    const std::string initialSecondAnnotationValue = "OtherValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, secondAnnotationKey, initialSecondAnnotationValue));
    expectedAnnotationsForFirstQuantumOperation[secondAnnotationKey] = initialSecondAnnotationValue;

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsForFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SetAnnotationsForQuantumOperationWithEmptyKey) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string firstAnnotationKey          = "KEY_ONE";
    const std::string initialFirstAnnotationValue = "InitialValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, initialFirstAnnotationValue));

    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{firstAnnotationKey, initialFirstAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string valueForAnnotationWithEmptyKey = "OtherValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, "", valueForAnnotationWithEmptyKey));
    expectedAnnotationsOfFirstQuantumOperation[""] = valueForAnnotationWithEmptyKey;

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SettingAnnotationForQuantumOperationNotAllowedIfQuantumOperationAnnotationsCannotBeGenerated) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 1U));

    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);

    ASSERT_FALSE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.setOrUpdateAnnotationOfQuantumOperation(0U, "ANNOTATION_KEY", "ANNOTATION_VALUE"));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SetGlobalQuantumOperationAnnotation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string globalAnnotationKey   = "KEY_ONE";
    const std::string globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsForSecondQuantumOperation = {{globalAnnotationKey, globalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsForSecondQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, UpdateGlobalQuantumOperationAnnotation) {
    const std::string globalAnnotationKey          = "KEY_ONE";
    const std::string initialGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string updatedGlobalAnnoatationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, updatedGlobalAnnoatationValue));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{globalAnnotationKey, updatedGlobalAnnoatationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumComputation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, UpdateNotExistingGlobalQuantumOperationAnnotation) {
    const std::string firstGlobalAnnotationKey   = "KEY_ONE";
    const std::string firstGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(firstGlobalAnnotationKey, firstGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{firstGlobalAnnotationKey, firstGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string secondGlobalAnnotationKey   = "KEY_TWO";
    const std::string secondGlobalAnnotationValue = "OtherValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(secondGlobalAnnotationKey, secondGlobalAnnotationValue));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{firstGlobalAnnotationKey, firstGlobalAnnotationValue}, {secondGlobalAnnotationKey, secondGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumComputation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RemoveGlobalQuantumOperationAnnotation) {
    const std::string globalAnnotationKey          = "KEY_ONE";
    const std::string initialGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    ASSERT_TRUE(annotatedQuantumComputation->removeGlobalQuantumOperationAnnotation(globalAnnotationKey));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RemoveNotExistingGlobalQuantumOperationAnnotation) {
    const std::string firstGlobalAnnotationKey   = "KEY_ONE";
    const std::string firstGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(firstGlobalAnnotationKey, firstGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{firstGlobalAnnotationKey, firstGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string unknownGlobalAnnotationKey = "KEY_TWO";
    ASSERT_FALSE(annotatedQuantumComputation->removeGlobalQuantumOperationAnnotation(unknownGlobalAnnotationKey));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string secondGlobalAnnotationKey   = "KEY_TWO";
    const std::string secondGlobalAnnotationValue = "OtherValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(secondGlobalAnnotationKey, secondGlobalAnnotationValue));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{firstGlobalAnnotationKey, firstGlobalAnnotationValue}, {secondGlobalAnnotationKey, secondGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumComputation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RemovalOfGlobalQuantumOperationAnnotationNotAllowedIfQuantumOperationAnnotationsCannotBeGenerated) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 1U));

    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);

    ASSERT_FALSE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.removeGlobalQuantumOperationAnnotation("NOT_EXISTING_KEY"));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SetGlobalQuantumOperationAnnotationWithEmptyKey) {
    const std::string globalAnnotationKey          = "KEY_ONE";
    const std::string initialGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string valueOfAnnotationWithEmptyKey = "OtherValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation("", valueOfAnnotationWithEmptyKey));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}, {"", valueOfAnnotationWithEmptyKey}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumComputation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SetGlobalQuantumOperationAnnotationMatchingExistingAnnotationOfGateDoesNotUpdateTheLatter) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string localAnnotationKey   = "KEY_ONE";
    const std::string localAnnotationValue = "LocalValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, localAnnotationValue));
    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{localAnnotationKey, localAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    const std::string& globalAnnotationKey   = localAnnotationKey;
    const std::string  globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumOperation = {{globalAnnotationKey, globalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, SettingGlobalQuantumOperationAnnotationNotAllowedIfQuantumOperationAnnotationsCannotBeGenerated) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 1U));

    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    const auto expectedAnnotationsOfAddedQuantumOperation = AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup();
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);

    ASSERT_FALSE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.setOrUpdateGlobalQuantumOperationAnnotation("NOT_EXISTING_KEY", "A_VALUE"));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 0, expectedAnnotationsOfAddedQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, RemovingGlobalQuantumOperationAnnotationMatchingExistingAnnotationOfGateDoesNotRemoveTheLatter) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string localAnnotationKey   = "KEY_ONE";
    const std::string localAnnotationValue = "LocalValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, localAnnotationValue));
    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{localAnnotationKey, localAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    const std::string& globalAnnotationKey   = localAnnotationKey;
    const std::string  globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    ASSERT_TRUE(annotatedQuantumComputation->removeGlobalQuantumOperationAnnotation(globalAnnotationKey));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
}

TEST_F(AnnotatableQuantumComputationTestsFixture, UpdateLocalAnnotationWhoseKeyMatchesGlobalAnnotationDoesOnlyUpdateLocalAnnotation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string localAnnotationKey   = "KEY_ONE";
    const std::string localAnnotationValue = "LocalValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, localAnnotationValue));
    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{localAnnotationKey, localAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    const std::string& globalAnnotationKey   = localAnnotationKey;
    const std::string  globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumOperation = {{globalAnnotationKey, globalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumOperation);

    const std::string updatedLocalAnnotationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, updatedLocalAnnotationValue));
    expectedAnnotationsOfFirstQuantumOperation[localAnnotationKey] = updatedLocalAnnotationValue;

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumOperation);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetAnnotationsOfUnknownQuantumOperationInQuantumComputation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup annotationsForUnknownQuantumOperation = annotatedQuantumComputation->getAnnotationsOfQuantumOperation(2);
    ASSERT_TRUE(annotationsForUnknownQuantumOperation.empty());
}
// END Annotation tests

// BEGIN Replay operations tests
TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexLargerThanSecondIndexAndBothIndicesReferenceExistingOperations) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_TRUE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(2U, 1U));
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(2)->clone());
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(1)->clone());
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    for (const auto quantumOperationIdx: std::views::iota(0U, 6U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexLargerThanSecondIndexAndFirstIndexBeingInvalidDoesNotReplayAnyOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_FALSE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(4U, 1U));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexLargerThanSecondIndexAndSecondIndexBeingInvalidDoesNotReplayAnyOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_FALSE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(6U, 4U));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexSmallerThanSecondIndexAndBothIndicesReferenceExistingOperations) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_TRUE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(1U, 3U));
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(1)->clone());
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(2)->clone());
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(3)->clone());
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    for (const auto quantumOperationIdx: std::views::iota(0U, 7U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexSmallerThanSecondIndexAndFirstIndexBeingInvalidDoesNotReplayAnyOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_FALSE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(4U, 6U));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexSmallerThanSecondIndexAndSecondIndexBeingInvalidDoesNotReplayAnyOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_FALSE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(1U, 4U));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithFirstIndexEqualToSecondIndexAndBothIndicesReferenceExistingOperations) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit thirdGateTargetQubitIndex    = 2;
    const qc::Controls  thirdGateControlQubitIndices = {firstGateTargetQubitIndex, secondGateTargetQubitIndex};

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(thirdGateControlQubitIndices, thirdGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(thirdGateControlQubitIndices, thirdGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit fourthGateTargetQubitIndex = 3;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(fourthGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), fourthGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }

    ASSERT_TRUE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(2U, 2U));
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(2)->clone());
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    for (const auto quantumOperationIdx: std::views::iota(0U, 5U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, quantumOperationIdx, {});
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsDoesNotCopyAnnotationsOfAlreadyExistingOperations) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 2U));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    const std::string firstGateLocalAnnotationKey   = "firstAnnotation";
    const std::string firstGateLocalAnnotationValue = "A value";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstGateLocalAnnotationKey, firstGateLocalAnnotationValue));

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumGate = {{firstGateLocalAnnotationKey, firstGateLocalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0U, expectedAnnotationsOfFirstQuantumGate);

    const std::string globalAnnotationKey          = "globalAnnotation";
    const std::string initialGlobalAnnotationValue = "initialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    const std::string secondGateLocalAnnotationKey   = "secondAnnotation";
    const std::string secondGateLocalAnnotationValue = "another value";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(1, secondGateLocalAnnotationKey, secondGateLocalAnnotationValue));

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumGate = {{globalAnnotationKey, initialGlobalAnnotationValue}, {secondGateLocalAnnotationKey, secondGateLocalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumGate);
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string updatedGlobalAnnotationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, updatedGlobalAnnotationValue));
    ASSERT_TRUE(annotatedQuantumComputation->replayOperationsAtGivenIndexRange(0U, 1U));
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(0)->clone());
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(1)->clone());
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string localAnnotationOfFirstReplayedGateKey   = "thirdAnnotation";
    const std::string localAnnotationOfFirstReplayedGateValue = "yet another value";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(2, localAnnotationOfFirstReplayedGateKey, localAnnotationOfFirstReplayedGateValue));
    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsForFirstReplayedQuantumGate  = {{globalAnnotationKey, updatedGlobalAnnotationValue}, {localAnnotationOfFirstReplayedGateKey, localAnnotationOfFirstReplayedGateValue}};
    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsForSecondReplayedQuantumGate = {{globalAnnotationKey, updatedGlobalAnnotationValue}};

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumGate);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumGate);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 2, expectedAnnotationsForFirstReplayedQuantumGate);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 3, expectedAnnotationsForSecondReplayedQuantumGate);
}

TEST_F(AnnotatableQuantumComputationTestsFixture, ReplayQuantumOperationsWithQuantumOperationAnnotationFeatureDisabledPossible) {
    auto                                        annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled = AnnotatableQuantumComputation(false);
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit firstGateTargetQubitIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, 2U));
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingNotGate(firstGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), firstGateTargetQubitIndex, qc::OpType::X));

    constexpr qc::Qubit secondGateTargetQubitIndex = 1;
    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.addOperationsImplementingNotGate(secondGateTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), secondGateTargetQubitIndex, qc::OpType::X));

    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);
    for (const auto quantumOperationIdx: std::views::iota(0U, 2U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, quantumOperationIdx, {});
    }

    ASSERT_TRUE(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled.replayOperationsAtGivenIndexRange(0U, 1U));
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(0)->clone());
    expectedQuantumComputations.emplace_back(expectedQuantumComputations.at(1)->clone());
    assertThatOperationsOfQuantumComputationAreEqualToSequence(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, expectedQuantumComputations);

    for (const auto quantumOperationIdx: std::views::iota(0U, 4U)) {
        assertThatAnnotationsOfQuantumOperationAreEqualTo(annotatableQuantumComputationWithQuantumOperationAnnotationsGenerationDisabled, quantumOperationIdx, {});
    }
}
// END Replay operations tests

TEST_F(AnnotatableQuantumComputationTestsFixture, GetQuantumOperationUsingOutOfRangeIndexNotPossible) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 1U));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_THAT(annotatedQuantumComputation->getQuantumOperation(2), testing::IsNull());
    // Since we are using zero-based indices, an index equal to the number of quantum operations in the quantum computation should also not work
    ASSERT_THAT(annotatedQuantumComputation->getQuantumOperation(1), testing::IsNull());
}

// BEGIN getInlineQubitInformation tests
TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineInformationOfUnknownQubit) {
    ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(0U).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineQubitInformationForQubitForWhichSuchInformationWasNotRecordedPossibleWithoutError) {
    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitType::Garbage, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubit, std::nullopt));
    }
}
// END getInlineQubitInformation tests
