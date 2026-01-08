/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
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
#include "qubit_inlining_stack.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace syrec {
    /**
     * A class to build a MQT::Core QuantumComputation and offer functionality to optionally annotate its quantum operations with string key-value pairs.
     */
    class AnnotatableQuantumComputation: public qc::QuantumComputation {
    public:
        using QuantumOperationAnnotationsLookup = std::map<std::string, std::string, std::less<>>;
        using SynthesisCostMetricValue          = std::uint64_t;

        /**
         * A wrapper for a qubit index range [first, last] in which the firstQubitIndex is less than or equal to the last qubit index.
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

        /**
         * A simpler container for the layout information about a SyReC variable.
         */
        struct AssociatedVariableLayoutInformation {
            /**
             * The number of values for each dimension of a SyReC variable
             */
            std::vector<unsigned> numValuesPerDimension;
            /**
             * The bitwidth of each element in the SyReC variable.
             */
            unsigned bitwidth;
        };

        AnnotatableQuantumComputation() = default;

        /**
         * Initialize a new annotatable quantum computation. Since no quantum operation or any of its annotations can be removed via the public interface of this class, enabling or disabling the generation of
         * quantum gate annotations can only be performed in this constructor. Additionally, since the annotations will almost always only be used for debugging purposes or are to be displayed in a GUI, users should be able to
         * enable/disable the generation of this debug information depending on whether it is actually needed.
         * @param generateQuantumGateAnnotations Define whether this instance supports the generation of quantum operation annotations.
         */
        explicit AnnotatableQuantumComputation(const bool generateQuantumGateAnnotations):
            generateQuantumOperationAnnotations(generateQuantumGateAnnotations) {}

        /**
         * Determine whether the generation of quantum operation annotations is supported by the annotatable quantum computation.
         * @return Whether the generation of quantum operation annotations is supported.
         */
        [[nodiscard]] bool isGenerationOfQuantumOperationAnnotationsEnabled() const;

        /**
         * Add a quantum operation representing a NOT gate to the quantum computation.
         * @param targetQubit The target qubit of the NOT gate, said qubit must be in the range of the known qubits of the quantum computation.
         * @return Whether the quantum operation for the NOT gate could be added to the quantum computation. Additionally, the target qubit cannot be equal to any control qubit currently registered to be propagated.
         */
        [[nodiscard]] bool addOperationsImplementingNotGate(qc::Qubit targetQubit);

        /**
         * Add a quantum operation representing a CNOT gate to the quantum computation.
         * @param controlQubit The control qubit of the CNOT gate, said qubit must be in the range of the known qubits of the quantum computation. Cannot be equal to the \p targetQubit.
         * @param targetQubit The target qubit of the CNOT gate, said qubit must be in the range of the known qubits of the quantum computation. Cannot be equal to the \p controlQubit. Additionally, the target qubit cannot be equal to any control qubit currently registered to be propagated.
         * @return Whether the quantum operation for the CNOT gate could be added to the quantum computation.
         */
        [[nodiscard]] bool addOperationsImplementingCnotGate(qc::Qubit controlQubit, qc::Qubit targetQubit);

        /**
         * Add a quantum operation representing a Toffoli gate to the quantum computation.
         * @param controlQubitOne The first control qubit of the Toffoli gate, said qubit must be in the range of the known qubits of the quantum computation. Cannot be equal to the \p targetQubit or \p controlQubitTwo.
         * @param controlQubitTwo The second control qubit of the Toffoli gate, said qubit must be in the range of the known qubits of the quantum computation. Cannot be equal to the \p targetQubit or \p controlQubitOne.
         * @param targetQubit The target qubit of the Toffoli gate, said qubit must be in the range of the known qubits of the quantum computation. Cannot be equal to the \p controlQubitOne or \p controlQubitTwo. Additionally, the target qubit cannot be equal to any control qubit currently registered to be propagated.
         * @return Whether the quantum operation for the Toffoli gate could be added to the quantum computation.
         */
        [[nodiscard]] bool addOperationsImplementingToffoliGate(qc::Qubit controlQubitOne, qc::Qubit controlQubitTwo, qc::Qubit targetQubit);

        /**
         * Add a quantum operation representing a multi-control Toffoli gate to the quantum computation.
         * @param controlQubits The set of control qubits of the multi-control Toffoli gate which must contain at least one control qubit. Cannot be equal to the \p targetQubit.
         * @param targetQubit The target qubit of the multi-control Toffoli gate, said qubit must be in the range of the known qubits of the quantum computation. Cannot be equal to any control qubit of \p controlQubitSet or currently registered to be propagated control qubits.
         * @return Whether the quantum operation for the multi-control Toffoli gate could be added to the quantum computation.
         */
        [[nodiscard]] bool addOperationsImplementingMultiControlToffoliGate(const qc::Controls& controlQubits, qc::Qubit targetQubit);

        /**
         * Add a quantum operation representing a Fredkin gate to the quantum computation.
         * @param targetQubitOne The first target qubit of the Fredkin gate. Cannot be equal to the \p targetQubitTwo or any of the currently registered to be propagated control qubits.
         * @param targetQubitTwo The second target qubit of the Fredkin gate. Cannot be equal to the \p targetQubitOne or any of the currently registered to be propagated control qubits.
         * @return Whether the quantum operation for the Fredkin gate could be added to the quantum computation.
         */
        [[nodiscard]] bool addOperationsImplementingFredkinGate(qc::Qubit targetQubitOne, qc::Qubit targetQubitTwo);

        /**
         * Add a quantum register for the qubits of a SyReC variable to the quantum computation.
         * @param quantumRegisterLabel The label for the to be added quantum register. Must not be empty and no other qubit or quantum register with the same name must exist in the quantum computation.
         * @param associatedVariableLayoutInformation Layout information about the associated SyReC variable that is used to determine the number of qubits to generate. Total number of elements stored in variable must be larger than zero. Bitwidth of variable must be larger than 0.
         * @param areGeneratedQubitsGarbage Whether the generated qubits are garbage qubits.
         * @param optionalInliningInformation Optional debug information to determine the origin of the qubits in the associated SyReC program.
         * @return The index of the first generated non-ancillary qubit for the \p variable in the quantum computation, std::nullopt if the validation of the \p quantumRegisterLabel or \p variable failed, no further qubits can be added due to a qubit being set to be ancillary via \see AnnotatableQuantumComputation#setQubitAncillary or if the inline information is invalid (empty or no user defined qubit label or invalid or empty inline stack).
         */
        [[nodiscard]] std::optional<qc::Qubit> addQuantumRegisterForSyrecVariable(const std::string& quantumRegisterLabel, const AssociatedVariableLayoutInformation& associatedVariableLayoutInformation, bool areGeneratedQubitsGarbage, const std::optional<InlinedQubitInformation>& optionalInliningInformation = std::nullopt);

        /**
         * Add a quantum register for a number of preliminary ancillary qubits in the quantum computation.
         * @param quantumRegisterLabel The label for the created quantum register. A new quantum register is only created if the ancillary qubits could not be appended to an adjacent ancillary qubit register.
         * @param initialStateOfAncillaryQubits A collection defining how many ancillary qubits should be added but also their initial values (each ancillary qubit initialized with '1' will cause the addition of a controlled X gate to the quantum computation). Cannot be empty.
         * @param sharedInliningInformation The inline information recorded for all ancillary qubits generated with this call.
         * @return The index of the first generated ancillary qubits. If more than one ancillary qubits was added then their indices are adjacent to the returned index.
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
         * @param qubitLabelType The type of qubit label to generate. Can either be the internal or user declared one.
         * @return Returns the label of the qubit in the form of a stringified syrec::VariableAccess (e.g. the label to generate for qubit 3 of the syrec::Variable a[2][3](2) is equal to a[0][1].1), otherwise std::nullopt.
         */
        [[nodiscard]] std::optional<std::string> getQubitLabel(qc::Qubit qubit, QubitLabelType qubitLabelType) const;

        /**
         * Get a pointer to the quantum operation at a given index in the quantum computation.
         * @param indexOfQuantumOperationInQuantumComputation The index to the quantum operation in the quantum computation.
         * @return A pointer to the quantum operation if an operation at the given index existed in the quantum computation, otherwise nullptr.
         */
        [[nodiscard]] const qc::Operation* getQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const;

        /**
        * Replay a set of already existing quantum operations by readding the quantum operations to the quantum computation.
        * @param indexOfFirstQuantumOperationToReplayInQuantumComputation The index of the first quantum operation to replay. The index of the first quantum operation to replay is allowed to be larger than the index of the last quantum operation to replay.
        * @param indexOfLastQuantumOperationToReplayInQuantumComputation The index of the last quantum operation to replay.
        * @return Whether the indices referenced an existing quantum operation and whether all requested quantum operation could be replayed.
        * @remark While a quantum operation can be added to the qc::QuantumComputation with qc::QuantumComputation::emplace_back(...), the required quantum gate annotations are not added to the annotatable quantum computation. Additionally, this function restricts the user to operations that can be simulated by syrec::SimpleSimulation (assuming the replayed operations were generated by addOperationsImplementingXGate calls).
        * @remark This function is not thread-safe. Additionally, the annotations of the replayed operations are not copied to the newly created operations.
        */
        [[nodiscard]] bool replayOperationsAtGivenIndexRange(std::size_t indexOfFirstQuantumOperationToReplayInQuantumComputation, std::size_t indexOfLastQuantumOperationToReplayInQuantumComputation);

        /**
         * Get the annotations of a quantum operation at a given index in the quantum computation.
         * @param indexOfQuantumOperationInQuantumComputation The index to the quantum operation whose annotations shall be fetched in the quantum computation.
         * @return A lookup of the fetched annotations. If the index did not reference an operation in the quantum computation then an empty lookup is returned.
         */
        [[nodiscard]] QuantumOperationAnnotationsLookup getAnnotationsOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation) const;

        /**
         * Determine the quantum cost to synthesis the given quantum computation.
         * @return The quantum cost for the synthesis of the quantum computation.
         */
        [[nodiscard]] SynthesisCostMetricValue getQuantumCostForSynthesis() const;

        /**
         * Determine the transistor cost to synthesis the given quantum computation.
         * @return The transistor cost for the synthesis of the quantum computation.
         */
        [[nodiscard]] SynthesisCostMetricValue getTransistorCostForSynthesis() const;

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
         * @return If the generation of quantum gate annotations is enabled, returns whether an existing global annotation was updated. Otherwise, false is returned.
         * @remark The generation of quantum gate annotations needs to be explicitly be enabled in the constructor of the annotatable quantum computation.
         */
        [[maybe_unused]] bool setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value);

        /**
         * Remove a global gate annotation. Existing annotations of the gates of the circuit are not modified.
         * @param key The key of the global gate annotation to be removed.
         * @return If the generation of quantum gate annotations is enabled, returns whether a global gate annotation was removed. Otherwise, false is returned.
         * @remark The generation of quantum gate annotations needs to be explicitly be enabled in the constructor of the annotatable quantum computation.
         */
        [[maybe_unused]] bool removeGlobalQuantumOperationAnnotation(const std::string_view& key);

        /**
         * Set a key value annotation for a quantum operation.
         * @param indexOfQuantumOperationInQuantumComputation The index of the quantum operation in the quantum computation.
         * @param annotationKey The key of the quantum operation annotation.
         * @param annotationValue The value of the quantum operation annotation.
         * @return If the generation of quantum gate annotations is enabled, returns whether an operation at the user-provided index existed in the quantum computation. Otherwise, false is returned.
         * @remark The generation of quantum gate annotations needs to be explicitly be enabled in the constructor of the annotatable quantum computation.
         */
        [[maybe_unused]] bool setOrUpdateAnnotationOfQuantumOperation(std::size_t indexOfQuantumOperationInQuantumComputation, const std::string_view& annotationKey, const std::string& annotationValue);

        /**
         * Get the inlined qubit information.
         * @param qubit The qubit whose inline information shall be fetched.
         * @return The inline information of the qubit if such information exists, otherwise std::nullopt is returned.
         */
        [[nodiscard]] std::optional<InlinedQubitInformation> getInlinedQubitInformation(qc::Qubit qubit) const;

    protected:
        /**
         * Annotate all quantum operations in the range [\p fromQuantumOperationIndex, \p toQuantumOperationIndex] with the union of the \p userProvidedAnnotationsPerQuantumOperation and the currently activate global quantum operation annotations.
         * @param fromQuantumOperationIndex The index of the first quantum operation in the quantum computation to annotate.
         * @param toQuantumOperationIndex The index of the last quantum operation in the quantum computation to annotate.
         * @param userProvidedAnnotationsPerQuantumOperation A set of unique quantum gate annotation key-value pairs.
         * @return If the generation of quantum gate annotations is enabled, returns whether the index range [\p fromQuantumOperationIndex, \p toQuantumOperationIndex] was within the range of quantum operations of the quantum computation. Otherwise, false is returned.
         * @remark Already existing quantum gate annotations that have a matching entry in the union of the \p userProvidedAnnotationsPerQuantumOperation and the active global quantum operation annotations will be updated, new entries are simply inserted.
         * @remark The currently active global quantum operation annotations are processed before the user provided ones.
         */
        [[maybe_unused]] bool annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const QuantumOperationAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation);
        [[nodiscard]] bool    isQubitWithinRange(qc::Qubit qubit) const noexcept;

        /**
         * Check whether a qubit index range is the immediate successor for the covered qubit index range of the last added quantum register.
         * @param toBeCheckedQubitIndexRange The qubit index range to check.
         * @return Whether the first qubit index of \p toBeCheckedQubitIndexRange is equal to the last qubit index + 1 of the last added quantum register.
         */
        [[nodiscard]] bool isQubitIndexRangeImmediateSuccessorOfCoveredRangeOfLastAddedQuantumRegister(const QubitIndexRange& toBeCheckedQubitIndexRange) const noexcept;

        std::unordered_set<qc::Qubit>                    aggregateOfPropagatedControlQubits;
        std::vector<std::unordered_map<qc::Qubit, bool>> controlQubitPropagationScopes;
        bool                                             canQubitsBeAddedToQuantumComputation = true;
        bool                                             generateQuantumOperationAnnotations  = false;

        QuantumOperationAnnotationsLookup activateGlobalQuantumOperationAnnotations;

        // We are assuming that no operations in the qc::QuantumComputation are removed (i.e. by applying qc::CircuitOptimizer) and will thus use the index of the quantum operation
        // as the search key in the container storing the annotations per quantum operation.
        std::vector<QuantumOperationAnnotationsLookup> annotationsPerQuantumOperation;

        /**
         * A container to store layout information for a quantum register.
         */
        struct BaseQuantumRegisterVariableLayout {
            /**
             * Information about a qubit in the variable layout of a quantum register.
             */
            struct QubitInVariableLayoutData {
                /**
                 * The required value per dimension to access the element storing the associated qubit in the variable layout of the quantum register.
                 */
                std::vector<unsigned> accessedValuePerDimensionOfElementStoringQubit;
                /**
                 * The relative index to access the qubit in the element storing the associated qubit in the variable layout of the quantum register.
                 */
                qc::Qubit relativeQubitIndexInElementStoringQubit;
                /**
                 * The optional inline qubit information about the qubit storing in the quantum register
                 */
                std::optional<InlinedQubitInformation> inlinedQubitInformation;
            };

            /**
             * Store basic variable layout information of a quantum register as well as the quantum registers label.
             * @param storedQubitIndices The stored qubit range of the quantum register.
             * @param quantumRegisterLabel The label of the quantum register.
             */
            BaseQuantumRegisterVariableLayout(const QubitIndexRange storedQubitIndices, std::string quantumRegisterLabel):
                storedQubitIndices(storedQubitIndices), quantumRegisterLabel(std::move(quantumRegisterLabel)) {}

            virtual ~BaseQuantumRegisterVariableLayout() = default;
            // Prevent object slicing when trying to copy assign or copy construct base class object from derived class object.
            // (see C++ core guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c67-a-polymorphic-class-should-suppress-public-copymove and https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es63-dont-slice)
            // Additionally, explicitly define remaining rule of 5 member functions (destructor, copy constructor, copy assignment operator, move constructor, and move assignment operator.)
            BaseQuantumRegisterVariableLayout(const BaseQuantumRegisterVariableLayout&)            = delete;
            BaseQuantumRegisterVariableLayout& operator=(const BaseQuantumRegisterVariableLayout&) = delete;
            BaseQuantumRegisterVariableLayout(BaseQuantumRegisterVariableLayout&&)                 = default;
            BaseQuantumRegisterVariableLayout& operator=(BaseQuantumRegisterVariableLayout&&)      = default;

            /**
             * Determine various information about a given qubit in the variable layout of the quantum register.
             * @param qubit The qubit whose quantum register variable layout information should be determined.
             * @return Information about the qubit in the variable layout of the quantum register, otherwise std::nullopt.
             */
            [[nodiscard]] virtual std::optional<QubitInVariableLayoutData> determineQubitInVariableLayoutData(qc::Qubit qubit) const = 0;

            /**
             * Determine the number of qubits of the quantum register.
             * @return The number of qubits stored in the quantum register
             */
            [[nodiscard]] unsigned getNumberOfQubitsInQuantumRegister() const { return storedQubitIndices.lastQubitIndex - storedQubitIndices.firstQubitIndex + 1U; }

            QubitIndexRange storedQubitIndices;
            std::string     quantumRegisterLabel;
        };

        /**
         * A container for the layout of a syrec::Variable in a quantum register with the qubits of the latter assumed to not be ancillary qubits.
         */
        struct NonAncillaryQuantumRegisterVariableLayout final: BaseQuantumRegisterVariableLayout {
            /**
             * Determine various information about a given qubit in the variable layout of the quantum register.
             * @param qubit The qubit whose quantum register variable layout information should be determined.
             * @return Information about the qubit in the variable layout of the quantum register, otherwise std::nullopt.
             */
            [[nodiscard]] std::optional<QubitInVariableLayoutData> determineQubitInVariableLayoutData(qc::Qubit qubit) const override;

            /**
             * Determine the required accessed value per dimension in the variable layout to access the element in the syrec::Variable that contains the \p qubit.
             * @param qubit The qubit for which the accessed value per dimension should be determined.
             * @return The required accessed value per dimension to access the element storing the qubit in the variable layout, otherwise std::nullopt.
             */
            [[nodiscard]] std::optional<std::vector<unsigned>> getRequiredValuesPerDimensionToAccessQubitOfVariable(qc::Qubit qubit) const;

            /**
             * Create a new variable layout for a non-ancillary quantum register storing the qubits of a syrec::Variable.
             * @param coveredQubitIndicesOfQuantumRegister The covered qubit index range of the ancillary quantum register. The first qubit index is assumed to be less than or equal to the last qubit index.
             * @param quantumRegisterLabel The label of the ancillary quantum register. Must not be empty.
             * @param numValuesPerDimensionOfVariable The number of values per dimension of the associated syrec::Variable.
             * @param qubitSizeOfElementInVariable The bitwidth of every element in the syrec::Variable.
             * @param optionalSharedInlinedQubitInformation The optional inline qubit information shared by all qubits of the quantum register.
             */
            NonAncillaryQuantumRegisterVariableLayout(QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const std::vector<unsigned>& numValuesPerDimensionOfVariable, unsigned qubitSizeOfElementInVariable, const std::optional<InlinedQubitInformation>& optionalSharedInlinedQubitInformation);

            unsigned                               elementQubitSize;
            std::vector<unsigned>                  numValuesPerDimensionOfVariable;
            std::vector<unsigned>                  offsetToNextElementInDimensionMeasuredInNumberOfVariableBitwidths;
            std::optional<InlinedQubitInformation> optionalSharedInlinedQubitInformation;
        };

        /**
         * A container for the layout of an ancillary quantum register.
         */
        struct AncillaryQuantumRegisterVariableLayout final: BaseQuantumRegisterVariableLayout {
            /**
             * Determine various information about a given qubit in the variable layout of the quantum register.
             * @param qubit The qubit whose quantum register variable layout information should be determined.
             * @return Information about the qubit in the variable layout of the quantum register, otherwise std::nullopt.
             */
            [[nodiscard]] std::optional<QubitInVariableLayoutData> determineQubitInVariableLayoutData(qc::Qubit qubit) const override;

            /**
             * Append a qubit index range to the ancillary quantum register
             * @param qubitIndexRange The qubit index range to append. The first qubit index must be less than or equal to its last qubit index while the first qubit index must be the next qubit after the current last qubit in the ancillary quantum register.
             * @param sharedInlinedQubitInformation The inline qubit information of the to be added qubits.
             * @return Whether the qubit index range could be appended.
             */
            [[nodiscard]] bool appendQubitRange(QubitIndexRange qubitIndexRange, const InlinedQubitInformation& sharedInlinedQubitInformation);

            /**
             * Create a new variable layout for a ancillary quantum register.
             * @param coveredQubitIndicesOfQuantumRegister The covered qubit index range of the ancillary quantum register. The first qubit index is assumed to be less than or equal to the last qubit index.
             * @param quantumRegisterLabel The label of the ancillary quantum register. Must not be empty.
             * @param sharedInlinedQubitInformation The inline qubit information of the associated qubit index range.
             */
            AncillaryQuantumRegisterVariableLayout(QubitIndexRange coveredQubitIndicesOfQuantumRegister, const std::string& quantumRegisterLabel, const InlinedQubitInformation& sharedInlinedQubitInformation);

            struct SharedQubitRangeInlineInformation {
                QubitIndexRange         coveredQubitIndexRange;
                InlinedQubitInformation inlinedQubitInformation;

                SharedQubitRangeInlineInformation(const QubitIndexRange coveredQubitIndexRange, InlinedQubitInformation inlinedQubitInformation):
                    coveredQubitIndexRange(coveredQubitIndexRange), inlinedQubitInformation(std::move(inlinedQubitInformation)) {}
            };

            /**
             * A collection of shared inlined qubit information for each of the shared qubit ranges in the quantum register. The collection is assumed to be sorted in ascending order according to the value of the first qubit of each qubit range.
             * No gaps between the stored qubit index ranges is allowed to exist.
             */
            std::vector<SharedQubitRangeInlineInformation> sharedQubitRangeInlineInformationLookup;
        };

        /**
         * Determine which quantum register in the quantum computation contains the given qubit.
         * @param qubit The qubit whose associated quantum register shall be determined.
         * @return The index of the quantum register containing the qubit, std::nullopt if no such quantum register is found.
         */
        [[nodiscard]] std::optional<std::size_t> determineIndexOfQuantumRegisterStoringQubit(qc::Qubit qubit) const;

        /**
         * Build the label of a qubit in the format of a stringified syrec::VariableAccess.
         * @param quantumRegisterLabel The label of the quantum register storing the qubit.
         * @param accessedValuePerDimension The required value per dimension to access the qubit in the associated quantum registers variable layout.
         * @param relativeQubitIndexInElement The relative index in the element containing the qubit in the associated quantum register variable layout (the element is assumed to be accessed using the \p accessedValuePerDimension dimension access).
         * @return The build qubit label (e.g. a[0][2].2)
         */
        [[nodiscard]] static std::string buildQubitLabelForQubitOfVariableInQuantumRegister(const std::string& quantumRegisterLabel, const std::vector<unsigned>& accessedValuePerDimension, std::size_t relativeQubitIndexInElement);

        /**
         * An ordered collection storing the variable layout stored in each quantum register.
         * Said quantum registers are assumed to be sorted according to the index of their first qubit in the quantum computation in ascending order. Additionally, no gaps are allowed to exist between the stored qubits of the quantum registers.
         */
        std::vector<std::unique_ptr<BaseQuantumRegisterVariableLayout>> quantumRegisterAssociatedVariableLayouts;
    };
} // namespace syrec
