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
#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// The current tests do not cover the functionality:
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

    void SetUp() override {
        annotatedQuantumComputation = std::make_unique<AnnotatableQuantumComputation>(true);
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

    static void assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& expectedQuantumRegisterLabel, const AnnotatableQuantumComputation::QubitIndexRange expectedQubitRangeOfRegister, const AnnotatableQuantumComputation::AssociatedVariableLayoutInformation& associatedVariableLayoutInformation, const bool areGeneratedQubitsGarbage, const std::optional<AnnotatableQuantumComputation::InlinedQubitInformation>& optionalSharedInlinedQubitInformation = std::nullopt) {
        ASSERT_NE(expectedQuantumRegisterLabel, DEFAULT_QUANTUM_REGISTER_LABEL) << "Please do not use the default quantum register label set in the annotatable quantum computation tests";

        const auto numQubitsPriorToAdditionOfQuantumRegister       = annotatableQuantumComputation.getNqubits();
        const auto numQubitsInFutureQuantumComputation             = (expectedQubitRangeOfRegister.lastQubitIndex - expectedQubitRangeOfRegister.firstQubitIndex) + 1U;
        const auto expectedNumQubitsAfterAdditionOfQuantumRegister = numQubitsPriorToAdditionOfQuantumRegister + numQubitsInFutureQuantumComputation;

        std::optional<qc::Qubit> actualFirstQubitOfQuantumRegister;
        ASSERT_NO_FATAL_FAILURE(actualFirstQubitOfQuantumRegister = annotatableQuantumComputation.addQuantumRegisterForSyrecVariable(expectedQuantumRegisterLabel, associatedVariableLayoutInformation, areGeneratedQubitsGarbage, optionalSharedInlinedQubitInformation));
        ASSERT_TRUE(actualFirstQubitOfQuantumRegister.has_value()) << "Failed to create quantum register " << expectedQuantumRegisterLabel << " for variable";
        ASSERT_EQ(expectedQubitRangeOfRegister.firstQubitIndex, actualFirstQubitOfQuantumRegister.value()) << "Expected first qubit of quantum register " << expectedQuantumRegisterLabel << " should be equal to " << std::to_string(expectedQubitRangeOfRegister.firstQubitIndex) << " but was actually " << std::to_string(actualFirstQubitOfQuantumRegister.value());

        ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(annotatableQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfRegister));
        ASSERT_EQ(annotatableQuantumComputation.getNqubits(), expectedNumQubitsAfterAdditionOfQuantumRegister) << "Total number of qubits in quantum computation after addition of quantum register did not match";
    }

    static void assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::string& expectedQuantumRegisterLabel, const AnnotatableQuantumComputation::QubitIndexRange expectedQubitRangeOfRegister, const std::vector<bool>& expectedInitialValuesOfAncillaryQubits, const AnnotatableQuantumComputation::InlinedQubitInformation& sharedInlineQubitInformation) {
        ASSERT_NE(expectedQuantumRegisterLabel, DEFAULT_QUANTUM_REGISTER_LABEL) << "Please do not use the default quantum register label set in the annotatable quantum computation tests";

        const auto numQubitsPriorToAdditionOfQuantumRegister = annotatableQuantumComputation.getNqubits();
        const auto numQubitsInToBeAddedQuantumRegister       = (expectedQubitRangeOfRegister.lastQubitIndex - expectedQubitRangeOfRegister.firstQubitIndex) + 1U;
        ASSERT_EQ(numQubitsInToBeAddedQuantumRegister, expectedInitialValuesOfAncillaryQubits.size()) << "The number of initial states must match the number of qubits in the to be added ancillary quantum register";

        const auto        expectedNumQubitsAfterAdditionOfQuantumRegister            = numQubitsPriorToAdditionOfQuantumRegister + numQubitsInToBeAddedQuantumRegister;
        const std::size_t numQuantumOperationsPriorToAdditionOfQuantumRegister       = annotatableQuantumComputation.getNops();
        const std::size_t numAncillaryQubitsToBeInitializedToOne                     = static_cast<std::size_t>(std::ranges::count(expectedInitialValuesOfAncillaryQubits, true));
        const std::size_t expectedNumQuantumOperationsAfterAdditionOfQuantumRegister = annotatableQuantumComputation.getNops() + numAncillaryQubitsToBeInitializedToOne;

        std::optional<qc::Qubit> actualFirstQubitOfQuantumRegister;
        ASSERT_NO_FATAL_FAILURE(actualFirstQubitOfQuantumRegister = annotatableQuantumComputation.addPreliminaryAncillaryRegisterOrAppendToAdjacentOne(expectedQuantumRegisterLabel, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
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
        ASSERT_NO_FATAL_FAILURE(actualFirstQubitOfQuantumRegister = annotatableQuantumComputation.addPreliminaryAncillaryRegisterOrAppendToAdjacentOne(DEFAULT_QUANTUM_REGISTER_LABEL, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
        ASSERT_TRUE(actualFirstQubitOfQuantumRegister.has_value()) << "Failed to append ancillary qubits to adjacent ancillary quantum register " << labelOfAppendedToQuantumRegister;
        ASSERT_EQ(expectedFirstGeneratedAncillaryQubit, actualFirstQubitOfQuantumRegister.value()) << "Expected first ancillary qubit index  should be equal to " << std::to_string(expectedQubitRangeOfRegisterAfterQubitsWereAppended.firstQubitIndex) << " but was actually " << std::to_string(actualFirstQubitOfQuantumRegister.value());

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
        ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(annotatableQuantumComputation, "1dNQubitReg", expectedQubitRangeOfQuantumRegister, variableLayoutAssociatedWithQuantumRegister, false));

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
};

// BEGIN Add quantum register for SyReC variable tests
TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBeing1DVariableAndQubitsNotBeingGarbage) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, false, sharedInlinedQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBeing1DVariableAndQubitsBeingGarbage) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true, sharedInlinedQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBeing1DVariableAndNoInlinedQubitInformation) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBeingNDimensionalVariableAndQubitsNotBeingGarbage) {
    const auto         associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInVariable               = 96;

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});

    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInVariable);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBeingNDimensionalVariableAndQubitsBeingGarbage) {
    const auto         associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInVariable               = 96;

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInVariable);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBeingNDimensionalVariableAndNoInlinedQubitInformation) {
    const auto         associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInVariable               = 96;

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInVariable);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableIsNotFusedWithExistingAncillaryQuantumRegister) {
    constexpr auto     expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0, .lastQubitIndex = 4U});
    constexpr unsigned expectedNumQubitsInAncillaryQuantumRegister  = 5;
    const auto         expectedInitialStateOfAncillaryQubits        = std::vector({false, false, true, false, false});
    const std::string  expectedAncillaryquantumRegisterLabel        = "ancReg";

    const auto ancillaryQubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(ancillaryQubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = ancillaryQubitInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedAncillaryquantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialStateOfAncillaryQubits, sharedInlinedQubitInformationOfAncillaryQubits));

    const auto         associatedVariableForNonAncillaryQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {4U}, .bitwidth = 3U});
    constexpr unsigned expectedNumQubitsInNonAncillaryQuantumRegister   = 12U;
    constexpr auto     expectedQubitRangeOfNonAncillaryQuantumRegister  = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 16U});
    const std::string  expectedNonAncillaryQuantumRegisterLabel         = "nonAnc_qReg";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedNonAncillaryQuantumRegisterLabel, expectedQubitRangeOfNonAncillaryQuantumRegister, associatedVariableForNonAncillaryQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInAncillaryQuantumRegister + expectedNumQubitsInNonAncillaryQuantumRegister);
    for (qc::Qubit qubit = 0U; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }

    for (qc::Qubit qubit = expectedQubitRangeOfNonAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfNonAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableIsNotFusedWithExistingNonAncillaryQuantumRegister) {
    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});

    const auto         associatedVariableLayoutForFirstNonAncillaryQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U}, .bitwidth = 2U});
    constexpr unsigned expectedNumQubitsInFirstNonAncillaryQuantumRegister         = 24U;
    constexpr auto     expectedQubitRangeOfFirstNonAncillaryQuantumRegister        = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 23U});
    const std::string  expectedFirstNonAncillaryQubitQuantumRegisterLabel          = "nonAnc_qReg1_garbage";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedFirstNonAncillaryQubitQuantumRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister, associatedVariableLayoutForFirstNonAncillaryQuantumRegister, true, sharedInlinedQubitInformation));

    const auto         associatedVariableLayoutForSecondNonAncillaryQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 2U}, .bitwidth = 4U});
    constexpr unsigned expectedNumQubitsInSecondNonAncillaryQuantumRegister         = 16U;
    constexpr auto     expectedQubitRangeOfSecondNonAncillaryQuantumRegister        = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 24U, .lastQubitIndex = 39U});
    const std::string  expectedSecondNonAncillaryQubitQuantumRegisterLabel          = "nonAnc_qReg1";
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedSecondNonAncillaryQubitQuantumRegisterLabel, expectedQubitRangeOfSecondNonAncillaryQuantumRegister, associatedVariableLayoutForSecondNonAncillaryQuantumRegister, false, sharedInlinedQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedNumQubitsInFirstNonAncillaryQuantumRegister + expectedNumQubitsInSecondNonAncillaryQuantumRegister);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = 0U; qubit <= expectedQubitRangeOfFirstNonAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }

    for (qc::Qubit qubit = expectedQubitRangeOfSecondNonAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfSecondNonAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithVariableBitwidthEqualToZeroNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("main")})));
    const auto sharedInlinedQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = qubitInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true, sharedInlinedQubitInformation));

    const auto associatedVariableLayoutWithInvalidBitwidth = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 4U}, .bitwidth = 0U});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("aLabel", associatedVariableLayoutWithInvalidBitwidth, false, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithNumberOfValuesOfAnyDimensionEqualToZeroNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    const auto associatedVariableLayoutWithInvalidNumberOfValuesOfDimension = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 0U, 4U}, .bitwidth = 2U});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("aLabel", associatedVariableLayoutWithInvalidNumberOfValuesOfDimension, false, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithTotalNumberOfDimensionEqualToZeroNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    const auto associatedVariableLayoutWithInvalidNumberOfDimensions = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {}, .bitwidth = 2U});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("aLabel", associatedVariableLayoutWithInvalidNumberOfDimensions, false, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithEmptyQuantumRegisterLabelNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("", associatedVariableLayoutOfQuantumRegister, false, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithDuplicateQuantumRegisterLabelNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, false, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithQuantumRegisterLabelEqualToAncillaryQuantumRegisterNotPossible) {
    const std::string expectedQuantumRegisterLabel               = "qReg";
    constexpr auto    expectedAncillaryQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U});
    const auto        initialValuesOfAncillaryQubits             = {false, true, true, false};

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    const auto ancillaryQubitsSharedInlineInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedAncillaryQuantumRegisterQubitRange, initialValuesOfAncillaryQubits, ancillaryQubitsSharedInlineInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(expectedQuantumRegisterLabel, AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U}, .bitwidth = 3U}), false));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 4U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 2U);
    for (qc::Qubit qubit = 0U; qubit <= 3U; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithInvalidQubitStackInInlinedQubitInformationNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));

    const auto inlinedQubitInformationWithNullptrInlineStack = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = nullptr});
    const auto inlinedQubitInformationWithEmptyInlineStack   = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = std::make_shared<QubitInliningStack>()});

    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("aLabel", associatedVariableLayoutOfQuantumRegister, false, inlinedQubitInformationWithNullptrInlineStack).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("aLabel", associatedVariableLayoutOfQuantumRegister, false, inlinedQubitInformationWithEmptyInlineStack).has_value());

    const auto inlineStackWithInvalidEntry = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackWithInvalidEntry->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    QubitInliningStack::QubitInliningStackEntry* inlineStackEntry = inlineStackWithInvalidEntry->getStackEntryAt(0);
    ASSERT_THAT(inlineStackEntry, testing::NotNull());
    inlineStackEntry->targetModule = nullptr;

    const auto inlinedQubitInformationWithInvalidInlineStackEntry = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "testLabel", .inlineStack = inlineStackWithInvalidEntry});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("aLabel", associatedVariableLayoutOfQuantumRegister, false, inlinedQubitInformationWithInvalidInlineStackEntry).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableWithInvalidUserDeclaredQubitLabelInInlinedQubitInformationNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));

    const auto validInlineQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "test", .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true, validInlineQubitInformation));

    const auto inlineQubitInformationWithNotSetUserDeclaredQubitLabel = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, false, inlineQubitInformationWithNotSetUserDeclaredQubitLabel).has_value());

    const auto inlineQubitInformationWithEmptyUserDeclaredQubitLabel = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "", .inlineStack = inlineStack});
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable(expectedQuantumRegisterLabel, associatedVariableLayoutOfQuantumRegister, false, inlineQubitInformationWithEmptyUserDeclaredQubitLabel).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    ASSERT_EQ(annotatedQuantumComputation->getNops(), 0U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddQuantumRegisterForSyrecVariableAfterPreliminaryAncillaryQubitsWerePromotedToActualOnesNotPossible) {
    const auto associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U}, .bitwidth = 4U});

    const std::string expectedQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQuantumRegisterQubitRange, associatedVariableLayoutOfQuantumRegister, true));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits());
    ASSERT_FALSE(annotatedQuantumComputation->addQuantumRegisterForSyrecVariable("anotherQReg", associatedVariableLayoutOfQuantumRegister, false, std::nullopt).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 12U);
    for (qc::Qubit qubit = expectedQuantumRegisterQubitRange.firstQubitIndex; qubit <= expectedQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}
// END Add quantum register for SyReC variable tests

// BEGIN Add preliminary ancillary quantum register tests
TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterOfLengthOne) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 0U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 1U);
    ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(0U));
    ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(0U));
    ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(0U).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterOfLengthN) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterAndCheckAdjacentAncillaryQuantumRegistersAreMerged) {
    const std::string ancillaryQubitQuantumRegisterLabel                  = "aReg";
    constexpr auto    expectedInitialQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits              = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                        = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    const auto          expectedInitialValuesOfOtherAncillaryQubits       = std::vector({true, false, true});
    constexpr auto      expectedFinalQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 8U});
    constexpr qc::Qubit expectedFirstAppendedAncillaryQubit               = 6U;
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedInitialValuesOfOtherAncillaryQubits, sharedInlineQubitInformation, expectedFirstAppendedAncillaryQubit, expectedFinalQubitRangeOfAncillaryQuantumRegister));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 9U);
    for (qc::Qubit qubit = 0U; qubit <= 8U; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterAndCheckAdjacentNonAncillaryQuantumRegisterIsNotMerged) {
    const auto         associatedVariableLayoutOfNonAncillaryQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {3U, 4U}, .bitwidth = 3U});
    constexpr unsigned expectedNumQubitsInVariable                           = 36;

    const std::string expectedNonAncillaryQuantumRegisterLabel      = "qReg";
    constexpr auto    expectedNonAncillaryQuantumRegisterQubitRange = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = expectedNumQubitsInVariable - 1U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedNonAncillaryQuantumRegisterLabel, expectedNonAncillaryQuantumRegisterQubitRange, associatedVariableLayoutOfNonAncillaryQuantumRegister, true));

    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 36U, .lastQubitIndex = 41U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    constexpr unsigned expectedTotalNumQubits = 42;
    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), expectedTotalNumQubits);
    for (qc::Qubit qubit = 0U; qubit <= expectedNonAncillaryQuantumRegisterQubitRange.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }

    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterWithEmptyQuantumRegisterLabelNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("", expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterWithQuantumRegisterLabelMatchingExistingQuantumRegisterLabelNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne(ancillaryQubitQuantumRegisterLabel, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterWithEmptyAncillaryQubitInitialStatesNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("anotherLabel", {}, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterWithInvalidInlineStackNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));

    const auto inlinedQubitInformationWithNullptrInlineStack = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = nullptr});
    const auto inlinedQubitInformationWithEmptyInlineStack   = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = std::make_shared<QubitInliningStack>()});

    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("aLabel", expectedInitialValuesOfAncillaryQubits, inlinedQubitInformationWithNullptrInlineStack).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("aLabel", expectedInitialValuesOfAncillaryQubits, inlinedQubitInformationWithEmptyInlineStack).has_value());

    const auto inlineStackWithInvalidEntry = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackWithInvalidEntry->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("test")})));
    QubitInliningStack::QubitInliningStackEntry* inlineStackEntry = inlineStackWithInvalidEntry->getStackEntryAt(0);
    ASSERT_THAT(inlineStackEntry, testing::NotNull());
    inlineStackEntry->targetModule = nullptr;

    const auto inlinedQubitInformationWithInvalidInlineStackEntry = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStackWithInvalidEntry});
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("aLabel", expectedInitialValuesOfAncillaryQubits, inlinedQubitInformationWithInvalidInlineStackEntry).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterWithUserDeclaredQubitLabelInInlinedInformationNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});

    const auto qubitInlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(qubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = std::make_shared<Module>("test")})));

    const auto validInlineQubitInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = qubitInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, validInlineQubitInformation));

    const auto inlineQubitInformationWithEmptyUserDeclaredQubitLabel = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = "", .inlineStack = qubitInlineStack});
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("anotherLabel", expectedInitialValuesOfAncillaryQubits, inlineQubitInformationWithEmptyUserDeclaredQubitLabel).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddPreliminaryAncillaryQuantumRegisterAfterPromotionOfPreliminaryAncillaryQubitsNotPossible) {
    const std::string ancillaryQubitQuantumRegisterLabel           = "aReg";
    constexpr auto    expectedQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 5U});
    const auto        expectedInitialValuesOfAncillaryQubits       = std::vector({false, false, true, true, false, false});
    const auto        sharedInlineQubitInformation                 = AnnotatableQuantumComputation::InlinedQubitInformation();
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQubitQuantumRegisterLabel, expectedQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits());
    ASSERT_FALSE(annotatedQuantumComputation->addPreliminaryAncillaryRegisterOrAppendToAdjacentOne("anotherLabel", expectedInitialValuesOfAncillaryQubits, sharedInlineQubitInformation).has_value());

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_EQ(annotatedQuantumComputation->getNqubits(), 6U);
    for (qc::Qubit qubit = expectedQubitRangeOfAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->getInlinedQubitInformation(qubit).has_value());
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, AddMixtureOfDifferentQuantumRegisters) {
    const std::string firstNonAncillaryQubitRegisterLabel                  = "nqReg_1";
    const auto        variableLayoutOfFirstNonAncillaryQuantumRegister     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 3U}, .bitwidth = 2U});
    constexpr auto    expectedQubitRangeOfFirstNonAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister, variableLayoutOfFirstNonAncillaryQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 1U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));

    const auto        initialStateOfFirstAncillaryQubitRange                   = std::vector({false, true, true, false});
    const auto        sharedInlineQubitInformationOfAncillaryQubits            = AnnotatableQuantumComputation::InlinedQubitInformation();
    const std::string firstAncillaryQubitRegisterLabel                         = "aReg_1";
    constexpr auto    expectedInitialQubitRangeOfFirstAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 15U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedInitialQubitRangeOfFirstAncillaryQuantumRegister, initialStateOfFirstAncillaryQubitRange, sharedInlineQubitInformationOfAncillaryQubits));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedInitialQubitRangeOfFirstAncillaryQuantumRegister));

    const auto          initialStateOfFirstAddedAncillaryQubitRange                   = std::vector({true, true, true});
    constexpr qc::Qubit expectedFirstQubitOfFirstAddedAncillaryQubitOfRange           = 16U;
    constexpr auto      expectedIntermediateQubitRangeOfFirstAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 18U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, initialStateOfFirstAddedAncillaryQubitRange, sharedInlineQubitInformationOfAncillaryQubits, expectedFirstQubitOfFirstAddedAncillaryQubitOfRange, expectedIntermediateQubitRangeOfFirstAncillaryQuantumRegister));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedIntermediateQubitRangeOfFirstAncillaryQuantumRegister));

    const auto          initialStateOfSecondAddedAncillaryQubitRange           = std::vector({false, true, false, true});
    constexpr qc::Qubit expectedFirstQubitOfSecondAddedAncillaryQubitOfRange   = 19U;
    constexpr auto      expectedFinalQubitRangeOfFirstAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 22U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, initialStateOfSecondAddedAncillaryQubitRange, sharedInlineQubitInformationOfAncillaryQubits, expectedFirstQubitOfSecondAddedAncillaryQubitOfRange, expectedFinalQubitRangeOfFirstAncillaryQuantumRegister));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 2U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedFinalQubitRangeOfFirstAncillaryQuantumRegister));

    const std::string secondNonAncillaryQubitRegisterLabel                  = "nqReg_2";
    const auto        variableLayoutOfSecondNonAncillaryQuantumRegister     = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {4U}, .bitwidth = 2U});
    constexpr auto    expectedQubitRangeOfSecondNonAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 23U, .lastQubitIndex = 30U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, secondNonAncillaryQubitRegisterLabel, expectedQubitRangeOfSecondNonAncillaryQuantumRegister, variableLayoutOfSecondNonAncillaryQuantumRegister, true));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 3U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedFinalQubitRangeOfFirstAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondNonAncillaryQubitRegisterLabel, expectedQubitRangeOfSecondNonAncillaryQuantumRegister));

    const auto        initialStateOfSecondAncillaryQubitRange                   = std::vector({false, true});
    const std::string secondAncillaryQubitRegisterLabel                         = "aReg_2";
    constexpr auto    expectedInitialQubitRangeOfSecondAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 31U, .lastQubitIndex = 32U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, secondAncillaryQubitRegisterLabel, expectedInitialQubitRangeOfSecondAncillaryQuantumRegister, initialStateOfSecondAncillaryQubitRange, sharedInlineQubitInformationOfAncillaryQubits));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 4U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedFinalQubitRangeOfFirstAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondNonAncillaryQubitRegisterLabel, expectedQubitRangeOfSecondNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondAncillaryQubitRegisterLabel, expectedInitialQubitRangeOfSecondAncillaryQuantumRegister));

    const auto          initialStateOfThirdAddedAncillaryQubitRange             = std::vector({false, true, false, true});
    constexpr qc::Qubit expectedFirstQubitOfThirdAddedAncillaryQubitOfRange     = 33U;
    constexpr auto      expectedFinalQubitRangeOfSecondAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 31U, .lastQubitIndex = 36U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, secondAncillaryQubitRegisterLabel, initialStateOfThirdAddedAncillaryQubitRange, sharedInlineQubitInformationOfAncillaryQubits, expectedFirstQubitOfThirdAddedAncillaryQubitOfRange, expectedFinalQubitRangeOfSecondAncillaryQuantumRegister));

    ASSERT_EQ(annotatedQuantumComputation->getQuantumRegisters().size(), 4U);
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstNonAncillaryQubitRegisterLabel, expectedQubitRangeOfFirstNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, firstAncillaryQubitRegisterLabel, expectedFinalQubitRangeOfFirstAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondNonAncillaryQubitRegisterLabel, expectedQubitRangeOfSecondNonAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(assertQuantumRegisterExists(*annotatedQuantumComputation, secondAncillaryQubitRegisterLabel, expectedFinalQubitRangeOfSecondAncillaryQuantumRegister));
    ASSERT_NO_FATAL_FAILURE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits());

    for (qc::Qubit qubit = 0; qubit <= expectedQubitRangeOfFirstNonAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
    }

    for (qc::Qubit qubit = expectedFinalQubitRangeOfFirstAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedFinalQubitRangeOfFirstAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
    }

    for (qc::Qubit qubit = expectedQubitRangeOfSecondNonAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedQubitRangeOfSecondNonAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
    }

    for (qc::Qubit qubit = expectedFinalQubitRangeOfSecondAncillaryQuantumRegister.firstQubitIndex; qubit <= expectedFinalQubitRangeOfSecondAncillaryQuantumRegister.lastQubitIndex; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->logicalQubitIsGarbage(qubit));
        ASSERT_TRUE(annotatedQuantumComputation->logicalQubitIsAncillary(qubit));
    }
}
// END Add preliminary ancillary quantum register tests

// BEGIN getQubitLabel tests
TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelInEmptyQuantumComputation) {
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(0U, AnnotatableQuantumComputation::QubitLabelType::Internal).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfUnknownQubitNotPossible) {
    ASSERT_NO_FATAL_FAILURE(create1DQuantumRegisterContainingNQubits(*annotatedQuantumComputation, 4U));
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(4U, AnnotatableQuantumComputation::QubitLabelType::Internal).has_value());
    ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(6U, AnnotatableQuantumComputation::QubitLabelType::Internal).has_value());
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfQubitOf1DVariable) {
    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, qubit, expectedQuantumRegisterLabel, {0U}, qubit));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfQubitOfNDimensionalVariable) {
    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 29U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 3U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false));

    qc::Qubit currentlyCheckedQubit = 0U;
    for (const auto& expectedAccessedValuePerDimensionCombination: std::vector<std::vector<unsigned>>({{0U, 0U}, {0U, 1U}, {0U, 2U}, {1U, 0U}, {1U, 1U}})) {
        for (qc::Qubit relativeQubitIndexInAccessedElementOfVariableInQuantumRegister = 0; relativeQubitIndexInAccessedElementOfVariableInQuantumRegister <= 4U; ++relativeQubitIndexInAccessedElementOfVariableInQuantumRegister) {
            ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, currentlyCheckedQubit, expectedQuantumRegisterLabel, expectedAccessedValuePerDimensionCombination, relativeQubitIndexInAccessedElementOfVariableInQuantumRegister));
            ++currentlyCheckedQubit;
        }
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInternalQubitLabelOfAncillaryQubit) {
    const std::string expectedQuantumRegisterLabel           = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister    = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQubits = std::vector({false, false, true, false, false});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineInformationOfAncillaryQubits));

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
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQuantumRegisterQubits, sharedInlineInformationOfAncillaryQubits));

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

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfQubitOf1DVariable) {
    const std::string associatedVariableIdentifier = "varName";

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = associatedVariableIdentifier, .inlineStack = inlineStack});

    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false, sharedInlineInformationOfAncillaryQubits));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::UserDeclared, qubit, associatedVariableIdentifier, {0U}, qubit));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfQubitOfNDimensionalVariable) {
    const std::string associatedVariableIdentifier = "varName";

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = associatedVariableIdentifier, .inlineStack = inlineStack});

    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 29U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 3U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false, sharedInlineInformationOfAncillaryQubits));

    qc::Qubit currentlyCheckedQubit = 0U;
    for (const auto& expectedAccessedValuePerDimensionCombination: std::vector<std::vector<unsigned>>({{0U, 0U}, {0U, 1U}, {0U, 2U}, {1U, 0U}, {1U, 1U}})) {
        for (qc::Qubit relativeQubitIndexInAccessedElementOfVariableInQuantumRegister = 0; relativeQubitIndexInAccessedElementOfVariableInQuantumRegister <= 4U; ++relativeQubitIndexInAccessedElementOfVariableInQuantumRegister) {
            ASSERT_NO_FATAL_FAILURE(assertExpectedAndActualQubitLabelMatch(*annotatedQuantumComputation, AnnotatableQuantumComputation::QubitLabelType::Internal, currentlyCheckedQubit, expectedQuantumRegisterLabel, expectedAccessedValuePerDimensionCombination, relativeQubitIndexInAccessedElementOfVariableInQuantumRegister));
            ++currentlyCheckedQubit;
        }
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetUserDeclaredQubitLabelOfAncillaryQubitNotPossible) {
    const std::string expectedQuantumRegisterLabel           = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister    = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQubits = std::vector({false, false, true, false, false});

    const auto inlineStack = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<syrec::Module>("moduleLabel")})));
    const auto sharedInlineInformationOfAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedInlineInformationOfAncillaryQubits));

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
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQuantumRegisterQubits, sharedInlineInformationOfAncillaryQubits));

    constexpr auto qubitRangeAppendedToAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 10U});
    const auto     expectedInitialValuesOfAppendedAncillaryQubits = std::vector({true, true, false, false, true, true});

    constexpr auto expectedFinalQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 10U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialValuesOfAppendedAncillaryQubits, sharedInlineInformationOfAncillaryQubits, qubitRangeAppendedToAncillaryQuantumRegister.firstQubitIndex, expectedFinalQubitRangeOfAncillaryQuantumRegister));

    for (qc::Qubit qubit = 0; qubit <= 10U; ++qubit) {
        ASSERT_FALSE(annotatedQuantumComputation->getQubitLabel(qubit, AnnotatableQuantumComputation::QubitLabelType::UserDeclared).has_value()) << "Expected not to be able to fetch user declared label of qubit " << std::to_string(qubit);
    }
}
// END getQubitLabel tests

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

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineQubitInformationOfQuantumRegisterAssociatedWith1DVariable) {
    const auto sharedInlineStack     = std::make_shared<QubitInliningStack>();
    const auto firstInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = std::make_shared<Module>("mainModule")});
    ASSERT_TRUE(sharedInlineStack->push(firstInlineStackEntry));

    const auto secondInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("childModule")});
    ASSERT_TRUE(sharedInlineStack->push(secondInlineStackEntry));

    const std::string associatedVariableOfQuantumRegisterIdentifier = "var";
    const auto        sharedQubitInlineInformation                  = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = associatedVariableOfQuantumRegisterIdentifier, .inlineStack = sharedInlineStack});

    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false, sharedQubitInlineInformation));

    auto expectedInlineQubitInformationOfQubit0                   = sharedQubitInlineInformation;
    expectedInlineQubitInformationOfQubit0.userDeclaredQubitLabel = buildExpectedQubitLabel(associatedVariableOfQuantumRegisterIdentifier, {0U}, 0U);
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, 0U, expectedInlineQubitInformationOfQubit0));

    auto expectedInlineQubitInformationOfQubit1                   = sharedQubitInlineInformation;
    expectedInlineQubitInformationOfQubit1.userDeclaredQubitLabel = buildExpectedQubitLabel(associatedVariableOfQuantumRegisterIdentifier, {0U}, 1U);
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, 1U, expectedInlineQubitInformationOfQubit1));

    auto expectedInlineQubitInformationOfQubit2                   = sharedQubitInlineInformation;
    expectedInlineQubitInformationOfQubit2.userDeclaredQubitLabel = buildExpectedQubitLabel(associatedVariableOfQuantumRegisterIdentifier, {0U}, 2U);
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, 2U, expectedInlineQubitInformationOfQubit2));

    auto expectedInlineQubitInformationOfQubit3                   = sharedQubitInlineInformation;
    expectedInlineQubitInformationOfQubit3.userDeclaredQubitLabel = buildExpectedQubitLabel(associatedVariableOfQuantumRegisterIdentifier, {0U}, 3U);
    ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, 3U, expectedInlineQubitInformationOfQubit3));
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineQubitInformationOfQuantumRegisterAssociatedWithNDimensionalVariable) {
    const auto sharedInlineStack     = std::make_shared<QubitInliningStack>();
    const auto firstInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = std::make_shared<Module>("mainModule")});
    ASSERT_TRUE(sharedInlineStack->push(firstInlineStackEntry));

    const auto secondInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("childModule")});
    ASSERT_TRUE(sharedInlineStack->push(secondInlineStackEntry));

    const std::string associatedVariableOfQuantumRegisterIdentifier = "var";
    const auto        sharedQubitInlineInformation                  = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = associatedVariableOfQuantumRegisterIdentifier, .inlineStack = sharedInlineStack});

    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 11U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {2U, 2U}, .bitwidth = 3U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false, sharedQubitInlineInformation));

    qc::Qubit currentlyAccessedQubit = 0;
    for (const auto& accessedValuePerDimension: std::vector<std::vector<unsigned>>({{0U, 0U}, {0U, 1U}, {1U, 0U}, {1U, 1U}})) {
        for (qc::Qubit relativeQubitIndexInAccessedElement = 0; relativeQubitIndexInAccessedElement < 3U; ++relativeQubitIndexInAccessedElement) {
            auto expectedInlineQubitInformationOfQubit                   = sharedQubitInlineInformation;
            expectedInlineQubitInformationOfQubit.userDeclaredQubitLabel = buildExpectedQubitLabel(associatedVariableOfQuantumRegisterIdentifier, accessedValuePerDimension, relativeQubitIndexInAccessedElement);
            ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, currentlyAccessedQubit++, expectedInlineQubitInformationOfQubit));
        }
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineQubitInformationOfAncillaryQuantumRegister) {
    const std::string expectedQuantumRegisterLabel           = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister    = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQubits = std::vector({false, false, true, false, false});

    const auto sharedInlineStack     = std::make_shared<QubitInliningStack>();
    const auto firstInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = std::make_shared<Module>("mainModule")});
    ASSERT_TRUE(sharedInlineStack->push(firstInlineStackEntry));

    const auto secondInlineStackEntry = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("childModule")});
    ASSERT_TRUE(sharedInlineStack->push(secondInlineStackEntry));

    const auto sharedQubitInlineInformation = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = sharedInlineStack});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, expectedInitialValuesOfAncillaryQubits, sharedQubitInlineInformation));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubit, sharedQubitInlineInformation));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineQubitInformationOfAncillaryQuantumRegisterAfterMergeWithAdjacentAncillaryQuantumRegister) {
    const std::string ancillaryQuantumRegisterLabel                         = "ancReg_1";
    constexpr auto    expectedInitialQubitRangeOfAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        expectedInitialValuesOfAncillaryQuantumRegisterQubits = std::vector({false, false, true, false, false});

    const auto inlineStackOfInitialAncillaryQubits = std::make_shared<QubitInliningStack>();
    ASSERT_TRUE(inlineStackOfInitialAncillaryQubits->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = true, .targetModule = std::make_shared<syrec::Module>("anotherModule")})));
    const auto inlineQubitInformationOfInitialAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStackOfInitialAncillaryQubits});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQuantumRegisterIsSuccessfulWithNewRegisterCreated(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialQubitRangeOfAncillaryQuantumRegister, expectedInitialValuesOfAncillaryQuantumRegisterQubits, inlineQubitInformationOfInitialAncillaryQubits));

    const auto inlineStackOfAppendedAncillaryQubits               = std::make_shared<QubitInliningStack>();
    const auto firstInlineStackEntryInInlineStackOfAppendedQubits = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = 1U, .isTargetModuleAccessedViaCallStmt = false, .targetModule = std::make_shared<Module>("mainModule")});
    ASSERT_TRUE(inlineStackOfAppendedAncillaryQubits->push(firstInlineStackEntryInInlineStackOfAppendedQubits));

    const auto secondInlineStackEntryInInlineStackOfAppendedQubits = QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = std::make_shared<Module>("childModule")});
    ASSERT_TRUE(inlineStackOfAppendedAncillaryQubits->push(secondInlineStackEntryInInlineStackOfAppendedQubits));
    const auto inlineQubitInformationOfAppendedAncillaryQubits = AnnotatableQuantumComputation::InlinedQubitInformation({.userDeclaredQubitLabel = std::nullopt, .inlineStack = inlineStackOfAppendedAncillaryQubits});

    constexpr auto qubitRangeAppendedToAncillaryQuantumRegister   = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 5U, .lastQubitIndex = 10U});
    const auto     expectedInitialValuesOfAppendedAncillaryQubits = std::vector({true, true, false, false, true, true});

    constexpr auto expectedFinalQubitRangeOfAncillaryQuantumRegister = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 10U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfAncillaryQantumRegisterIsSuccessfulByAppendingToAdjacentQuantumRegister(*annotatedQuantumComputation, ancillaryQuantumRegisterLabel, expectedInitialValuesOfAppendedAncillaryQubits, inlineQubitInformationOfAppendedAncillaryQubits, qubitRangeAppendedToAncillaryQuantumRegister.firstQubitIndex, expectedFinalQubitRangeOfAncillaryQuantumRegister));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubit, inlineQubitInformationOfInitialAncillaryQubits));
    }

    for (qc::Qubit qubit = 5; qubit <= 10U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubit, inlineQubitInformationOfAppendedAncillaryQubits));
    }
}

TEST_F(AnnotatableQuantumComputationTestsFixture, GetInlineQubitInformationForQubitForWhichSuchInformationWasNotRecordedPossibleWithoutError) {
    const std::string expectedQuantumRegisterLabel              = "regLabel";
    constexpr auto    expectedQubitRangeOfQuantumRegister       = AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 4U});
    const auto        associatedVariableLayoutOfQuantumRegister = AnnotatableQuantumComputation::AssociatedVariableLayoutInformation({.numValuesPerDimension = {1U}, .bitwidth = 5U});
    ASSERT_NO_FATAL_FAILURE(assertAdditionOfQuantumRegisterForSyrecVariableIsSuccessful(*annotatedQuantumComputation, expectedQuantumRegisterLabel, expectedQubitRangeOfQuantumRegister, associatedVariableLayoutOfQuantumRegister, false));

    for (qc::Qubit qubit = 0; qubit <= 4U; ++qubit) {
        ASSERT_NO_FATAL_FAILURE(assertInlineQubitInformationMatchesExpectedOne(*annotatedQuantumComputation, qubit, std::nullopt));
    }
}
// END getInlineQubitInformation tests
