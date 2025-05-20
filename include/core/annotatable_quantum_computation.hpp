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
    class AnnotatableQuantumComputation {
    public:
        explicit AnnotatableQuantumComputation(qc::QuantumComputation& quantumComputation):
            quantumComputation(quantumComputation) {}

        [[maybe_unused]] bool                       addOperationsImplementingNotGate(qc::Qubit targetQubit);
        [[maybe_unused]] bool                       addOperationsImplementingCnotGate(qc::Qubit controlQubit, qc::Qubit targetQubit);
        [[maybe_unused]] bool                       addOperationsImplementingToffoliGate(qc::Qubit controlQubitOne, qc::Qubit controlQubitTwo, qc::Qubit targetQubit);
        [[maybe_unused]] bool                       addOperationsImplementingMultiControlToffoliGate(const std::unordered_set<qc::Qubit>& controlQubitsSet, qc::Qubit targetQubit);
        [[maybe_unused]] bool                       addOperationsImplementingFredkinGate(qc::Qubit targetQubitOne, qc::Qubit targetQubitTwo);
        [[nodiscard]] std::optional<qc::Qubit>      addNonAncillaryQubit(const std::string& qubitLabel, bool isGarbageQubit) const;
        [[nodiscard]] std::optional<qc::Qubit>      addAncillaryQubit(const std::string& qubitLabel, bool initialStateOfQubit);
        [[nodiscard]] std::unordered_set<qc::Qubit> getAddedAncillaryQubitIndices() const { return addedAncillaryQubitIndices; }
        [[nodiscard]] bool                          setQubitAncillary(qc::Qubit qubit) const;

        [[nodiscard]] std::size_t              getNqubits() const;
        [[nodiscard]] std::vector<std::string> getQubitLabels() const;

        [[nodiscard]] auto cbegin() const {
            return quantumComputation.cbegin();
        }

        [[nodiscard]] auto begin() const {
            return quantumComputation.begin();
        }

        [[nodiscard]] auto cend() const {
            return quantumComputation.cend();
        }

        [[nodiscard]] auto end() const {
            return quantumComputation.end();
        }

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
         * @return Whether an existing annotation was updated.
         */
        [[maybe_unused]] bool setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value);

        /**
         * Remove a global gate annotation. Existing annotations of the gates of the circuit are not modified.
         * @param key The key of the global gate annotation to be removed
         * @return Whether a global gate annotation was removed.
         */
        [[maybe_unused]] bool removeGlobalQuantumOperationAnnotation(const std::string_view& key);

    protected:
        using GateAnnotationsLookup = std::map<std::string, std::string, std::less<>>;

        [[maybe_unused]] bool annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const GateAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation);
        [[nodiscard]] bool    isQubitWithinRange(qc::Qubit qubit) const noexcept;
        [[nodiscard]] bool    areQubitsWithinRange(const std::unordered_set<qc::Qubit>& qubitsToCheck) const noexcept;

        // TODO: Can we find a solution that does not require a pass of a reference but use a smart pointer instead
        qc::QuantumComputation&                          quantumComputation; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        std::unordered_set<qc::Qubit>                    aggregateOfPropagatedControlQubits;
        std::vector<std::unordered_map<qc::Qubit, bool>> controlQubitPropgationScopes;

        // To be able to use a std::string_view key lookup (heterogeneous lookup) in a std::map/std::unordered_set
        // we need to define the transparent comparator (std::less<>). This feature is only available starting with C++17
        // For further information, see: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3465.pdf
        GateAnnotationsLookup activateGlobalQuantumOperationAnnotations;

        // We are assuming that no operations in the qc::QuantumComputation are removed (i.e. by applying qc::CircuitOptimizer) and will thus use the index of the quantum operation
        // as the search key in the container storing the annotations per quantum operation.
        std::vector<GateAnnotationsLookup> annotationsPerQuantumOperation;
        std::unordered_set<qc::Qubit>      addedAncillaryQubitIndices;
    };
} // namespace syrec