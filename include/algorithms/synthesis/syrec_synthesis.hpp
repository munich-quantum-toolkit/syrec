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

#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "ir/Definitions.hpp"

#include <map>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace syrec {
    class SyrecSynthesis {
    public:
        std::stack<BinaryExpression::BinaryOperation>  expOpp;
        std::stack<std::vector<unsigned>>              expLhss;
        std::stack<std::vector<unsigned>>              expRhss;
        bool                                           subFlag = false;
        std::vector<BinaryExpression::BinaryOperation> opVec;
        std::vector<AssignStatement::AssignOperation>  assignOpVector;
        std::vector<BinaryExpression::BinaryOperation> expOpVector;
        std::vector<std::vector<unsigned>>             expLhsVector;
        std::vector<std::vector<unsigned>>             expRhsVector;

        using VarLinesMap = std::map<Variable::ptr, qc::Qubit>;

        explicit SyrecSynthesis(AnnotatableQuantumComputation& annotatableQuantumComputation);
        virtual ~SyrecSynthesis() = default;

        [[nodiscard]] bool addVariables(const Variable::vec& variables);
        void               setMainModule(const Module::ptr& mainModule);

        [[maybe_unused]] static bool synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics);

    protected:
        constexpr static std::string_view GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER = "lno";
        using OperationVariant                                                                 = std::variant<AssignStatement::AssignOperation, BinaryExpression::BinaryOperation, ShiftExpression::ShiftOperation>;

        virtual bool processStatement(const Statement::ptr& statement) = 0;
        virtual bool onModule(const Module::ptr&);

        virtual bool opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v);

        virtual bool              onStatement(const Statement::ptr& statement);
        virtual bool              onStatement(const AssignStatement& statement);
        virtual bool              onStatement(const IfStatement& statement);
        virtual bool              onStatement(const ForStatement& statement);
        virtual bool              onStatement(const CallStatement& statement);
        virtual bool              onStatement(const UncallStatement& statement);
        bool                      onStatement(const SwapStatement& statement);
        bool                      onStatement(const UnaryStatement& statement);
        [[nodiscard]] static bool onStatement(const SkipStatement& statement);

        virtual bool assignAdd(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation)      = 0;
        virtual bool assignSubtract(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) = 0;
        virtual bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation)     = 0;

        virtual bool onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);
        virtual bool onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);
        virtual bool onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, OperationVariant operationVariant);
        virtual bool onExpression(const NumericExpression& expression, std::vector<qc::Qubit>& lines);
        virtual bool onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines);

        virtual bool expAdd([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)      = 0;
        virtual bool expSubtract([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) = 0;
        virtual bool expExor([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)     = 0;

        // BEGIN: Add circuit parameter to functions

        // unary operations
        static bool bitwiseNegation(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest); // ~
        static bool decrement(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest);       // --
        static bool increment(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest);       // ++

        // binary operations
        static bool  bitwiseAnd(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // &
        static bool  bitwiseCnot(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src);                                     // ^=
        static bool  bitwiseOr(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);  // &
        static bool  conjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                            // &&// -=
        static bool  decreaseWithCarry(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        static bool  disjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                          // ||
        static bool  division(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // /
        static bool  equals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                       // =
        static bool  greaterEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne);            // >
        static bool  greaterThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                  // >// +=
        static bool  increaseWithCarry(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        static bool  lessEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                         // <=
        static bool  lessThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                           // <
        static bool  modulo(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);         // %
        static bool  multiplication(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // *
        static bool  notEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                          // !=
        static bool  swap(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2);                                             // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap) <=>
        static bool  decrease(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        static bool  increase(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        virtual bool expressionOpInverse([[maybe_unused]] BinaryExpression::BinaryOperation binaryOperation, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs);
        bool         checkRepeats();

        // shift operations
        static bool leftShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2);  // <<
        static bool rightShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2); // >>

        [[nodiscard]] static bool addVariable(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr);
        void                      getVariables(const VariableAccess::ptr& var, std::vector<qc::Qubit>& lines);

        [[nodiscard]] std::optional<qc::Qubit> getConstantLine(bool value);
        [[nodiscard]] bool                     getConstantLines(unsigned bitwidth, unsigned value, std::vector<qc::Qubit>& lines);

        [[nodiscard]] static std::optional<AssignStatement::AssignOperation>  tryMapBinaryToAssignmentOperation(BinaryExpression::BinaryOperation binaryOperation) noexcept;
        [[nodiscard]] static std::optional<BinaryExpression::BinaryOperation> tryMapAssignmentToBinaryOperation(AssignStatement::AssignOperation assignOperation) noexcept;

        std::stack<Statement::ptr>  stmts;
        Number::LoopVariableMapping loopMap;
        std::stack<Module::ptr>     modules;

        AnnotatableQuantumComputation& annotatableQuantumComputation; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    private:
        VarLinesMap                            varLines;
        std::map<bool, std::vector<qc::Qubit>> freeConstLinesMap;
    };

} // namespace syrec
