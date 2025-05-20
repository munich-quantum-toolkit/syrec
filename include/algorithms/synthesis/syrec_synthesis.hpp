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
#include "ir/QuantumComputation.hpp"

#include <map>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

namespace syrec {
    class SyrecSynthesis {
    public:
        std::stack<qc::Qubit>              expOpp;
        std::stack<std::vector<unsigned>>  expLhss;
        std::stack<std::vector<unsigned>>  expRhss;
        bool                               subFlag = false;
        std::vector<unsigned>              opVec;
        std::vector<unsigned>              assignOpVector;
        std::vector<unsigned>              expOpVector;
        std::vector<std::vector<unsigned>> expLhsVector;
        std::vector<std::vector<unsigned>> expRhsVector;

        using VarLinesMap = std::map<Variable::ptr, qc::Qubit>;

        explicit SyrecSynthesis(qc::QuantumComputation& quantumComputation);
        virtual ~SyrecSynthesis() = default;

        [[nodiscard]] bool addVariables(const Variable::vec& variables);
        void               setMainModule(const Module::ptr& mainModule);

        [[maybe_unused]] static bool synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics);

    protected:
        constexpr static std::string_view GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER = "lno";

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

        virtual bool assignAdd(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op)      = 0;
        virtual bool assignSubtract(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op) = 0;
        virtual bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op)     = 0;

        virtual bool onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op);
        virtual bool onExpression(const NumericExpression& expression, std::vector<qc::Qubit>& lines);
        virtual bool onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines);

        virtual bool expAdd([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)      = 0;
        virtual bool expSubtract([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) = 0;
        virtual bool expExor([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs)     = 0;

        // BEGIN: Add circuit parameter to functions

        // unary operations
        bool bitwiseNegation(const std::vector<qc::Qubit>& dest); // ~
        bool decrement(const std::vector<qc::Qubit>& dest);       // --
        bool increment(const std::vector<qc::Qubit>& dest);       // ++

        // binary operations
        bool         bitwiseAnd(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // &
        bool         bitwiseCnot(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src);                                     // ^=
        bool         bitwiseOr(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);  // &
        bool         conjunction(qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                            // &&// -=
        bool         decreaseWithCarry(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        bool         disjunction(qc::Qubit dest, qc::Qubit src1, qc::Qubit src2);                                                          // ||
        bool         division(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // /
        bool         equals(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                       // =
        bool         greaterEquals(qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne);            // >
        bool         greaterThan(qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                  // >// +=
        bool         increaseWithCarry(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry);
        bool         lessEquals(qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1);                         // <=
        bool         lessThan(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                           // <
        bool         modulo(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);         // %
        bool         multiplication(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2); // *
        bool         notEquals(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2);                          // !=
        bool         swap(const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2);                                             // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap) <=>
        bool         decrease(const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        bool         increase(const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);
        virtual bool expressionOpInverse([[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs);
        bool         checkRepeats();

        // shift operations
        bool leftShift(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2);  // <<
        bool rightShift(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2); // >>

        [[nodiscard]] bool addVariable(const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr);
        void               getVariables(const VariableAccess::ptr& var, std::vector<qc::Qubit>& lines);

        [[nodiscard]] std::optional<qc::Qubit> getConstantLine(bool value);
        [[nodiscard]] bool                     getConstantLines(unsigned bitwidth, unsigned value, std::vector<qc::Qubit>& lines);

        std::stack<Statement::ptr>    stmts;
        Number::loop_variable_mapping loopMap;
        std::stack<Module::ptr>       modules;

        AnnotatableQuantumComputation annotatableQuantumComputation;

    private:
        VarLinesMap                            varLines;
        std::map<bool, std::vector<qc::Qubit>> freeConstLinesMap;
    };

} // namespace syrec
