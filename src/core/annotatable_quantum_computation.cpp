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

// TODO: We will need to store the dimensions and bitwidth of the variable to be able to generate qubit labels of the form a[0][1].0 for all qubits of the quantum registers.
// TODO: Tests
std::optional<qc::Qubit> AnnotatableQuantumComputation::addQuantumRegisterForSyrecVariable(const std::string& quantumRegisterLabel, const Variable& variable, bool areGeneratedQubitsGarbage, const std::optional<InlinedQubitInformation>& optionalInliningInformation) {
    if (!canQubitsBeAddedToQuantumComputation || variable.bitwidth == 0 || std::ranges::all_of(variable.dimensions, [](const unsigned numberOfValuesOfDimension) { return numberOfValuesOfDimension == 0; }) || quantumRegisterLabel.empty() || getQuantumRegisters().contains(quantumRegisterLabel) || (optionalInliningInformation.has_value() && ((optionalInliningInformation->inlineStack.has_value() && isInlineStackNotSetOrEmpty(optionalInliningInformation->inlineStack.value())) || !optionalInliningInformation->userDeclaredQubitLabel.has_value() || optionalInliningInformation->userDeclaredQubitLabel->empty()))) {
        return std::nullopt;
    }

    const unsigned numberOfElementsInVariable         = std::accumulate(variable.dimensions.cbegin(), variable.dimensions.cend(), 1U, std::multiplies());
    const unsigned totalNumberOfQubitsOfVariable      = numberOfElementsInVariable * variable.bitwidth;
    const auto     indexToFirstQubitOfQuantumRegister = addQubitRegister(totalNumberOfQubitsOfVariable, quantumRegisterLabel);
    if (areGeneratedQubitsGarbage) {
        setLogicalQubitsGarbage(indexToFirstQubitOfQuantumRegister.getStartIndex(), indexToFirstQubitOfQuantumRegister.getEndIndex());
    }

    const auto coveredQubitIndices = QubitIndexRange({.firstQubitIndex = indexToFirstQubitOfQuantumRegister.getStartIndex(), .lastQubitIndex = indexToFirstQubitOfQuantumRegister.getEndIndex()});
    quantumRegisterAssociatedVariableLayouts.emplace_back(std::make_unique<NonAncillaryQuantumRegisterVariableLayout>(coveredQubitIndices, quantumRegisterLabel, variable.dimensions, variable.bitwidth, optionalInliningInformation));
    return indexToFirstQubitOfQuantumRegister.getStartIndex();
}

std::optional<qc::Qubit> AnnotatableQuantumComputation::addPreliminaryAncillaryRegister(const std::string& quantumRegisterLabel, const std::vector<bool>& initialStateOfAncillaryQubits, const InlinedQubitInformation& sharedInliningInformation) {
    if (!canQubitsBeAddedToQuantumComputation || quantumRegisterLabel.empty() || getQuantumRegisters().contains(quantumRegisterLabel) || initialStateOfAncillaryQubits.empty() || (sharedInliningInformation.inlineStack.has_value() && isInlineStackNotSetOrEmpty(sharedInliningInformation.inlineStack.value())) || sharedInliningInformation.userDeclaredQubitLabel.has_value()) {
        return std::nullopt;
    }

    const auto indexToFirstQubitOfQuantumRegister = addQubitRegister(initialStateOfAncillaryQubits.size(), quantumRegisterLabel).getStartIndex();
    for (qc::Qubit ancillaryQubitOffsetInQuantumRegister = 0; ancillaryQubitOffsetInQuantumRegister < initialStateOfAncillaryQubits.size(); ++ancillaryQubitOffsetInQuantumRegister) {
        // Since ancillary qubits are assumed to have an initial value of
        // zero, we need to add an inversion gate to derive the correct
        // initial value of 1.
        // We can either use a simple X quantum operation to initialize the qubit with '1' but we should
        // probably also consider the active control qubits set in the currently active control qubit propagation scopes.
        if (!initialStateOfAncillaryQubits.at(ancillaryQubitOffsetInQuantumRegister)) {
            continue;
        }

        if (!addOperationsImplementingNotGate(indexToFirstQubitOfQuantumRegister + ancillaryQubitOffsetInQuantumRegister)) {
            return std::nullopt;
        }
    }
    return indexToFirstQubitOfQuantumRegister;
}

// TODO: If we are generating quantum registers for all SyReC module parameters then this function is obsolete. Currently only left due to existing tests which will need to be refactored.
std::optional<qc::Qubit> AnnotatableQuantumComputation::addNonAncillaryQubit(const std::string& qubitLabel, bool isGarbageQubit, const std::optional<InlinedQubitInformation>& optionalInliningInformation) {
    // if (!canQubitsBeAddedToQuantumComputation || qubitLabel.empty() || getQuantumRegisters().contains(qubitLabel) || inlinedQubitsInformationLookup.contains(qubitLabel) || (optionalInliningInformation.has_value() && ((optionalInliningInformation->inlineStack.has_value() && isInlineStackNotSetOrEmpty(optionalInliningInformation->inlineStack.value())) || !optionalInliningInformation->userDeclaredQubitLabel.has_value() || optionalInliningInformation->userDeclaredQubitLabel->empty()))) {
    //     return std::nullopt;
    // }
    //
    // constexpr std::size_t qubitSize  = 1;
    // const auto            qubitIndex = addQubitRegister(qubitSize, qubitLabel).getStartIndex();
    // if (isGarbageQubit) {
    //     setLogicalQubitGarbage(qubitIndex);
    // }
    //
    // if (optionalInliningInformation.has_value()) {
    //     inlinedQubitsInformationLookup[qubitLabel] = *optionalInliningInformation;
    // }
    // return qubitIndex;
    return std::nullopt;
}

// TODO: If possible aggregate all ancillary qubits into two quantum registers that store the ancillary qubits initialized to 0 and 1 respectively. Whether a reordering/resize/modification of quantum registers in the quantum computation is possible needs to be determined.
std::optional<qc::Qubit> AnnotatableQuantumComputation::addPreliminaryAncillaryQubit(const std::string& qubitLabel, bool initialStateOfQubit, const InlinedQubitInformation& inliningInformation) {
    // if (!canQubitsBeAddedToQuantumComputation || qubitLabel.empty() || getQuantumRegisters().contains(qubitLabel) || inlinedQubitsInformationLookup.contains(qubitLabel) || inliningInformation.userDeclaredQubitLabel.has_value() || (inliningInformation.inlineStack.has_value() && isInlineStackNotSetOrEmpty(inliningInformation.inlineStack.value()))) {
    //     return std::nullopt;
    // }
    //
    // constexpr std::size_t qubitSize  = 1;
    // const auto            qubitIndex = addQubitRegister(qubitSize, qubitLabel).getStartIndex();
    // addedAncillaryQubitIndices.emplace(qubitIndex);
    // inlinedQubitsInformationLookup[qubitLabel] = inliningInformation;
    //
    // if (initialStateOfQubit) {
    //     // Since ancillary qubits are assumed to have an initial value of
    //     // zero, we need to add an inversion gate to derive the correct
    //     // initial value of 1.
    //     // We can either use a simple X quantum operation to initialize the qubit with '1' but we should
    //     // probably also consider the active control qubits set in the currently active control qubit propagation scopes.
    //     if (!addOperationsImplementingNotGate(qubitIndex)) {
    //         return std::nullopt;
    //     }
    // }
    // return qubitIndex;
    return std::nullopt;
}

bool AnnotatableQuantumComputation::promotePreliminaryAncillaryQubitsToDefinitiveAncillaryRegistersAndMergeAdjacentOnes() {
    canQubitsBeAddedToQuantumComputation = false;

    bool modificationsSuccessful = true;
    for (auto quantumRegisterIterator = quantumRegisterAssociatedVariableLayouts.begin(); quantumRegisterIterator != quantumRegisterAssociatedVariableLayouts.end() && modificationsSuccessful;) {
        auto* currQuantumRegisterAsAncillaryOne = dynamic_cast<AncillaryQuantumRegisterVariableLayout*>(quantumRegisterIterator->get());
        if (currQuantumRegisterAsAncillaryOne == nullptr) {
            ++quantumRegisterIterator;
            continue;
        }

        for (auto nextQuantumRegisterIterator = std::next(quantumRegisterIterator); nextQuantumRegisterIterator != quantumRegisterAssociatedVariableLayouts.end() && modificationsSuccessful;) {
            const auto* nextQuantumRegisterAsAncillaryOne = dynamic_cast<const AncillaryQuantumRegisterVariableLayout*>(nextQuantumRegisterIterator->get());
            if (nextQuantumRegisterAsAncillaryOne == nullptr) {
                break;
            }
            modificationsSuccessful     = currQuantumRegisterAsAncillaryOne->mergeWithOtherAncillaryQubitRegister(*nextQuantumRegisterAsAncillaryOne);
            nextQuantumRegisterIterator = quantumRegisterAssociatedVariableLayouts.erase(nextQuantumRegisterIterator);
        }

        const auto [mergedFirstAncillaryQubitIndexOfMergedRegister, lastAncillaryQubitIndexOfMergedRegister] = QubitIndexRange({.firstQubitIndex = currQuantumRegisterAsAncillaryOne->storedQubitIndices.firstQubitIndex, .lastQubitIndex = currQuantumRegisterAsAncillaryOne->storedQubitIndices.lastQubitIndex});
        modificationsSuccessful &= isQubitWithinRange(mergedFirstAncillaryQubitIndexOfMergedRegister) && isQubitWithinRange(lastAncillaryQubitIndexOfMergedRegister);
        if (modificationsSuccessful) {
            setLogicalQubitsAncillary(mergedFirstAncillaryQubitIndexOfMergedRegister, lastAncillaryQubitIndexOfMergedRegister);
        }
        ++quantumRegisterIterator;
    }
    return modificationsSuccessful;
}

bool AnnotatableQuantumComputation::promotePreliminaryAncillaryQubitToDefinitiveAncillary(qc::Qubit qubit) {
    if (!isQubitWithinRange(qubit)) {
        return false;
    }

    canQubitsBeAddedToQuantumComputation = false;
    setLogicalQubitAncillary(qubit);
    return true;
}

// TODO: quantumRegister is std::unordered_map so iterating over entries could return different order than using the qubit indices
// TODO: This function should build the qubit label of the form a[0][1].0 since we are using the variable identifier as the label of the quantum register
// TODO: Tests
std::optional<std::vector<std::string>> AnnotatableQuantumComputation::getQubitLabels() const {
    // std::size_t totalNumberOfQubitLabelsToGenerate = 0U;
    // for (const auto& quantumRegisterVariableLayout: quantumRegisterAssociatedVariableLayouts) {
    //     totalNumberOfQubitLabelsToGenerate += quantumRegisterVariableLayout->getNumberOfQubitsInQuantumRegister();
    // }
    //
    // auto        generatedQubitLabels      = std::vector<std::string>(totalNumberOfQubitLabelsToGenerate, "");
    // bool        generationOfQubitLabelsOk = true;
    // std::size_t insertionIndex            = 0U;
    //
    // for (std::size_t i = 0; i < quantumRegisterAssociatedVariableLayouts.size() && generationOfQubitLabelsOk; ++i) {
    //     const auto& currProcessedQuantumRegister = quantumRegisterAssociatedVariableLayouts.at(i);
    //     generationOfQubitLabelsOk                = currProcessedQuantumRegister->generateAndAddLabelsForAllStoredQubitsToContainer(generatedQubitLabels, insertionIndex);
    //     insertionIndex += currProcessedQuantumRegister->getNumberOfQubitsInQuantumRegister();
    // }
    // return generationOfQubitLabelsOk ? std::make_optional(generatedQubitLabels) : std::nullopt;
    return std::nullopt;
}

std::optional<std::string> AnnotatableQuantumComputation::getQubitLabel(const qc::Qubit qubit, const QubitLabelType qubitLabelType) const {
    const std::optional<std::size_t>                                                            indexOfQuantumRegisterStoringQubit  = determineIndexOfQuantumRegisterStoringQubit(qubit);
    const std::optional<BaseQuantumRegisterVariableLayout::QuantumRegisterQubitIndexLookupData> qubitInformationFromQuantumRegister = indexOfQuantumRegisterStoringQubit.has_value() ? quantumRegisterAssociatedVariableLayouts.at(*indexOfQuantumRegisterStoringQubit)->determineLookupDataForQubitFromQuantumRegister(qubit) : std::nullopt;
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

qc::Operation* AnnotatableQuantumComputation::getQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const {
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

const AnnotatableQuantumComputation::InlinedQubitInformation* AnnotatableQuantumComputation::getInliningInformationOfQubit(const std::string& qubitLabel) const {
    // if (!inlinedQubitsInformationLookup.contains(qubitLabel)) {
    //     return nullptr;
    // }
    // return &inlinedQubitsInformationLookup.at(qubitLabel);
    // TODO:
    return nullptr;
}

// BEGIN NON-PUBLIC FUNCTIONALITY
bool AnnotatableQuantumComputation::isQubitWithinRange(const qc::Qubit qubit) const noexcept {
    return qubit < getNqubits();
}

bool AnnotatableQuantumComputation::annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const QuantumOperationAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation) {
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
        annotationsPerQuantumOperation[i] = gateAnnotations;
    }
    return true;
}

// BEGIN Quantum register variable alyout functionality
AnnotatableQuantumComputation::NonAncillaryQuantumRegisterVariableLayout::NonAncillaryQuantumRegisterVariableLayout(const QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const std::vector<unsigned>& numValuesPerDimensionOfVariable, const unsigned qubitSizeOfElementInVariable, const std::optional<InlinedQubitInformation>& optionalSharedQubitInliningInformation):
    BaseQuantumRegisterVariableLayout(coveredQubitIndicesOfQuantumRegister, quantumRegisterLabel), elementQubitSize(qubitSizeOfElementInVariable), numValuesPerDimensionOfVariable(numValuesPerDimensionOfVariable), optionalSharedQubitInliningInformation(optionalSharedQubitInliningInformation) {
    offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths = std::vector(numValuesPerDimensionOfVariable.size(), 1U);
    std::size_t dimensionIndex                                        = numValuesPerDimensionOfVariable.size() - 1U;
    for (auto offsetIterator = std::next(offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.rbegin()); offsetIterator != offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.rend(); ++offsetIterator) {
        *offsetIterator = *std::prev(offsetIterator) * numValuesPerDimensionOfVariable.at(dimensionIndex--);
    }
}

std::optional<AnnotatableQuantumComputation::BaseQuantumRegisterVariableLayout::QuantumRegisterQubitIndexLookupData> AnnotatableQuantumComputation::NonAncillaryQuantumRegisterVariableLayout::determineLookupDataForQubitFromQuantumRegister(qc::Qubit qubit) const {
    const std::optional<std::vector<unsigned>> requiredValuePerDimensionToAccessElementStoringQubit = storedQubitIndices.firstQubitIndex >= qubit && storedQubitIndices.lastQubitIndex <= qubit ? getRequiredValuesPerDimensionToAccessQubitOfVariable(qubit) : std::nullopt;
    if (!requiredValuePerDimensionToAccessElementStoringQubit.has_value()) {
        return std::nullopt;
    }

    const qc::Qubit relativeQubitIndexInQuantumRegister = qubit - storedQubitIndices.firstQubitIndex;
    return QuantumRegisterQubitIndexLookupData({.quantumRegisterLabel                           = quantumRegisterLabel,
                                                .accessedValuePerDimensionOfElementStoringQubit = *requiredValuePerDimensionToAccessElementStoringQubit,
                                                .relativeQubitIndexInElementStoringQubit        = relativeQubitIndexInQuantumRegister,
                                                .inlinedQubitInformation                        = optionalSharedQubitInliningInformation});
}

[[nodiscard]] std::optional<std::vector<unsigned>> AnnotatableQuantumComputation::NonAncillaryQuantumRegisterVariableLayout::getRequiredValuesPerDimensionToAccessQubitOfVariable(const qc::Qubit qubit) const {
    if (offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths.empty() || numValuesPerDimensionOfVariable.empty() || elementQubitSize == 0 || storedQubitIndices.firstQubitIndex > qubit) {
        return std::nullopt;
    }

    // I. Calculate offset to next element in dimension
    // II. For each dimension perform a binary search to determine which element contains the qubit
    // III. Use indices to build accessed values per dimension for qubit.
    bool couldRequiredValuePerDimensionBeDetermined = true;
    auto requiredValuesPerDimension                 = std::vector(numValuesPerDimensionOfVariable.size(), 0U);
    for (std::size_t i = 0; i < requiredValuesPerDimension.size() && couldRequiredValuePerDimensionBeDetermined; ++i) {
        const unsigned qubitOffsetToNextElementInDimension = offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths[i] * elementQubitSize;

        std::vector<unsigned> firstQubitIndexPerElementInDimension = std::vector(numValuesPerDimensionOfVariable.at(i), qubitOffsetToNextElementInDimension);
        firstQubitIndexPerElementInDimension[0]                    = storedQubitIndices.firstQubitIndex;
        for (std::size_t j = 0; j < i; ++j) {
            firstQubitIndexPerElementInDimension[0] += (requiredValuesPerDimension[j] * offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths[j]) * elementQubitSize;
        }

        for (std::size_t j = 1; j < firstQubitIndexPerElementInDimension.size(); ++j) {
            firstQubitIndexPerElementInDimension[j] += firstQubitIndexPerElementInDimension[j - 1];
        }

        const auto& indexOfElementContainingQubit = std::ranges::lower_bound(std::as_const(firstQubitIndexPerElementInDimension), qubit);
        couldRequiredValuePerDimensionBeDetermined &= indexOfElementContainingQubit != firstQubitIndexPerElementInDimension.cend();
        const unsigned indexOffset    = indexOfElementContainingQubit != firstQubitIndexPerElementInDimension.cend() ? static_cast<unsigned>(*indexOfElementContainingQubit > qubit) : 0U;
        requiredValuesPerDimension[i] = static_cast<unsigned>(std::distance(firstQubitIndexPerElementInDimension.cbegin(), indexOfElementContainingQubit)) - indexOffset;
    }
    return couldRequiredValuePerDimensionBeDetermined ? std::make_optional(requiredValuesPerDimension) : std::nullopt;
}

AnnotatableQuantumComputation::AncillaryQuantumRegisterVariableLayout::AncillaryQuantumRegisterVariableLayout(const QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const InlinedQubitInformation& sharedQubitInliningInformation):
    BaseQuantumRegisterVariableLayout(coveredQubitIndicesOfQuantumRegister, quantumRegisterLabel) {
    // TODO: We should somewhere check the invariant that the first qubit index must be larger than the last qubit index.
    storedQubitIndices         = coveredQubitIndicesOfQuantumRegister;
    this->quantumRegisterLabel = quantumRegisterLabel;

    for (qc::Qubit firstQubitIndex = storedQubitIndices.firstQubitIndex; firstQubitIndex <= storedQubitIndices.lastQubitIndex; ++firstQubitIndex) {
        qubitInliningInformation.emplace(std::make_pair(firstQubitIndex, sharedQubitInliningInformation));
    }
}

bool AnnotatableQuantumComputation::AncillaryQuantumRegisterVariableLayout::mergeWithOtherAncillaryQubitRegister(const AncillaryQuantumRegisterVariableLayout& other) {
    if (storedQubitIndices.firstQubitIndex < other.storedQubitIndices.firstQubitIndex && storedQubitIndices.lastQubitIndex == other.storedQubitIndices.firstQubitIndex - 1) {
        return false;
    }

    const std::unordered_map<qc::Qubit, InlinedQubitInformation>* smallerInlineInformationLookup = &qubitInliningInformation;
    const std::unordered_map<qc::Qubit, InlinedQubitInformation>* largerInlineInformationLookup  = &other.qubitInliningInformation;
    if (qubitInliningInformation.size() >= other.qubitInliningInformation.size()) {
        smallerInlineInformationLookup = &other.qubitInliningInformation;
        largerInlineInformationLookup  = &qubitInliningInformation;
    }

    bool containsDuplicateInlineInformation = false;
    for (auto smallerInlineInformationLookupIterator = smallerInlineInformationLookup->begin(); smallerInlineInformationLookupIterator != smallerInlineInformationLookup->end() && !containsDuplicateInlineInformation; ++smallerInlineInformationLookupIterator) {
        containsDuplicateInlineInformation = largerInlineInformationLookup->contains(smallerInlineInformationLookupIterator->first);
    }

    if (containsDuplicateInlineInformation) {
        return false;
    }

    storedQubitIndices.lastQubitIndex = other.storedQubitIndices.lastQubitIndex;
    qubitInliningInformation.insert(other.qubitInliningInformation.cbegin(), other.qubitInliningInformation.cend());
    return true;
}

std::optional<AnnotatableQuantumComputation::BaseQuantumRegisterVariableLayout::QuantumRegisterQubitIndexLookupData> AnnotatableQuantumComputation::AncillaryQuantumRegisterVariableLayout::determineLookupDataForQubitFromQuantumRegister(const qc::Qubit qubit) const {
    if (storedQubitIndices.firstQubitIndex > qubit || storedQubitIndices.lastQubitIndex < qubit) {
        return std::nullopt;
    }

    const qc::Qubit relativeQubitIndexInQuantumRegister = qubit - storedQubitIndices.firstQubitIndex;
    const auto&     inliningInformationOfQubit          = qubitInliningInformation.find(qubit);

    return QuantumRegisterQubitIndexLookupData({.quantumRegisterLabel                           = quantumRegisterLabel,
                                                .accessedValuePerDimensionOfElementStoringQubit = std::vector({0U}),
                                                .relativeQubitIndexInElementStoringQubit        = relativeQubitIndexInQuantumRegister,
                                                .inlinedQubitInformation                        = inliningInformationOfQubit != qubitInliningInformation.cend() ? std::make_optional(inliningInformationOfQubit->second) : std::nullopt});
}

// END Quantum register variable alyout functionality

std::string AnnotatableQuantumComputation::buildQubitLabelForQubitOfVariableInQuantumRegister(const std::string& quantumRegisterLabel, const std::vector<unsigned>& accessedValuePerDimension, std::size_t relativeQubitIndexInElement) {
    std::string generatedQubitLabel = quantumRegisterLabel;
    for (const auto accessedValueOfDimension: accessedValuePerDimension) {
        generatedQubitLabel += "[" + std::to_string(accessedValueOfDimension) + "]";
    }
    generatedQubitLabel += "." + std::to_string(relativeQubitIndexInElement);
    return generatedQubitLabel;
}

std::optional<std::size_t> AnnotatableQuantumComputation::determineIndexOfQuantumRegisterStoringQubit(const qc::Qubit qubit) const {
    // TODO: Comment Returns the first element in variable element indices lookup whose firstQubitIndex >= qubit. This is equivalent to a binary search ?
    const auto& iteratorToQuantumRegisterStoringQubit = std::lower_bound(quantumRegisterAssociatedVariableLayouts.cbegin(), quantumRegisterAssociatedVariableLayouts.cend(), qubit, [](const std::unique_ptr<BaseQuantumRegisterVariableLayout>& quantumRegisterVariableLayout, const qc::Qubit qubit) {
        return quantumRegisterVariableLayout->storedQubitIndices.lastQubitIndex < qubit;
    });
    return iteratorToQuantumRegisterStoringQubit != quantumRegisterAssociatedVariableLayouts.cend() ? std::make_optional(std::distance(quantumRegisterAssociatedVariableLayouts.cbegin(), iteratorToQuantumRegisterStoringQubit)) : std::nullopt;
}
// END NON-PUBLIC FUNCTIONALITY
