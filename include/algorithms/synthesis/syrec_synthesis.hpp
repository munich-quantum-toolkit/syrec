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

#include "algorithms/synthesis/first_variable_qubit_offset_lookup.hpp"
#include "algorithms/synthesis/statement_execution_order_stack.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/qubit_inlining_stack.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "ir/Definitions.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace syrec {
    class SyrecSynthesis {
    public:
        inline static const std::string MAIN_MODULE_IDENTIFIER_CONFIG_KEY            = "main_module";
        inline static const std::string GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY = "create_qubit_inline_debug_information";

        std::stack<BinaryExpression::BinaryOperation>  expOpp;
        std::stack<std::vector<unsigned>>              expLhss;
        std::stack<std::vector<unsigned>>              expRhss;
        bool                                           subFlag = false;
        std::vector<BinaryExpression::BinaryOperation> opVec;
        std::vector<AssignStatement::AssignOperation>  assignOpVector;
        std::vector<BinaryExpression::BinaryOperation> expOpVector;
        std::vector<std::vector<unsigned>>             expLhsVector;
        std::vector<std::vector<unsigned>>             expRhsVector;

        explicit SyrecSynthesis(AnnotatableQuantumComputation& annotatableQuantumComputation);
        virtual ~SyrecSynthesis() = default;

        [[nodiscard]] bool addVariables(const Variable::vec& variables) const;
        void               setMainModule(const Module::ptr& mainModule);

        [[maybe_unused]] static bool synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics);

    protected:
        constexpr static std::string_view GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER = "lno";
        using OperationVariant                                                                 = std::variant<AssignStatement::AssignOperation, BinaryExpression::BinaryOperation, ShiftExpression::ShiftOperation, UnaryExpression::UnaryOperation>;

        virtual bool processStatement(const Statement::ptr& statement) = 0;
        virtual bool onModule(const Module::ptr&);

        virtual bool opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);

        virtual bool onStatement(const Statement::ptr& statement);
        virtual bool onStatement(const AssignStatement& statement);
        virtual bool onStatement(const IfStatement& statement);
        virtual bool onStatement(const ForStatement& statement);
        virtual bool onStatement(const CallStatement& statement);
        virtual bool onStatement(const UncallStatement& statement);
        bool         onStatement(const SwapStatement& statement);
        bool         onStatement(const UnaryStatement& statement);
        virtual bool onStatement(const SkipStatement& statement);

        virtual bool assignAdd(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation)      = 0;
        virtual bool assignSubtract(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) = 0;
        virtual bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation)     = 0;

        virtual bool onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);
        virtual bool onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);
        virtual bool onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);
        virtual bool onExpression(const NumericExpression& expression, std::vector<qc::Qubit>& lines);
        virtual bool onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines);
        virtual bool onExpression(const UnaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);

        virtual bool expAdd([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)      = 0;
        virtual bool expSubtract([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) = 0;
        virtual bool expExor([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)     = 0;

        // unary operations
        static bool bitwiseNegation(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest); // ~
        static bool decrement(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest);       // --
        static bool increment(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest);       // ++

        // binary operations
        static bool bitwiseAnd(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // &
        static bool bitwiseCnot(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src);                                     // ^=
        static bool bitwiseOr(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);  // &
        static bool conjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                            // &&// -=
        static bool decreaseWithCarry(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        static bool disjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                                                                              // ||
        static bool division(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dividend, const std::vector<qc::Qubit>& divisor, const std::vector<qc::Qubit>& quotient, const std::vector<qc::Qubit>& remainder); // /
        static bool equals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                                                                           // =
        static bool greaterEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne);                                                                // >
        static bool greaterThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                                                                      // >// +=
        static bool lessEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                                                                       // <=
        static bool lessThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                                                                         // <
        static bool modulo(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dividend, const std::vector<qc::Qubit>& divisor, const std::vector<qc::Qubit>& quotient, const std::vector<qc::Qubit>& remainder);   // %
        static bool multiplication(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                                               // *
        static bool notEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                                                                        // !=
        static bool swap(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2);
        /**
         * Synthesizes the subtraction \p lhs - \p rhs and stores the result in the qubits of the rhs operand.
         * @param annotatableQuantumComputation The annotatable quantum computation to which the generated gates are added.
         * @param lhs The left hand side operand of the subtraction.
         * @param rhs The right hand side operand of the subtraction.
         * @return Whether the subtraction could be synthesized (i.e. no overlapping qubits and qubit length difference between the operands and whether all required gates could be added to the \p annotatableQuantumComputation).
         */
        static bool inplaceSubtract(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs);
        /**
         * Synthesizes the addition \p lhs + \p rhs and stores the result in the qubits of the rhs operand.
         * @param annotatableQuantumComputation The annotatable quantum computation to which the generated gates are added.
         * @param lhs The left hand side operand of the addition.
         * @param rhs The right hand side operand of the addition.
         * @param optionalCarryOut Optionally pass the qubit that will store the output carry of the addition.
         * @return Whether the addition could be synthesized (i.e. no overlapping qubits and qubit length difference between the operands and whether all required gates could be added to the \p annotatableQuantumComputation).
         */
        static bool  inplaceAdd(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs, const std::optional<qc::Qubit>& optionalCarryOut = std::nullopt);
        virtual bool expressionOpInverse([[maybe_unused]] BinaryExpression::BinaryOperation binaryOperation, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs);
        bool         checkRepeats();

        // shift operations
        static bool leftShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& toBeShiftedQubits, unsigned qubitIndexShiftAmount);  // <<
        static bool rightShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& toBeShiftedQubits, unsigned qubitIndexShiftAmount); // >>

        [[nodiscard]] static std::optional<qc::Qubit> addVariable(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr, const std::optional<QubitInliningStack::ptr>& currentModuleCallStack);

        /**
         * Get the qubits accessed by the defined variable access.
         * @param variableAccess The variable access to evaluate.
         * @param lines The container storing the qubits determined to be accessed by \p variableAccess
         * @return Whether the accessed qubits could be determine.
         * @remark If the variableAccess contains only compile time constant expressions (CTCE) then the container will store the accessed qubits without generating ancillary qubits. However, if CTCEs were defined then ancillary qubits
         * are needed to calculate the index of the accessed element in the variable using the defined dimension access of the variableAccess with a conditional COPY of the accessed qubits to the ancillary qubits. If one needs to operate
         * on the actually accessed qubits then the combination of syrec::SyrecSynthesis::calculateSymbolicUnrolledIndexForElementInVariable and syrec::SyrecSynthesis::transferQubitsOfElementAtIndexInVariableToOtherQubits needs to be used with the
         * latter using syrec::QubitTransferOperation::SwapQubits to transfer the qubits instead of a copy.
         */
        [[nodiscard]] bool getVariables(const VariableAccess::ptr& variableAccess, std::vector<qc::Qubit>& lines);

        [[nodiscard]] std::optional<qc::Qubit> getConstantLine(bool value, const std::optional<QubitInliningStack::ptr>& inlinedQubitModuleCallStack);
        [[nodiscard]] bool                     getConstantLines(unsigned bitwidth, unsigned value, std::vector<qc::Qubit>& lines);

        [[nodiscard]] static std::optional<AssignStatement::AssignOperation>  tryMapBinaryToAssignmentOperation(BinaryExpression::BinaryOperation binaryOperation) noexcept;
        [[nodiscard]] static std::optional<BinaryExpression::BinaryOperation> tryMapAssignmentToBinaryOperation(AssignStatement::AssignOperation assignOperation) noexcept;
        [[nodiscard]] std::optional<QubitInliningStack::ptr>                  getLastCreatedModuleCallStackInstance() const;
        [[nodiscard]] std::optional<QubitInliningStack::ptr>                  createInsertAndGetCopyOfLastCreatedCallStackInstance();
        [[nodiscard]] bool                                                    shouldQubitInlineInformationBeRecorded() const;
        void                                                                  discardLastCreateModuleCallStackInstance();

        struct EvaluatedBitrangeAccess {
            unsigned bitrangeStart;
            unsigned bitrangeEnd;

            [[nodiscard]] std::vector<unsigned> getIndicesOfAccessedBits() const;
        };

        struct EvaluatedDimensionAccess {
            bool                                 containedOnlyNumericExpressions;
            std::vector<std::optional<unsigned>> accessedValuePerDimension;
        };

        struct EvaluatedVariableAccess {
            qc::Qubit                        offsetToFirstQubitOfVariable;
            std::reference_wrapper<Variable> accessedVariable;
            EvaluatedBitrangeAccess          evaluatedBitrangeAccess;
            EvaluatedDimensionAccess         evaluatedDimensionAccess;
            Expression::vec                  userDefinedDimensionAccess;
        };

        enum class QubitTransferOperation : std::uint8_t {
            CopyValue,
            SwapQubits
        };

        [[nodiscard]] bool synthesizeModuleCall(const std::variant<const CallStatement*, const UncallStatement*>& callStmtVariant);

        /**
         * Evaluate and validate the value of the indices evaluable at compile time defined in the bitrange component of a variable access.
         * @param userDefinedVariableAccess The variable access to evaluate.
         * @param loopVariableValueLookup A lookup for loop variable values.
         * @return A container storing the value of the indices of the bitrange if the evaluation was possible (value for all loop variables known, etc.), otherwise std::nullopt is returned.
         */
        [[nodiscard]] static std::optional<EvaluatedBitrangeAccess> evaluateAndValidateBitrangeAccess(const VariableAccess& userDefinedVariableAccess, const Number::LoopVariableMapping& loopVariableValueLookup);

        /**
         * Evaluate and validate the value of the indices evaluable at compile time defined in the dimension access of a variable access.
         * @param userDefinedVariableAccess The variable access to validate, accessed variable must not be null.
         * @param loopVariableValueLookup A lookup containing the current value of the activate loop variables.
         * @return A container storing the evaluated values of each dimension, if the number of accessed dimensions is equal to the number of defined dimensions of the accessed variable and if all numeric expressions in the dimension access could be evaluated and defined a value within the range [0, number of values in dimension at same index in accessed variable - 1]. If the validation failed, std::nullopt is returned.
         * @remark Note that only numeric expressions are evaluated while all other expressions types are ignored. A flag in the returned container can be used to distinguish between the two cases.
         * @remark No arithmetic or logical simplifications are performed at the moment which could enable the evaluation of other expression types at compile time.
         */
        [[nodiscard]] static std::optional<EvaluatedDimensionAccess> evaluateAndValidateDimensionAccess(const VariableAccess& userDefinedVariableAccess, const Number::LoopVariableMapping& loopVariableValueLookup);

        /**
         * Determine and validate compile time information for a given syrec::VariableAccess.
         * @param userDefinedVariableAccess The variable access to validate, accessed variable must not be null.
         * @param loopVariableValueLookup A lookup containing the current value of the activate loop variables.
         * @param firstVariableQubitOffsetLookup A lookup usable to determine the first qubit of every variable using its identifier.
         * @return A container storing information about the evaluated syrec::VariableAccess known at compile time including its bitrange as well as dimension access component. If the syrec::VariableAccess contained invalid indices (e.g. nullptr, loop variables for which no value could be determined) then std::nullopt is returned.
         */
        [[nodiscard]] static std::optional<EvaluatedVariableAccess> evaluateAndValidateVariableAccess(const VariableAccess::ptr& userDefinedVariableAccess, const Number::LoopVariableMapping& loopVariableValueLookup, const std::unique_ptr<FirstVariableQubitOffsetLookup>& firstVariableQubitOffsetLookup);

        /**
         * Determine the qubits accessed by a syrec::VariableAccess.
         * @param evaluatedVariableAccess The evaluated variable access to be used to determine the accessed qubits, all defined indices in the dimension access as well as bit range must be evaluable at compile time.
         * @param containerForAccessedQubits The container storing the accessed qubits that needs to be passed as an empty container to this function.
         * @return Whether one could determine the accessed qubits by the \p evaluatedVariableAccess.
         */
        [[nodiscard]] static bool getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(const EvaluatedVariableAccess& evaluatedVariableAccess, std::vector<qc::Qubit>& containerForAccessedQubits);

        /**
         * Determine the qubits accessed by a syrec::VariableAccess using a combination of compile-time as well as non-compile time constant indices.
         * @param evaluatedVariableAccess The evaluated variable access that is used to the determine the accessed qubits.
         * @param containerForAccessedQubits The container storing the accessed qubits that needs to be passed as an empty container to this function.
         * @return Whether one could determine the accessed qubits by the \p evaluatedVariableAccess as well as whether all required quantum operations could be added to the internal annotatable quantum operation.
         * @remark Note that the qubits of the accessed element of the syrec::VariableAccess are copied to ancillary qubits and returned in the \p containerForAccessedQubits and are not the qubits of the element itself.
         *         To operate on the qubits of the accessed element one needs repeat both the SyrecSynthesis::calculateSymbolicUnrolledIndexForElementInVariable(...) and SyrecSynthesis::transferQubitsOfElementAtIndexInVariableToOtherQubits with the latter using the QubitTransferOperation::SwapQubits instead of QubitTransferOperation::CopyValue.
         */
        [[nodiscard]] bool getQubitsForVariableAccessContainingIndicesNotEvaluableAtCompileTime(const EvaluatedVariableAccess& evaluatedVariableAccess, std::vector<qc::Qubit>& containerForAccessedQubits);

        /**
         * Calculate the index of the accessed value in the unrolled variable if the evaluated variable access contained a non-compile time constant expression in any of its accessed dimensions.
         * @param evaluatedVariableAccess The evaluated variable access whose accessed index should be calculated.
         * @param containerToStoreUnrolledIndex The container storing the qubits storing the calculated index. Must be passed as an empty container.
         * @return Whether the index of the accessed element in the provided variable access could be calculcated.
         * @remark Note that the value of the calculated index is not known at compile time, e.g. the unrolled index of the element 'a[1][2][1]' in 'a[2][4][3]' is equal to 19 (1*12 + 2*3 + 1)
         */
        [[nodiscard]] bool calculateSymbolicUnrolledIndexForElementInVariable(const EvaluatedVariableAccess& evaluatedVariableAccess, std::vector<qc::Qubit>& containerToStoreUnrolledIndex);

        /**
         * Transfer the qubits at index of the accessed value in the unrolled variable using one of the supported transfer operations.
         * @param evaluatedVariableAccess The variable access defining the accessed variable from which qubits shall be extracted.
         * @param qubitsStoringUnrolledIndexOfElementToSelect The qubits storing the index of the accessed element in the unrolled variable.
         * @param qubitsStoringResultOfTransferOperation The qubits storing the qubits of the accessed variable transferred with the specified transfer operation.
         * @param qubitTransferOperation The transfer operation applied to the accessed qubits of the variable to "move" them to qubits storing the result of the transfer operation.
         * @return Whether the qubits of the accessed element in the variable could be transferred to the result container.
         */
        [[nodiscard]] bool transferQubitsOfElementAtIndexInVariableToOtherQubits(const EvaluatedVariableAccess& evaluatedVariableAccess, const std::vector<qc::Qubit>& qubitsStoringUnrolledIndexOfElementToSelect, const std::vector<qc::Qubit>& qubitsStoringResultOfTransferOperation, QubitTransferOperation qubitTransferOperation);

        std::stack<Statement::ptr>  stmts;
        Number::LoopVariableMapping loopMap;
        std::stack<Module::ptr>     modules;

        AnnotatableQuantumComputation&                      annotatableQuantumComputation; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        std::optional<std::vector<QubitInliningStack::ptr>> moduleCallStackInstances;
        std::unique_ptr<StatementExecutionOrderStack>       statementExecutionOrderStack;
        std::unique_ptr<FirstVariableQubitOffsetLookup>     firstVariableQubitOffsetLookup;

    private:
        std::map<bool, std::vector<qc::Qubit>> freeConstLinesMap;
    };
} // namespace syrec
