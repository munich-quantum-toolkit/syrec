/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/annotatable_quantum_computation.hpp"

#include "core/qubit_inlining_stack.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "nlohmann/detail/input/binary_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
    template<class ForwardIterator>
    std::optional<std::size_t> findIndexOfElementContainingQubit(ForwardIterator firstElementOfCollection, ForwardIterator lastElementOfCollection, const qc::Qubit qubitToFind) {
        static_assert(std::is_same_v<syrec::AnnotatableQuantumComputation::QubitIndexRange, typename std::iterator_traits<ForwardIterator>::value_type>);
        if (std::distance(firstElementOfCollection, lastElementOfCollection) == 0) {
            return std::nullopt;
        }

        const auto qubitRangeOfFirstElementInCollection = static_cast<syrec::AnnotatableQuantumComputation::QubitIndexRange>(*firstElementOfCollection);
        if (qubitToFind < qubitRangeOfFirstElementInCollection.firstQubitIndex) {
            return std::nullopt;
        }

        // Performs a binary search through the sorted collection of the syrec::AnnotatableQuantumComputation::QubitIndexRange elements, assumed to be sorted in ascending order based on the value of the start qubit of the index range, and returns the first element that is <= qubitToFind.
        // At this point we already checked whether the qubitToFind is smaller than the lowest qubit in qubit index range collection, if the qubitToFind is larger than the largest qubit in the qubit index range collection than the std::end() iterator of the collection is return. Otherwise,
        // the iterator points to the qubit index range that contains the qubitToFind.
        const auto iteratorToRangeElementGreaterThanQubit = std::lower_bound(firstElementOfCollection, lastElementOfCollection, qubitToFind, [](const syrec::AnnotatableQuantumComputation::QubitIndexRange& qubitIndexRangeOfElement, const qc::Qubit qubitToFind) { return qubitIndexRangeOfElement.lastQubitIndex < qubitToFind; });
        if (iteratorToRangeElementGreaterThanQubit == lastElementOfCollection) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(std::distance(firstElementOfCollection, iteratorToRangeElementGreaterThanQubit));
    }

    bool isInlineStackNotSetOrEmpty(const syrec::QubitInliningStack::ptr& inlineStackToCheck) {
        return inlineStackToCheck == nullptr || inlineStackToCheck->size() == 0;
    }
} // namespace

using namespace syrec;

bool AnnotatableQuantumComputation::addOperationsImplementingNotGate(const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(targetQubit) || aggregateOfPropagatedControlQubits.contains(targetQubit)) {
        return false;
    }

    const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    const std::size_t  prevNumQuantumOperations = getNops();
    mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations - 1U, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingCnotGate(const qc::Qubit controlQubit, const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(controlQubit) || !isQubitWithinRange(targetQubit) || controlQubit == targetQubit || aggregateOfPropagatedControlQubits.contains(targetQubit)) {
        return false;
    }

    qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    gateControlQubits.emplace(controlQubit);

    const std::size_t prevNumQuantumOperations = getNops();
    mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations - 1U, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingToffoliGate(const qc::Qubit controlQubitOne, const qc::Qubit controlQubitTwo, const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(controlQubitOne) || !isQubitWithinRange(controlQubitTwo) || !isQubitWithinRange(targetQubit) || controlQubitOne == targetQubit || controlQubitTwo == targetQubit || aggregateOfPropagatedControlQubits.contains(targetQubit)) {
        return false;
    }

    qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    gateControlQubits.emplace(controlQubitOne);
    gateControlQubits.emplace(controlQubitTwo);

    const std::size_t prevNumQuantumOperations = getNops();
    mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations - 1U, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingMultiControlToffoliGate(const qc::Controls& controlQubits, const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(targetQubit) || std::ranges::any_of(controlQubits, [&](const qc::Control& control) { return !isQubitWithinRange(control.qubit) || control.qubit == targetQubit; }) || aggregateOfPropagatedControlQubits.contains(targetQubit)) {
        return false;
    }

    qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    gateControlQubits.insert(controlQubits.cbegin(), controlQubits.cend());
    if (gateControlQubits.empty()) {
        return false;
    }

    const std::size_t prevNumQuantumOperations = getNops();
    mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations - 1U, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingFredkinGate(const qc::Qubit targetQubitOne, const qc::Qubit targetQubitTwo) {
    if (!isQubitWithinRange(targetQubitOne) || !isQubitWithinRange(targetQubitTwo) || targetQubitOne == targetQubitTwo || aggregateOfPropagatedControlQubits.contains(targetQubitOne) || aggregateOfPropagatedControlQubits.contains(targetQubitTwo)) {
        return false;
    }
    const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());

    const std::size_t prevNumQuantumOperations = getNops();
    mcswap(gateControlQubits, targetQubitOne, targetQubitTwo);

    const std::size_t currNumQuantumOperations = getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations - 1U, {});
}

// TODO: Tests
std::optional<qc::Qubit> AnnotatableQuantumComputation::addQuantumRegisterForSyrecVariable(const std::string& quantumRegisterLabel, const Variable& variable, const bool areGeneratedQubitsGarbage, const std::optional<InlinedQubitInformation>& optionalInliningInformation) {
    if (!canQubitsBeAddedToQuantumComputation || variable.bitwidth == 0 || std::ranges::all_of(variable.dimensions, [](const unsigned numberOfValuesOfDimension) { return numberOfValuesOfDimension == 0; }) || quantumRegisterLabel.empty() || getQuantumRegisters().contains(quantumRegisterLabel) || (optionalInliningInformation.has_value() && ((optionalInliningInformation->inlineStack.has_value() && isInlineStackNotSetOrEmpty(optionalInliningInformation->inlineStack.value())) || !optionalInliningInformation->userDeclaredQubitLabel.has_value() || optionalInliningInformation->userDeclaredQubitLabel->empty()))) {
        return std::nullopt;
    }

    const unsigned numberOfElementsInVariable    = std::accumulate(variable.dimensions.cbegin(), variable.dimensions.cend(), 1U, std::multiplies());
    const unsigned totalNumberOfQubitsOfVariable = numberOfElementsInVariable * variable.bitwidth;
    const auto     addedQuantumRegister          = addQubitRegister(totalNumberOfQubitsOfVariable, quantumRegisterLabel);
    if (areGeneratedQubitsGarbage) {
        setLogicalQubitsGarbage(addedQuantumRegister.getStartIndex(), addedQuantumRegister.getEndIndex());
    }

    const auto coveredQubitIndices = QubitIndexRange({.firstQubitIndex = addedQuantumRegister.getStartIndex(), .lastQubitIndex = addedQuantumRegister.getEndIndex()});
    quantumRegisterAssociatedVariableLayouts.emplace_back(std::make_unique<NonAncillaryQuantumRegisterVariableLayout>(coveredQubitIndices, quantumRegisterLabel, variable.dimensions, variable.bitwidth, optionalInliningInformation));
    return addedQuantumRegister.getStartIndex();
}

std::optional<qc::Qubit> AnnotatableQuantumComputation::addPreliminaryAncillaryRegisterOrAppendToAdjacentOne(const std::string& quantumRegisterLabel, const std::vector<bool>& initialStateOfAncillaryQubits, const InlinedQubitInformation& sharedInliningInformation) {
    if (!canQubitsBeAddedToQuantumComputation || quantumRegisterLabel.empty() || getQuantumRegisters().contains(quantumRegisterLabel) || initialStateOfAncillaryQubits.empty() || (sharedInliningInformation.inlineStack.has_value() && isInlineStackNotSetOrEmpty(sharedInliningInformation.inlineStack.value())) || sharedInliningInformation.userDeclaredQubitLabel.has_value()) {
        return std::nullopt;
    }

    qc::Qubit                          indexToFirstGeneratedAncillaryQubit = 0U;
    BaseQuantumRegisterVariableLayout* lastAddedQuantumRegister            = quantumRegisterAssociatedVariableLayouts.empty() ? nullptr : quantumRegisterAssociatedVariableLayouts.back().get();

    const auto& addedQuantumRegister    = addQubitRegister(initialStateOfAncillaryQubits.size(), quantumRegisterLabel);
    indexToFirstGeneratedAncillaryQubit = addedQuantumRegister.getStartIndex();

    if (auto* lastAddedQuantumRegisterAsAncillaryOne = dynamic_cast<AncillaryQuantumRegisterVariableLayout*>(lastAddedQuantumRegister); lastAddedQuantumRegisterAsAncillaryOne != nullptr) {
        const auto qubitRangeOfTemporaryQuantumRegister = QubitIndexRange(addedQuantumRegister.getStartIndex(), addedQuantumRegister.getEndIndex());
        const auto qubitRangeOfMergedQuantumRegisters   = QubitIndexRange(lastAddedQuantumRegisterAsAncillaryOne->storedQubitIndices.firstQubitIndex, addedQuantumRegister.getEndIndex());

        // We need to create a temporary quantum register so that the qubits are added to the quantum computation but can then delete this temporary register and merge the adjacent ancillary quantum register with the now
        // deleted one by updating the covered qubit range of the former. Additionally, we need to update the state of the now appended to quantum register in the annotatable quantum computation.
        if (!quantumRegisters.contains(lastAddedQuantumRegisterAsAncillaryOne->quantumRegisterLabel) || !lastAddedQuantumRegisterAsAncillaryOne->appendQubitRange(qubitRangeOfTemporaryQuantumRegister, sharedInliningInformation) || quantumRegisters.erase(quantumRegisterLabel) != 1U) {
            return std::nullopt;
        }
        const auto newAncillaryRegisterSize = (qubitRangeOfMergedQuantumRegisters.lastQubitIndex - qubitRangeOfMergedQuantumRegisters.firstQubitIndex) + 1U;
        // At this point we have deleted the temporary quantum register in the quantum register but also need to update the covered qubit range of the appended to adjacent quantum register in the quantum computation (the base class).
        // This 'work around' is required since no quantum register can be deleted or modified using the public functions of the base quantum computation interface.
        quantumRegisters.at(lastAddedQuantumRegisterAsAncillaryOne->quantumRegisterLabel) = qc::QuantumRegister(qubitRangeOfMergedQuantumRegisters.firstQubitIndex, newAncillaryRegisterSize, lastAddedQuantumRegisterAsAncillaryOne->quantumRegisterLabel);
    } else {
        quantumRegisterAssociatedVariableLayouts.emplace_back(std::make_unique<AncillaryQuantumRegisterVariableLayout>(QubitIndexRange(addedQuantumRegister.getStartIndex(), addedQuantumRegister.getEndIndex()), quantumRegisterLabel, sharedInliningInformation));
    }

    for (qc::Qubit ancillaryQubitOffsetInQuantumRegister = 0; ancillaryQubitOffsetInQuantumRegister < initialStateOfAncillaryQubits.size(); ++ancillaryQubitOffsetInQuantumRegister) {
        // Since ancillary qubits are assumed to have an initial value of
        // zero, we need to add an inversion gate to derive the correct
        // initial value of 1.
        // We can either use a simple X quantum operation to initialize the qubit with '1' but we should
        // probably also consider the active control qubits set in the currently active control qubit propagation scopes.
        if (!initialStateOfAncillaryQubits.at(ancillaryQubitOffsetInQuantumRegister)) {
            continue;
        }

        if (!addOperationsImplementingNotGate(indexToFirstGeneratedAncillaryQubit + ancillaryQubitOffsetInQuantumRegister)) {
            return std::nullopt;
        }
    }
    return indexToFirstGeneratedAncillaryQubit;
}

void AnnotatableQuantumComputation::promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits() {
    canQubitsBeAddedToQuantumComputation = false;

    for (auto quantumRegisterIterator = quantumRegisterAssociatedVariableLayouts.begin(); quantumRegisterIterator != quantumRegisterAssociatedVariableLayouts.end(); ++quantumRegisterIterator) {
        if (const auto* currQuantumRegisterAsAncillaryOne = dynamic_cast<const AncillaryQuantumRegisterVariableLayout*>(quantumRegisterIterator->get()); currQuantumRegisterAsAncillaryOne != nullptr) {
            setLogicalQubitsAncillary(currQuantumRegisterAsAncillaryOne->storedQubitIndices.firstQubitIndex, currQuantumRegisterAsAncillaryOne->storedQubitIndices.lastQubitIndex);
        }
    }
}

std::optional<std::string> AnnotatableQuantumComputation::getQubitLabel(const qc::Qubit qubit, const QubitLabelType qubitLabelType) const {
    const std::optional<std::size_t>                                                  indexOfQuantumRegisterStoringQubit  = determineIndexOfQuantumRegisterStoringQubit(qubit);
    const std::optional<BaseQuantumRegisterVariableLayout::QubitInVariableLayoutData> qubitInformationFromQuantumRegister = indexOfQuantumRegisterStoringQubit.has_value() ? quantumRegisterAssociatedVariableLayouts.at(*indexOfQuantumRegisterStoringQubit)->determineQubitInVariableLayoutData(qubit) : std::nullopt;
    if (!qubitInformationFromQuantumRegister.has_value()) {
        return std::nullopt;
    }

    std::string inheritedQubitIdentifierFromQuantumRegister;
    if (qubitLabelType == UserDeclared) {
        if (!qubitInformationFromQuantumRegister->inlinedQubitInformation.has_value() || !qubitInformationFromQuantumRegister->inlinedQubitInformation->userDeclaredQubitLabel.has_value()) {
            return std::nullopt;
        }
        inheritedQubitIdentifierFromQuantumRegister = *qubitInformationFromQuantumRegister->inlinedQubitInformation->userDeclaredQubitLabel;
    } else if (qubitLabelType == Internal) {
        inheritedQubitIdentifierFromQuantumRegister = qubitInformationFromQuantumRegister->quantumRegisterLabel;
    }
    return buildQubitLabelForQubitOfVariableInQuantumRegister(inheritedQubitIdentifierFromQuantumRegister, qubitInformationFromQuantumRegister->accessedValuePerDimensionOfElementStoringQubit, qubitInformationFromQuantumRegister->relativeQubitIndexInElementStoringQubit);
}

const qc::Operation* AnnotatableQuantumComputation::getQuantumOperation(const std::size_t indexOfQuantumOperationInQuantumComputation) const {
    if (indexOfQuantumOperationInQuantumComputation >= getNops()) {
        return nullptr;
    }
    return at(indexOfQuantumOperationInQuantumComputation).get();
}

bool AnnotatableQuantumComputation::replayOperationsAtGivenIndexRange(std::size_t indexOfFirstQuantumOperationToReplayInQuantumComputation, std::size_t indexOfLastQuantumOperationToReplayInQuantumComputation) {
    if (indexOfFirstQuantumOperationToReplayInQuantumComputation >= getNops() || indexOfLastQuantumOperationToReplayInQuantumComputation >= getNops()) {
        return false;
    }

    std::size_t idxOfFirstQuantumOperationToAnnotateAfterReplay = indexOfFirstQuantumOperationToReplayInQuantumComputation;
    std::size_t idxOfLastQuantumOperationToAnnotateAfterReplay  = indexOfLastQuantumOperationToReplayInQuantumComputation;
    std::size_t numQuantumOperationsToReplay                    = 0U;
    // Since we have already validated that the provided indices are within range and under the assumption that only valid quantum operations are stored in the quantum computation (i.e. no nullptrs)
    // then the result of the at(...) should return a valid quantum operation instance.
    // After the operations were replayed with the emplace_back(..) call of qc::QuantumComputation, the number of operations will be larger than the number of gate annotations since the annotations for the replayed operations are only
    // recorded in this derived class.
    if (indexOfFirstQuantumOperationToReplayInQuantumComputation > indexOfLastQuantumOperationToReplayInQuantumComputation) {
        numQuantumOperationsToReplay = (indexOfFirstQuantumOperationToReplayInQuantumComputation - indexOfLastQuantumOperationToReplayInQuantumComputation) + 1U;
        for (std::size_t quantumOperationIdxOffset = 0; quantumOperationIdxOffset < numQuantumOperationsToReplay; ++quantumOperationIdxOffset) {
            emplace_back(at(indexOfFirstQuantumOperationToReplayInQuantumComputation - quantumOperationIdxOffset)->clone());
        }
    } else {
        numQuantumOperationsToReplay = (indexOfLastQuantumOperationToReplayInQuantumComputation - indexOfFirstQuantumOperationToReplayInQuantumComputation) + 1U;
        for (std::size_t quantumOperationIdxOffset = 0; quantumOperationIdxOffset < numQuantumOperationsToReplay; ++quantumOperationIdxOffset) {
            emplace_back(at(indexOfFirstQuantumOperationToReplayInQuantumComputation + quantumOperationIdxOffset)->clone());
        }
    }

    idxOfFirstQuantumOperationToAnnotateAfterReplay += numQuantumOperationsToReplay;
    idxOfLastQuantumOperationToAnnotateAfterReplay += numQuantumOperationsToReplay;
    return annotateAllQuantumOperationsAtPositions(idxOfFirstQuantumOperationToAnnotateAfterReplay, idxOfLastQuantumOperationToAnnotateAfterReplay, {});
}

AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup AnnotatableQuantumComputation::getAnnotationsOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const {
    if (indexOfQuantumOperationInQuantumComputation >= annotationsPerQuantumOperation.size()) {
        return {};
    }
    return annotationsPerQuantumOperation[indexOfQuantumOperationInQuantumComputation];
}

AnnotatableQuantumComputation::SynthesisCostMetricValue AnnotatableQuantumComputation::getQuantumCostForSynthesis() const {
    SynthesisCostMetricValue cost = 0;

    const auto numQubits = getNqubits();
    if (numQubits == 0) {
        return cost;
    }

    for (const auto& quantumOperation: ops) {
        const std::size_t c             = std::min(quantumOperation->getNcontrols() + static_cast<std::size_t>(quantumOperation->getType() == qc::OpType::SWAP), numQubits - 1);
        const std::size_t numEmptyLines = numQubits - c - 1U;

        switch (c) {
            case 0U:
            case 1U:
                cost += 1ULL;
                break;
            case 2U:
                cost += 5ULL;
                break;
            case 3U:
                cost += 13ULL;
                break;
            case 4U:
                cost += (numEmptyLines >= 2U) ? 26ULL : 29ULL;
                break;
            case 5U:
                if (numEmptyLines >= 3U) {
                    cost += 38ULL;
                } else if (numEmptyLines >= 1U) {
                    cost += 52ULL;
                } else {
                    cost += 61ULL;
                }
                break;
            case 6U:
                if (numEmptyLines >= 4U) {
                    cost += 50ULL;
                } else if (numEmptyLines >= 1U) {
                    cost += 80ULL;
                } else {
                    cost += 125ULL;
                }
                break;
            case 7U:
                if (numEmptyLines >= 5U) {
                    cost += 62ULL;
                } else if (numEmptyLines >= 1U) {
                    cost += 100ULL;
                } else {
                    cost += 253ULL;
                }
                break;
            default:
                if (numEmptyLines >= c - 2U) {
                    cost += 12ULL * c - 22ULL;
                } else if (numEmptyLines >= 1U) {
                    cost += 24ULL * c - 87ULL;
                } else {
                    cost += (1ULL << (c + 1ULL)) - 3ULL;
                }
        }
    }
    return cost;
}

AnnotatableQuantumComputation::SynthesisCostMetricValue AnnotatableQuantumComputation::getTransistorCostForSynthesis() const {
    SynthesisCostMetricValue cost = 0;
    for (const auto& quantumOperation: ops) {
        cost += quantumOperation->getNcontrols() * 8;
    }
    return cost;
}

void AnnotatableQuantumComputation::activateControlQubitPropagationScope() {
    controlQubitPropgationScopes.emplace_back();
}

void AnnotatableQuantumComputation::deactivateControlQubitPropagationScope() {
    if (controlQubitPropgationScopes.empty()) {
        return;
    }

    const auto& localControlLineScope = controlQubitPropgationScopes.back();
    for (const auto [controlLine, wasControlLineActiveInParentScope]: localControlLineScope) {
        if (wasControlLineActiveInParentScope) {
            // Control lines registered prior to the local scope and deactivated by the latter should still be registered in the parent
            // scope after the local one was deactivated.
            aggregateOfPropagatedControlQubits.emplace(controlLine);
        } else {
            aggregateOfPropagatedControlQubits.erase(controlLine);
        }
    }
    controlQubitPropgationScopes.pop_back();
}

bool AnnotatableQuantumComputation::deregisterControlQubitFromPropagationInCurrentScope(const qc::Qubit controlQubit) {
    if (controlQubitPropgationScopes.empty() || !isQubitWithinRange(controlQubit)) {
        return false;
    }

    auto& localControlLineScope = controlQubitPropgationScopes.back();
    if (!localControlLineScope.contains(controlQubit)) {
        return false;
    }

    aggregateOfPropagatedControlQubits.erase(controlQubit);
    return true;
}

bool AnnotatableQuantumComputation::registerControlQubitForPropagationInCurrentAndNestedScopes(const qc::Qubit controlQubit) {
    if (!isQubitWithinRange(controlQubit)) {
        return false;
    }

    if (controlQubitPropgationScopes.empty()) {
        activateControlQubitPropagationScope();
    }

    auto& localControlLineScope = controlQubitPropgationScopes.back();
    // If an entry for the to be registered control line already exists in the current scope then the previously determine value of the flag indicating whether the control line existed in the parent scope
    // should have the same value that it had when the control line was initially added to the current scope

    if (!localControlLineScope.contains(controlQubit)) {
        localControlLineScope.emplace(std::make_pair(controlQubit, aggregateOfPropagatedControlQubits.contains(controlQubit)));
    }
    aggregateOfPropagatedControlQubits.emplace(controlQubit);
    return true;
}

bool AnnotatableQuantumComputation::setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value) {
    auto existingAnnotationForKey = activateGlobalQuantumOperationAnnotations.find(key);
    if (existingAnnotationForKey != activateGlobalQuantumOperationAnnotations.end()) {
        existingAnnotationForKey->second = value;
        return true;
    }
    activateGlobalQuantumOperationAnnotations.emplace(static_cast<std::string>(key), value);
    return false;
}

bool AnnotatableQuantumComputation::removeGlobalQuantumOperationAnnotation(const std::string_view& key) {
    // We utilize the ability to use a std::string_view to erase a matching element
    // of std::string in a std::map<std::string, ...> without needing to cast the
    // std::string_view to std::string for the std::map<>::erase() operation
    // (see further: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p2077r3.html)
    auto existingAnnotationForKey = activateGlobalQuantumOperationAnnotations.find(key);
    if (existingAnnotationForKey != activateGlobalQuantumOperationAnnotations.end()) {
        activateGlobalQuantumOperationAnnotations.erase(existingAnnotationForKey);
        return true;
    }
    return false;
}

bool AnnotatableQuantumComputation::setOrUpdateAnnotationOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation, const std::string_view& annotationKey, const std::string& annotationValue) {
    if (indexOfQuantumOperationInQuantumComputation >= annotationsPerQuantumOperation.size()) {
        return false;
    }

    auto& annotationsForQuantumOperation = annotationsPerQuantumOperation[indexOfQuantumOperationInQuantumComputation];
    if (auto matchingEntryForKey = annotationsForQuantumOperation.find(annotationKey); matchingEntryForKey != annotationsForQuantumOperation.end()) {
        matchingEntryForKey->second = annotationValue;
    } else {
        annotationsForQuantumOperation.emplace(std::string(annotationKey), annotationValue);
    }
    return true;
}

std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> AnnotatableQuantumComputation::getInlinedQubitInformation(const qc::Qubit qubit) const {
    const std::optional<std::size_t>                                                  indexOfQuantumRegisterContainingQubit           = determineIndexOfQuantumRegisterStoringQubit(qubit);
    const std::optional<BaseQuantumRegisterVariableLayout::QubitInVariableLayoutData> associatedVariableForQubitDataInQuantumRegister = indexOfQuantumRegisterContainingQubit.has_value() ? quantumRegisterAssociatedVariableLayouts.at(*indexOfQuantumRegisterContainingQubit)->determineQubitInVariableLayoutData(qubit) : std::nullopt;
    if (!associatedVariableForQubitDataInQuantumRegister.has_value() || !associatedVariableForQubitDataInQuantumRegister->inlinedQubitInformation.has_value()) {
        return std::nullopt;
    }

    InlinedQubitInformation qubitInlineInformation = associatedVariableForQubitDataInQuantumRegister->inlinedQubitInformation.value();
    // The user declared qubit label stored in the qubit inline information of the quantum register only stores the identifier of the associated SyReC variable but no information about where in the variable the qubit is stored.
    qubitInlineInformation.userDeclaredQubitLabel = getQubitLabel(qubit, UserDeclared);
    return qubitInlineInformation;
}

// BEGIN NON-PUBLIC FUNCTIONALITY
bool AnnotatableQuantumComputation::isQubitWithinRange(const qc::Qubit qubit) const noexcept {
    return qubit < getNqubits();
}

bool AnnotatableQuantumComputation::annotateAllQuantumOperationsAtPositions(const std::size_t fromQuantumOperationIndex, const std::size_t toQuantumOperationIndex, const QuantumOperationAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation) {
    if (fromQuantumOperationIndex >= getNops() || toQuantumOperationIndex >= getNops()) {
        return false;
    }

    std::size_t idxOfFirstGateToAnnotate = 0U;
    std::size_t idxOfLastGateToAnnotate  = 0U;
    if (fromQuantumOperationIndex <= toQuantumOperationIndex) {
        if (toQuantumOperationIndex >= annotationsPerQuantumOperation.size()) {
            annotationsPerQuantumOperation.resize(toQuantumOperationIndex + 1U);
        }
        idxOfFirstGateToAnnotate = fromQuantumOperationIndex;
        idxOfLastGateToAnnotate  = toQuantumOperationIndex;
    } else {
        if (fromQuantumOperationIndex >= annotationsPerQuantumOperation.size()) {
            annotationsPerQuantumOperation.resize(fromQuantumOperationIndex + 1U);
        }
        idxOfFirstGateToAnnotate = toQuantumOperationIndex;
        idxOfLastGateToAnnotate  = fromQuantumOperationIndex;
    }

    QuantumOperationAnnotationsLookup gateAnnotations = userProvidedAnnotationsPerQuantumOperation;
    for (const auto& [annotationKey, annotationValue]: activateGlobalQuantumOperationAnnotations) {
        gateAnnotations[annotationKey] = annotationValue;
    }

    for (std::size_t i = idxOfFirstGateToAnnotate; i <= idxOfLastGateToAnnotate; ++i) {
        annotationsPerQuantumOperation[i].insert(userProvidedAnnotationsPerQuantumOperation.cbegin(), userProvidedAnnotationsPerQuantumOperation.cend());
        annotationsPerQuantumOperation[i].insert(activateGlobalQuantumOperationAnnotations.cbegin(), activateGlobalQuantumOperationAnnotations.cend());
    }
    return true;
}

// BEGIN Quantum register variable alyout functionality
AnnotatableQuantumComputation::NonAncillaryQuantumRegisterVariableLayout::NonAncillaryQuantumRegisterVariableLayout(const QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const std::vector<unsigned>& numValuesPerDimensionOfVariable, const unsigned qubitSizeOfElementInVariable, const std::optional<InlinedQubitInformation>& optionalSharedInlinedQubitInformation):
    BaseQuantumRegisterVariableLayout(coveredQubitIndicesOfQuantumRegister, quantumRegisterLabel), elementQubitSize(qubitSizeOfElementInVariable), numValuesPerDimensionOfVariable(numValuesPerDimensionOfVariable), optionalSharedInlinedQubitInformation(optionalSharedInlinedQubitInformation) {
    offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths = std::vector(numValuesPerDimensionOfVariable.size(), 1U);
    std::size_t dimensionIndex                                        = numValuesPerDimensionOfVariable.size() - 1U;
    for (auto offsetIterator = std::next(offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.rbegin()); offsetIterator != offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.rend(); ++offsetIterator) {
        *offsetIterator = *std::prev(offsetIterator) * numValuesPerDimensionOfVariable.at(dimensionIndex--);
    }
}

std::optional<AnnotatableQuantumComputation::BaseQuantumRegisterVariableLayout::QubitInVariableLayoutData> AnnotatableQuantumComputation::NonAncillaryQuantumRegisterVariableLayout::determineQubitInVariableLayoutData(const qc::Qubit qubit) const {
    const std::optional<std::vector<unsigned>> requiredValuePerDimensionToAccessElementStoringQubit = storedQubitIndices.firstQubitIndex <= qubit && storedQubitIndices.lastQubitIndex >= qubit ? getRequiredValuesPerDimensionToAccessQubitOfVariable(qubit) : std::nullopt;
    if (!requiredValuePerDimensionToAccessElementStoringQubit.has_value()) {
        return std::nullopt;
    }

    qc::Qubit firstQubitOfAccessedElement = storedQubitIndices.firstQubitIndex;
    for (std::size_t i = 0; i < numValuesPerDimensionOfVariable.size(); ++i) {
        firstQubitOfAccessedElement += requiredValuePerDimensionToAccessElementStoringQubit->at(i) * offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.at(i) * elementQubitSize;
    }

    const qc::Qubit relativeQubitIndexInQuantumRegister = qubit - firstQubitOfAccessedElement;
    return QubitInVariableLayoutData({.quantumRegisterLabel                           = quantumRegisterLabel,
                                      .accessedValuePerDimensionOfElementStoringQubit = *requiredValuePerDimensionToAccessElementStoringQubit,
                                      .relativeQubitIndexInElementStoringQubit        = relativeQubitIndexInQuantumRegister,
                                      .inlinedQubitInformation                        = optionalSharedInlinedQubitInformation});
}

[[nodiscard]] std::optional<std::vector<unsigned>> AnnotatableQuantumComputation::NonAncillaryQuantumRegisterVariableLayout::getRequiredValuesPerDimensionToAccessQubitOfVariable(const qc::Qubit qubit) const {
    if (offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.empty() || numValuesPerDimensionOfVariable.empty() || std::ranges::any_of(numValuesPerDimensionOfVariable, [](const unsigned numValuesOfDimension) { return numValuesOfDimension == 0; }) || elementQubitSize == 0 || storedQubitIndices.firstQubitIndex > qubit) {
        return std::nullopt;
    }

    // I. Calculate offset to next element in dimension
    // II. For each dimension perform a binary search to determine which element contains the qubit
    // III. Use indices to build accessed values per dimension for qubit.
    bool            couldRequiredValuePerDimensionBeDetermined = true;
    auto            requiredValuesPerDimension                 = std::vector(numValuesPerDimensionOfVariable.size(), 0U);
    const qc::Qubit qubitSizeOfElements                        = elementQubitSize;

    for (std::size_t i = 0; i < requiredValuesPerDimension.size() && couldRequiredValuePerDimensionBeDetermined; ++i) {
        const unsigned qubitOffsetToNextElementInDimension = offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths[i] * qubitSizeOfElements;

        auto firstQubitIndexPerElementInDimension = std::vector(numValuesPerDimensionOfVariable.at(i), qubitOffsetToNextElementInDimension);
        firstQubitIndexPerElementInDimension[0]   = storedQubitIndices.firstQubitIndex;
        for (std::size_t j = 0; j < i; ++j) {
            firstQubitIndexPerElementInDimension[0] += (requiredValuesPerDimension[j] * offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths[j]) * qubitSizeOfElements;
        }

        for (std::size_t j = 1; j < firstQubitIndexPerElementInDimension.size(); ++j) {
            firstQubitIndexPerElementInDimension[j] += firstQubitIndexPerElementInDimension[j - 1];
        }

        const auto qubitIndexRangePerElementOfCollection = firstQubitIndexPerElementInDimension | std::views::transform([qubitOffsetToNextElementInDimension](const qc::Qubit firstQubitOfElement) { return QubitIndexRange(firstQubitOfElement, (firstQubitOfElement + qubitOffsetToNextElementInDimension) - 1U); });
        const auto indexOfElementContainingQubit         = static_cast<std::optional<unsigned>>(findIndexOfElementContainingQubit(qubitIndexRangePerElementOfCollection.begin(), qubitIndexRangePerElementOfCollection.end(), qubit));

        couldRequiredValuePerDimensionBeDetermined = indexOfElementContainingQubit.has_value();
        requiredValuesPerDimension[i]              = indexOfElementContainingQubit.value_or(0);
    }
    return couldRequiredValuePerDimensionBeDetermined ? std::make_optional(requiredValuesPerDimension) : std::nullopt;
}

AnnotatableQuantumComputation::AncillaryQuantumRegisterVariableLayout::AncillaryQuantumRegisterVariableLayout(const QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const InlinedQubitInformation& sharedInlinedQubitInformation):
    BaseQuantumRegisterVariableLayout(coveredQubitIndicesOfQuantumRegister, quantumRegisterLabel) {
    storedQubitIndices         = coveredQubitIndicesOfQuantumRegister;
    this->quantumRegisterLabel = quantumRegisterLabel;
    sharedQubitRangeInlineInformationLookup.emplace_back(storedQubitIndices, sharedInlinedQubitInformation);
}

std::optional<AnnotatableQuantumComputation::BaseQuantumRegisterVariableLayout::QubitInVariableLayoutData> AnnotatableQuantumComputation::AncillaryQuantumRegisterVariableLayout::determineQubitInVariableLayoutData(const qc::Qubit qubit) const {
    if (storedQubitIndices.firstQubitIndex > qubit || storedQubitIndices.lastQubitIndex < qubit) {
        return std::nullopt;
    }

    const qc::Qubit relativeQubitIndexInQuantumRegister = qubit - storedQubitIndices.firstQubitIndex;

    const auto&                      nonOverlappingQubitIndexRanges = sharedQubitRangeInlineInformationLookup | std::views::transform([](const SharedQubitRangeInlineInformation& sharedQubitRangeInlineInformation) { return sharedQubitRangeInlineInformation.coveredQubitIndexRange; });
    const std::optional<std::size_t> indexOfQubitRangeStoringQubit  = findIndexOfElementContainingQubit(nonOverlappingQubitIndexRanges.begin(), nonOverlappingQubitIndexRanges.end(), qubit);
    if (!indexOfQubitRangeStoringQubit.has_value()) {
        return std::nullopt;
    }

    return QubitInVariableLayoutData({.quantumRegisterLabel                           = quantumRegisterLabel,
                                      .accessedValuePerDimensionOfElementStoringQubit = std::vector({0U}),
                                      .relativeQubitIndexInElementStoringQubit        = relativeQubitIndexInQuantumRegister,
                                      .inlinedQubitInformation                        = sharedQubitRangeInlineInformationLookup.at(*indexOfQubitRangeStoringQubit).inlinedQubitInformation});
}

bool AnnotatableQuantumComputation::AncillaryQuantumRegisterVariableLayout::appendQubitRange(const QubitIndexRange qubitIndexRange, const InlinedQubitInformation& sharedInlinedQubitInformation) {
    if (sharedQubitRangeInlineInformationLookup.empty() || qubitIndexRange.firstQubitIndex != storedQubitIndices.lastQubitIndex + 1 || qubitIndexRange.firstQubitIndex > qubitIndexRange.lastQubitIndex) {
        return false;
    }

    sharedQubitRangeInlineInformationLookup.emplace_back(qubitIndexRange, sharedInlinedQubitInformation);
    const qc::Qubit qubitIndexRangeLength = (qubitIndexRange.lastQubitIndex - qubitIndexRange.firstQubitIndex) + 1U;
    storedQubitIndices.lastQubitIndex += qubitIndexRangeLength;
    return true;
}
// END Quantum register variable layout functionality

std::string AnnotatableQuantumComputation::buildQubitLabelForQubitOfVariableInQuantumRegister(const std::string& quantumRegisterLabel, const std::vector<unsigned>& accessedValuePerDimension, const std::size_t relativeQubitIndexInElement) {
    std::string generatedQubitLabel = quantumRegisterLabel;
    for (const auto accessedValueOfDimension: accessedValuePerDimension) {
        generatedQubitLabel += "[" + std::to_string(accessedValueOfDimension) + "]";
    }
    generatedQubitLabel += "." + std::to_string(relativeQubitIndexInElement);
    return generatedQubitLabel;
}

std::optional<std::size_t> AnnotatableQuantumComputation::determineIndexOfQuantumRegisterStoringQubit(const qc::Qubit qubit) const {
    const auto qubitRangeOfQuantumRegisters = quantumRegisterAssociatedVariableLayouts | std::views::transform([](const std::unique_ptr<BaseQuantumRegisterVariableLayout>& quantumRegisterVariableLayout) { return quantumRegisterVariableLayout->storedQubitIndices; });
    return findIndexOfElementContainingQubit(qubitRangeOfQuantumRegisters.begin(), qubitRangeOfQuantumRegisters.end(), qubit);
}
// END NON-PUBLIC FUNCTIONALITY
