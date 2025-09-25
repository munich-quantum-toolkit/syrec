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

#include "core/syrec/variable.hpp"
#include "dd/DDDefinitions.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/Operation.hpp"
#include "qubit_inlining_stack.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace syrec {
    /**
     * A class to build a MQT::Core QuantumComputation and offer functionality to annotate its quantum operations with string key-value pairs.
     */
    class AnnotatableQuantumComputation: public qc::QuantumComputation {
    public:
        using QuantumOperationAnnotationsLookup = std::map<std::string, std::string, std::less<>>;
        using SynthesisCostMetricValue          = std::uint64_t;

        /**
         * A wrapper for a qubit index range [first, last] in which the first qubit index is assumed to be smaller or equal to the last index.
         */
        struct QubitIndexRange {
            /**
             * The start index of the qubit index range.
             */
            qc::Qubit firstQubitIndex;
            /**
             * The last index of the qubit index range.
             */
            qc::Qubit lastQubitIndex;
        };

        /**
         * Stores debug information about the ancillary and local module variable qubits that can be used to determine the origin of the qubit in the
         * SyReC program or to determine the user declared identifier of the associated variable for a qubit. This information is not available for the
         * parameters of a SyReC module.
         */
        struct InlinedQubitInformation {
            /**
             * The user declared qubit label is generated from the associated variable declaration.
             */
            std::optional<std::string> userDeclaredQubitLabel;
            /**
             *  The inline stack to determine the origin of the qubit in the hierarchy of Call-/UncallStatements of a SyReC program. The last entry of the
             *  stack is equal to the module in which the associated variable of the qubit was declared.
             */
            std::optional<QubitInliningStack::ptr> inlineStack;
        };

        /**
         * A flag usable to control which type of qubit label should be generated when trying to fetch the label of a qubit.
         */
        enum QubitLabelType : std::uint8_t {
            /**
             * Generate the qubit label using the internal identifier of the qubit.
             */
            Internal,
            /**
             * Generate the qubit label using the user declared identifier of the associated syrec::Variable. Not usable for ancillary qubits since no user declared qubit label can be defined for this type of qubits.
             */
            UserDeclared
        };

        [[nodiscard]] bool addOperationsImplementingNotGate(qc::Qubit targetQubit);
        [[nodiscard]] bool addOperationsImplementingCnotGate(qc::Qubit controlQubit, qc::Qubit targetQubit);
        [[nodiscard]] bool addOperationsImplementingToffoliGate(qc::Qubit controlQubitOne, qc::Qubit controlQubitTwo, qc::Qubit targetQubit);
        [[nodiscard]] bool addOperationsImplementingMultiControlToffoliGate(const qc::Controls& controlQubitsSet, qc::Qubit targetQubit);
        [[nodiscard]] bool addOperationsImplementingFredkinGate(qc::Qubit targetQubitOne, qc::Qubit targetQubitTwo);

        /**
         * Add a quantum register for the qubits of a SyReC variable to the quantum computation.
         * @param quantumRegisterLabel The label for the to be added quantum register. Must not be empty and no other qubit or quantum register with the same name must exist in the quantum computation.
         * @param variable The SyReC variable for which qubits shall be generated. Total number of elements stored in variable must be larger than zero. Bitwidth of variable must be larger than 0.
         * @param areGeneratedQubitsGarbage Whether the generated qubits are garbage qubits.
         * @param optionalInliningInformation Optional debug information to determine the origin of the qubits in the associated SyReC program.
         * @return The index of the first generated non-ancillary qubit for the \p variable in the quantum computation, std::nullopt if the validation of the \p quantumRegisterLabel or \p variable failed, no further qubits can be added due to a qubit being set to be ancillary via \see AnnotatableQuantumComputation#setQubitAncillary or if the inline information is invalid (empty or no user defined qubit label or invalid or empty inline stack).
         */
        [[nodiscard]] std::optional<qc::Qubit> addQuantumRegisterForSyrecVariable(const std::string& quantumRegisterLabel, const Variable& variable, bool areGeneratedQubitsGarbage, const std::optional<InlinedQubitInformation>& optionalInliningInformation = std::nullopt);

        /**
         * Add a quantum register for a number of preliminary ancillary qubits in the quantum computation.
         * @param quantumRegisterLabel The label for the created quantum register. A new quantum register is only created if the ancillary qubits could not be appended to an adjacent ancillary qubit register.
         * @param initialStateOfAncillaryQubits A collection defining how many ancillary qubits should be added but also their initial values (each ancillary qubit initialized with '1' will cause the addition of a controlled X gate to the quantum computation). Cannot be empty.
         * @param sharedInliningInformation The inline information recorded for all ancillary qubits generated with this call.
         * @return The index of the first generated ancillary qubits. If more than one ancillary qubits wa added then their indices are adjacent to the return index.
         * @remark If no more qubits are to be added to the quantum computation then the preliminary ancillary qubits need to be promoted to actual ancillary qubits with a call to AnnotatableQuantumComputation::promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits().
         */
        [[nodiscard]] std::optional<qc::Qubit> addPreliminaryAncillaryRegisterOrAppendToAdjacentOne(const std::string& quantumRegisterLabel, const std::vector<bool>& initialStateOfAncillaryQubits, const InlinedQubitInformation& sharedInliningInformation);

        /**
         * Promote the added preliminary ancillary qubits to "actual" ancillary qubits in the quantum computation.
         * @remark After the promotion of the preliminary ancillary qubits was performed no further qubits can be added to the quantum computation.
         */
        void promotePreliminaryAncillaryQubitsToDefinitiveAncillaryQubits();

        /**
         * Determine the label of a qubit based on its location and the associated variable layout of the SyReC variable stored in the quantum register that stores the qubit.
         * @param qubit The qubit whose label shall be determined.
         * @param qubitLabelType The type of qubit label to generated. Can either be the internally or user declared one.
         * @return Returns the label of the qubit in the form of a stringified syrec::VariableAccess (e.g. the label generated for qubit 3 of the syrec::Variable a[2][3](2) is equal to a[0][1].1), otherwise std::nullopt.
         */
        [[nodiscard]] std::optional<std::string> getQubitLabel(qc::Qubit qubit, QubitLabelType qubitLabelType) const;
        [[nodiscard]] const qc::Operation*       getQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const;

        /**
        * Replay a set of already existing quantum operations by readding the quantum operations to the quantum computation.
        * @param indexOfFirstQuantumOperationToReplayInQuantumComputation The index of the first quantum operation to replay. The index of the first quantum operation to replay is allowed to be larger than the index of the last quantum operation to replay.
        * @param indexOfLastQuantumOperationToReplayInQuantumComputation The index of the last quantum operation to replay.
        * @return Whether the indices referenced an existing quantum operation and whether all requested quantum operation could be replayed.
        * @remark While a quantum operation can by added to the qc::QuantumComputation with the qc::QuantumComputation.emplace_back(...) function, the required quantum gate annotations are not added to the annotatable quantum computation. Additionally, with this function we can somewhat restrict the user to only add operations that can be simulated by the syrec::SimpleSimulation (assuming that the replayed operations where generated by calls to the addOperationsImplementingXGate functions of the annotatable quantum computation).
        * @remark This function is not thread-safe. Additionally, the annotations of the replayed operations are not copied to the newly created operations.
        */
        [[nodiscard]] bool replayOperationsAtGivenIndexRange(std::size_t indexOfFirstQuantumOperationToReplayInQuantumComputation, std::size_t indexOfLastQuantumOperationToReplayInQuantumComputation);

        [[nodiscard]] QuantumOperationAnnotationsLookup getAnnotationsOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const;
        [[nodiscard]] SynthesisCostMetricValue          getQuantumCostForSynthesis() const;
        [[nodiscard]] SynthesisCostMetricValue          getTransistorCostForSynthesis() const;

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
         * @param controlQubit The control qubit to deregister.
         * @return Whether the control qubit exists in the internally used qc::QuantumComputation and was deregistered from the last activated propagation scope.
         */
        [[nodiscard]] bool deregisterControlQubitFromPropagationInCurrentScope(qc::Qubit controlQubit);

        /**
         * Register a control qubit in the last activated control qubit propagation scope.
         *
         * @remarks If no active local control qubit scope exists, a new one is created.
         * @param controlQubit The control qubit to register.
         * @return Whether the control qubit exists in the \p quantumComputation and was registered in the last activated propagation scope.
         */
        [[nodiscard]] bool registerControlQubitForPropagationInCurrentAndNestedScopes(qc::Qubit controlQubit);

        /**
         * Register or update a global quantum operation annotation. Global quantum operation annotations are added to all quantum operations added to the internally used qc::QuantumComputation.
         * Already existing quantum computations in the qc::QuantumComputation are not modified.
         * @param key The key of the global quantum operation annotation.
         * @param value The value of the global quantum operation annotation.
         * @return Whether an existing global annotation was updated.
         */
        [[maybe_unused]] bool setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value);

        /**
         * Remove a global gate annotation. Existing annotations of the gates of the circuit are not modified.
         * @param key The key of the global gate annotation to be removed.
         * @return Whether a global gate annotation was removed.
         */
        [[maybe_unused]] bool removeGlobalQuantumOperationAnnotation(const std::string_view& key);

        /**
         * Set a key value annotation for a quantum operation.
         * @param indexOfQuantumOperationInQuantumComputation The index of the quantum operation in the quantum computation.
         * @param annotationKey The key of the quantum operation annotation.
         * @param annotationValue The value of the quantum operation annotation.
         * @return Whether an operation at the user-provided index existed in the quantum operation.
         */
        [[maybe_unused]] bool setOrUpdateAnnotationOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation, const std::string_view& annotationKey, const std::string& annotationValue);

        /**
         * Get the inlined qubit information.
         * @param qubit The qubit whose inline information shall be fetched.
         * @return The inline information of the qubit if such information exists, otherwise std::nullopt is returned.
         */
        [[nodiscard]] std::optional<InlinedQubitInformation> getInlinedQubitInformation(qc::Qubit qubit) const;

    protected:
        [[maybe_unused]] bool annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const QuantumOperationAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation);
        [[nodiscard]] bool    isQubitWithinRange(qc::Qubit qubit) const noexcept;

        std::unordered_set<qc::Qubit>                    aggregateOfPropagatedControlQubits;
        std::vector<std::unordered_map<qc::Qubit, bool>> controlQubitPropgationScopes;
        bool                                             canQubitsBeAddedToQuantumComputation = true;

        QuantumOperationAnnotationsLookup activateGlobalQuantumOperationAnnotations;

        // We are assuming that no operations in the qc::QuantumComputation are removed (i.e. by applying qc::CircuitOptimizer) and will thus use the index of the quantum operation
        // as the search key in the container storing the annotations per quantum operation.
        std::vector<QuantumOperationAnnotationsLookup> annotationsPerQuantumOperation;

        struct BaseQuantumRegisterVariableLayout {
            struct QubitInVariableLayoutData {
                std::string                            quantumRegisterLabel;
                std::vector<unsigned>                  accessedValuePerDimensionOfElementStoringQubit;
                qc::Qubit                              relativeQubitIndexInElementStoringQubit;
                std::optional<InlinedQubitInformation> inlinedQubitInformation;
            };

            BaseQuantumRegisterVariableLayout(const QubitIndexRange storedQubitIndices, std::string quantumRegisterLabel):
                storedQubitIndices(storedQubitIndices), quantumRegisterLabel(std::move(quantumRegisterLabel)) {}

            virtual ~BaseQuantumRegisterVariableLayout()                                                                             = default;
            [[nodiscard]] virtual std::optional<QubitInVariableLayoutData> determineQubitInVariableLayoutData(qc::Qubit qubit) const = 0;
            [[nodiscard]] unsigned                                         getNumberOfQubitsInQuantumRegister() const { return storedQubitIndices.lastQubitIndex - storedQubitIndices.firstQubitIndex + 1U; }

            QubitIndexRange storedQubitIndices;
            std::string     quantumRegisterLabel;
        };

        struct NonAncillaryQuantumRegisterVariableLayout final: BaseQuantumRegisterVariableLayout {
            [[nodiscard]] std::optional<QubitInVariableLayoutData> determineQubitInVariableLayoutData(qc::Qubit qubit) const override;
            [[nodiscard]] std::optional<std::vector<unsigned>>     getRequiredValuesPerDimensionToAccessQubitOfVariable(qc::Qubit qubit) const;

            NonAncillaryQuantumRegisterVariableLayout(QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const std::vector<unsigned>& numValuesPerDimensionOfVariable, unsigned qubitSizeOfElementInVariable, const std::optional<InlinedQubitInformation>& optionalSharedInlinedQubitInformation);

            unsigned                               elementQubitSize;
            std::vector<unsigned>                  numValuesPerDimensionOfVariable;
            std::vector<unsigned>                  offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths;
            std::optional<InlinedQubitInformation> optionalSharedInlinedQubitInformation;
        };

        struct AncillaryQuantumRegisterVariableLayout final: BaseQuantumRegisterVariableLayout {
            [[nodiscard]] std::optional<QubitInVariableLayoutData> determineQubitInVariableLayoutData(qc::Qubit qubit) const override;
            [[nodiscard]] bool                                     appendQubitRange(QubitIndexRange qubitIndexRange, const InlinedQubitInformation& sharedInlinedQubitInformation);

            AncillaryQuantumRegisterVariableLayout(QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const InlinedQubitInformation& sharedInlinedQubitInformation);

            struct SharedQubitRangeInlineInformation {
                QubitIndexRange         coveredQubitIndexRange;
                InlinedQubitInformation inlinedQubitInformation;

                SharedQubitRangeInlineInformation(const QubitIndexRange coveredQubitIndexRange, InlinedQubitInformation inlinedQubitInformation): coveredQubitIndexRange(coveredQubitIndexRange), inlinedQubitInformation(std::move(inlinedQubitInformation)) {}
            };
            std::vector<SharedQubitRangeInlineInformation> sharedQubitRangeInlineInformationLookup;
        };

        [[nodiscard]] std::optional<std::size_t> determineIndexOfQuantumRegisterStoringQubit(qc::Qubit qubit) const;
        [[nodiscard]] static std::string         buildQubitLabelForQubitOfVariableInQuantumRegister(const std::string& quantumRegisterLabel, const std::vector<unsigned>& accessedValuePerDimension, std::size_t relativeQubitIndexInElement);

        std::vector<std::unique_ptr<BaseQuantumRegisterVariableLayout>> quantumRegisterAssociatedVariableLayouts;
    };
} // namespace syrec
