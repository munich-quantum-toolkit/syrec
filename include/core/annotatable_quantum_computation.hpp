/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/Operation.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace syrec {
    /**
     * A class to build a MQT::Core QuantumComputation and offer functionality to annotate its quantum operations with string key-value pairs.
     */
    class AnnotatableQuantumComputation: public qc::QuantumComputation {
    public:
        using QuantumOperationAnnotationsLookup = std::map<std::string, std::string, std::less<>>;

        [[maybe_unused]] bool addOperationsImplementingNotGate(qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingCnotGate(qc::Qubit controlQubit, qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingToffoliGate(qc::Qubit controlQubitOne, qc::Qubit controlQubitTwo, qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingMultiControlToffoliGate(const qc::Controls& controlQubitsSet, qc::Qubit targetQubit);
        [[maybe_unused]] bool addOperationsImplementingFredkinGate(qc::Qubit targetQubitOne, qc::Qubit targetQubitTwo);

        /**
         * Add a non-ancillary qubit to the quantum computation.
         * @param qubitLabel The label of the ancillary qubit. Must be non-empty.
         * @param isGarbageQubit Whether the qubit is a garbage qubit.
         * @return The index of the non-ancillary qubit in the quantum computation, std::nullopt if either a qubit with the same label already exists or no further qubits can be added due to a qubit being set to be ancillary via \see AnnotatableQuantumComputation#setQubitAncillary.
         */
        [[nodiscard]] std::optional<qc::Qubit> addNonAncillaryQubit(const std::string& qubitLabel, bool isGarbageQubit);

        /**
         * Add a preliminary ancillary qubit to the quantum computation. Ancillary qubits added need to be explicitly marked as such via the \see AnnotatableQuantumComputation#setQubitAncillary call.
         * @param qubitLabel The label of the ancillary qubit. Must be non-empty.
         * @param initialStateOfQubit The initial state of the ancillary qubits. Is assumed to be 0 by default. The initial state of 1 is achieved by adding an X quantum operation.
         * @return The index of the ancillary qubit in the quantum computation, std::nullopt if a qubit with the same label already exists or no further qubits can be added due to a qubit being set to be ancillary via \see AnnotatableQuantumComputation#setQubitAncillary.
         */
        [[nodiscard]] std::optional<qc::Qubit> addPreliminaryAncillaryQubit(const std::string& qubitLabel, bool initialStateOfQubit);

        /**
         * Return the indices of the preliminary ancillary qubits added via \see AnnotatableQuantumComputation#addAncillaryQubit.
         *
         * Qubits not added as ancillary ones to the quantum computation will not be considered as such.
         * @return The indices of the ancillary qubits added to the quantum computation via \see AnnotatableQuantumComputation#addAncillaryQubit.
         */
        [[nodiscard]] std::unordered_set<qc::Qubit> getAddedPreliminaryAncillaryQubitIndices() const { return addedAncillaryQubitIndices; }

        /**
         * Promote a previously added preliminary ancillary qubit status to a permanent one. No qubits can be added to the quantum computation after this point.
         * @param qubit The index of the qubit in the quantum computation.
         * @return Whether the qubit index was known in the quantum computation. If the index is not known, qubits can still be added to the quantum computation.
         */
        [[nodiscard]] bool                              promotePreliminaryAncillaryQubitToDefinitiveAncillary(qc::Qubit qubit);
        [[nodiscard]] std::vector<std::string>          getQubitLabels() const;
        [[nodiscard]] qc::Operation*                    getQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const;
        [[nodiscard]] QuantumOperationAnnotationsLookup getAnnotationsOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const;

        /**
         * Activate a new control qubit propagation scope.
         *
         * @remarks All active control qubits registered in the currently active propagation scopes will be added to any quantum operation, created by any of the addOperationsImplementingXGate functions, in the qc::QuantumComputation.
         * Already existing quantum operations will not be modified.
         */
        void activateControlQubitPropagationScope();

        /**
         * Deactivates the last activated control qubit propagation scope.
         *
         * @remarks
         * All control qubits registered in the last activated control qubit propagation scope are removed from the aggregate of all active control qubits.
         * Control qubits registered for propagation prior to the last activated control qubit propagation scope and deregistered in said scope are registered for propagation again. \n
         * \n
         * Example:
         * Assuming that the aggregate A contains the control qubits (1,2,3), a propagation scope is activated and the control qubits (3,4)
         * registered setting the control qubit aggregate to (1,2,3,4). After the local scope is deactivated, only the control qubit 4 that was registered in the last activate propagation scope,
         * is removed from the aggregate while control qubit 3 will remain in the aggregate due to it also being registered in a parent scope thus the aggregate will be equal to (1,2,3) again.
         */
        void deactivateControlQubitPropagationScope();

        /**
         * Deregister a control qubit from the last activated control qubit propagation scope.
         *
         * @remarks The control qubit is only removed from the aggregate of all registered control qubits if the last activated local scope registered the @p controlQubit.
         * The deregistered control qubit is not 'inherited' by any quantum computation added to the internally used qc::QuantumComputation while the current scope is active. Additionally,
         * the deregistered control qubits are not filtered from the user defined control qubits provided as parameters to any of the addOperationsImplementingXGate calls.
         * @param controlQubit The control qubit to deregister
         * @return Whether the control qubit exists in the internally used qc::QuantumComputation and was deregistered from the last activated propagation scope.
         */
        [[maybe_unused]] bool deregisterControlQubitFromPropagationInCurrentScope(qc::Qubit controlQubit);

        /**
         * Register a control qubit in the last activated control qubit propagation scope.
         *
         * @remarks If no active local control qubit scope exists, a new one is created.
         * @param controlQubit The control qubit to register
         * @return Whether the control qubit exists in the \p quantumComputation and was registered in the last activated propagation scope.
         */
        [[maybe_unused]] bool registerControlQubitForPropagationInCurrentAndNestedScopes(qc::Qubit controlQubit);

        /**
         * Register or update a global quantum operation annotation. Global quantum operation annotations are added to all quantum operations added to the internally used qc::QuantumComputation.
         * Already existing quantum computations in the qc::QuantumComputation are not modified.
         * @param key The key of the global quantum operation annotation
         * @param value The value of the global quantum operation annotation
         * @return Whether an existing global annotation was updated.
         */
        [[maybe_unused]] bool setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value);

        /**
         * Remove a global gate annotation. Existing annotations of the gates of the circuit are not modified.
         * @param key The key of the global gate annotation to be removed
         * @return Whether a global gate annotation was removed.
         */
        [[maybe_unused]] bool removeGlobalQuantumOperationAnnotation(const std::string_view& key);

        /**
         * Set a key value annotation for a quantum operation
         * @param indexOfQuantumOperationInQuantumComputation The index of the quantum operation in the quantum computation
         * @param annotationKey The key of the quantum operation annotation
         * @param annotationValue The value of the quantum operation annotation
         * @return Whether an operation at the user-provided index existed in the quantum operation
         */
        [[maybe_unused]] bool setOrUpdateAnnotationOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation, const std::string_view& annotationKey, const std::string& annotationValue);

    protected:
        [[maybe_unused]] bool annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const QuantumOperationAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation);
        [[nodiscard]] bool    isQubitWithinRange(qc::Qubit qubit) const noexcept;

        std::unordered_set<qc::Qubit>                    aggregateOfPropagatedControlQubits;
        std::vector<std::unordered_map<qc::Qubit, bool>> controlQubitPropgationScopes;
        bool                                             canQubitsBeAddedToQuantumComputation = true;

        // To be able to use a std::string_view key lookup (heterogeneous lookup) in a std::map/std::unordered_set
        // we need to define the transparent comparator (std::less<>). This feature is only available starting with C++17
        // For further information, see: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3465.pdf
        QuantumOperationAnnotationsLookup activateGlobalQuantumOperationAnnotations;

        // We are assuming that no operations in the qc::QuantumComputation are removed (i.e. by applying qc::CircuitOptimizer) and will thus use the index of the quantum operation
        // as the search key in the container storing the annotations per quantum operation.
        std::vector<QuantumOperationAnnotationsLookup> annotationsPerQuantumOperation;
        std::unordered_set<qc::Qubit>                  addedAncillaryQubitIndices;
    };
} // namespace syrec
