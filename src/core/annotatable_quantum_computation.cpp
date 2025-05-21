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

#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace syrec;

bool AnnotatableQuantumComputation::addOperationsImplementingNotGate(const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(targetQubit) || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
        return false;
    }

    const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    const std::size_t  prevNumQuantumOperations = quantumComputation.getNops();
    quantumComputation.mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = quantumComputation.getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingCnotGate(const qc::Qubit controlQubit, const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(controlQubit) || !isQubitWithinRange(targetQubit) || controlQubit == targetQubit || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
        return false;
    }

    qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    gateControlQubits.emplace(controlQubit);

    const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
    quantumComputation.mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = quantumComputation.getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingToffoliGate(const qc::Qubit controlQubitOne, const qc::Qubit controlQubitTwo, const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(controlQubitOne) || !isQubitWithinRange(controlQubitTwo) || !isQubitWithinRange(targetQubit) || controlQubitOne == targetQubit || controlQubitTwo == targetQubit || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
        return false;
    }

    qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    gateControlQubits.emplace(controlQubitOne);
    gateControlQubits.emplace(controlQubitTwo);

    const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
    quantumComputation.mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = quantumComputation.getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingMultiControlToffoliGate(const qc::Controls& controlQubits, const qc::Qubit targetQubit) {
    if (!isQubitWithinRange(targetQubit) || std::any_of(controlQubits.cbegin(), controlQubits.cend(), [&](const qc::Control& control) { return !isQubitWithinRange(control.qubit) || control.qubit == targetQubit; }) || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
        return false;
    }

    qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
    gateControlQubits.insert(controlQubits.cbegin(), controlQubits.cend());
    if (gateControlQubits.empty()) {
        return false;
    }

    const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
    quantumComputation.mcx(gateControlQubits, targetQubit);

    const std::size_t currNumQuantumOperations = quantumComputation.getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations, {});
}

bool AnnotatableQuantumComputation::addOperationsImplementingFredkinGate(const qc::Qubit targetQubitOne, const qc::Qubit targetQubitTwo) {
    if (!isQubitWithinRange(targetQubitOne) || !isQubitWithinRange(targetQubitTwo) || targetQubitOne == targetQubitTwo || aggregateOfPropagatedControlQubits.count(targetQubitOne) != 0 || aggregateOfPropagatedControlQubits.count(targetQubitTwo) != 0) {
        return false;
    }
    const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());

    const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
    quantumComputation.mcswap(gateControlQubits, targetQubitOne, targetQubitTwo);

    const std::size_t currNumQuantumOperations = quantumComputation.getNops();
    return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations, currNumQuantumOperations, {});
}

std::optional<qc::Qubit> AnnotatableQuantumComputation::addNonAncillaryQubit(const std::string& qubitLabel, bool isGarbageQubit) const {
    if (!canQubitsBeAddedToQuantumComputation || qubitLabel.empty() || quantumComputation.getQuantumRegisters().count(qubitLabel) != 0) {
        return std::nullopt;
    }

    const auto            qubitIndex = static_cast<qc::Qubit>(quantumComputation.getNqubits());
    constexpr std::size_t qubitSize  = 1;
    // TODO: Should we also add classical registers here?
    quantumComputation.addQubitRegister(qubitSize, qubitLabel);
    if (isGarbageQubit) {
        quantumComputation.setLogicalQubitGarbage(qubitIndex);
    }
    return qubitIndex;
}

std::optional<qc::Qubit> AnnotatableQuantumComputation::addAncillaryQubit(const std::string& qubitLabel, bool initialStateOfQubit) {
    if (!canQubitsBeAddedToQuantumComputation || qubitLabel.empty() || quantumComputation.getQuantumRegisters().count(qubitLabel) != 0) {
        return std::nullopt;
    }
    const auto            qubitIndex = static_cast<qc::Qubit>(quantumComputation.getNqubits());
    constexpr std::size_t qubitSize  = 1;

    quantumComputation.addQubitRegister(qubitSize, qubitLabel);
    addedAncillaryQubitIndices.emplace(qubitIndex);

    if (initialStateOfQubit) {
        // Since ancillary qubits are assumed to have an initial value of
        // zero, we need to add an inversion gate to derive the correct
        // initial value of 1.
        // We can either use a simple X quantum operation to initialize the qubit with '1' but we should
        // probably also consider the active control qubits set in the currently active control qubit propagation scopes.
        if (!addOperationsImplementingNotGate(qubitIndex)) {
            return std::nullopt;
        }
    }
    return qubitIndex;
}

bool AnnotatableQuantumComputation::setQubitAncillary(qc::Qubit qubit) {
    if (!isQubitWithinRange(qubit)) {
        return false;
    }

    canQubitsBeAddedToQuantumComputation = false;
    quantumComputation.setLogicalQubitAncillary(qubit);
    return true;
}

std::size_t AnnotatableQuantumComputation::getNqubits() const {
    return quantumComputation.getNqubits();
}

std::vector<std::string> AnnotatableQuantumComputation::getQubitLabels() const {
    std::vector<std::string> qubitLabels(getNqubits(), "");
    for (const auto& quantumRegister: quantumComputation.getQuantumRegisters()) {
        const qc::Qubit qubitIndex = quantumRegister.second.getStartIndex();
        qubitLabels[qubitIndex]    = quantumRegister.first;
    }
    return qubitLabels;
}

AnnotatableQuantumComputation::GateAnnotationsLookup AnnotatableQuantumComputation::getAnnotationsOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const {
    if (indexOfQuantumOperationInQuantumComputation > annotationsPerQuantumOperation.size()) {
        return {};
    }
    return annotationsPerQuantumOperation[indexOfQuantumOperationInQuantumComputation];
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
    if (localControlLineScope.count(controlQubit) == 0) {
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

    if (localControlLineScope.count(controlQubit) == 0) {
        localControlLineScope.emplace(std::make_pair(controlQubit, aggregateOfPropagatedControlQubits.count(controlQubit) != 0));
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

// BEGIN NON-PUBLIC FUNCTIONALITY
bool AnnotatableQuantumComputation::isQubitWithinRange(const qc::Qubit qubit) const noexcept {
    return qubit < quantumComputation.getNqubits();
}

bool AnnotatableQuantumComputation::areQubitsWithinRange(const qc::Controls& qubitsToCheck) const noexcept {
    return std::all_of(qubitsToCheck.cbegin(), qubitsToCheck.cend(), [&](const qc::Control control) { return isQubitWithinRange(control.qubit); });
}

bool AnnotatableQuantumComputation::annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const GateAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation) {
    if (fromQuantumOperationIndex > annotationsPerQuantumOperation.size() || fromQuantumOperationIndex > toQuantumOperationIndex) {
        return false;
    }
    annotationsPerQuantumOperation.resize(toQuantumOperationIndex);

    GateAnnotationsLookup gateAnnotations = userProvidedAnnotationsPerQuantumOperation;
    for (const auto& [annotationKey, annotationValue]: activateGlobalQuantumOperationAnnotations) {
        gateAnnotations[annotationKey] = annotationValue;
    }

    for (std::size_t i = fromQuantumOperationIndex; i < toQuantumOperationIndex; ++i) {
        annotationsPerQuantumOperation[i] = userProvidedAnnotationsPerQuantumOperation;
    }
    return true;
}