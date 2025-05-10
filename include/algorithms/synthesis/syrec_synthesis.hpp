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

#include "core/circuit.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <map>
#include <stack>
#include <string>
#include <vector>

namespace syrec {
    class SyrecSynthesis {
    public:
        std::stack<unsigned>               expOpp;
        std::stack<std::vector<unsigned>>  expLhss;
        std::stack<std::vector<unsigned>>  expRhss;
        bool                               subFlag = false;
        std::vector<unsigned>              opVec;
        std::vector<unsigned>              assignOpVector;
        std::vector<unsigned>              expOpVector;
        std::vector<std::vector<unsigned>> expLhsVector;
        std::vector<std::vector<unsigned>> expRhsVector;

        using VarLinesMap = std::map<Variable::ptr, unsigned int>;

        explicit SyrecSynthesis(Circuit& circ);
        virtual ~SyrecSynthesis() = default;

        void addVariables(Circuit& circ, const Variable::vec& variables);
        void setMainModule(const Module::ptr& mainModule);

    protected:
        virtual bool processStatement(Circuit& circuit, const Statement::ptr& statement) = 0;

        virtual bool onModule(Circuit& circuit, const Module::ptr&);

        virtual bool opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<unsigned>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<unsigned>& v);
        virtual bool opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<unsigned>& v);

        virtual bool              onStatement(Circuit& circuit, const Statement::ptr& statement);
        virtual bool              onStatement(Circuit& circuit, const AssignStatement& statement);
        virtual bool              onStatement(Circuit& circuit, const IfStatement& statement);
        virtual bool              onStatement(Circuit& circuit, const ForStatement& statement);
        virtual bool              onStatement(Circuit& circuit, const CallStatement& statement);
        virtual bool              onStatement(Circuit& circuit, const UncallStatement& statement);
        bool                      onStatement(Circuit& circuit, const SwapStatement& statement);
        bool                      onStatement(Circuit& circuit, const UnaryStatement& statement);
        [[nodiscard]] static bool onStatement(const SkipStatement& statement);

        virtual bool assignAdd(Circuit& circuit, std::vector<unsigned>& lhs, std::vector<unsigned>& rhs, [[maybe_unused]] const unsigned& op)      = 0;
        virtual bool assignSubtract(Circuit& circuit, std::vector<unsigned>& lhs, std::vector<unsigned>& rhs, [[maybe_unused]] const unsigned& op) = 0;
        virtual bool assignExor(Circuit& circuit, std::vector<unsigned>& lhs, std::vector<unsigned>& rhs, [[maybe_unused]] const unsigned& op)     = 0;

        virtual bool onExpression(Circuit& circuit, const Expression::ptr& expression, std::vector<unsigned>& lines, std::vector<unsigned> const& lhsStat, unsigned op);
        virtual bool onExpression(Circuit& circuit, const BinaryExpression& expression, std::vector<unsigned>& lines, std::vector<unsigned> const& lhsStat, unsigned op);
        virtual bool onExpression(Circuit& circuit, const ShiftExpression& expression, std::vector<unsigned>& lines, std::vector<unsigned> const& lhsStat, unsigned op);
        virtual bool onExpression(Circuit& circuit, const NumericExpression& expression, std::vector<unsigned>& lines);
        virtual bool onExpression(const VariableExpression& expression, std::vector<unsigned>& lines);

        virtual bool expAdd(Circuit& circuit, [[maybe_unused]] const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs)      = 0;
        virtual bool expSubtract(Circuit& circuit, [[maybe_unused]] const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) = 0;
        virtual bool expExor(Circuit& circuit, [[maybe_unused]] const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs)     = 0;

        // BEGIN: Add circuit parameter to functions

        // unary operations
        static bool bitwiseNegation(Circuit& circuit, const std::vector<unsigned>& dest); // ~
        static bool decrement(Circuit& circuit, const std::vector<unsigned>& dest);       // --
        static bool increment(Circuit& circuit, const std::vector<unsigned>& dest);       // ++

        // binary operations
        static bool  bitwiseAnd(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2); // &
        static bool  bitwiseCnot(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src);                                    // ^=
        static bool  bitwiseOr(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2);  // &
        static bool  conjunction(Circuit& circuit, unsigned dest, unsigned src1, unsigned src2);                                                            // &&// -=
        static bool  decreaseWithCarry(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src, unsigned carry);
        static bool  disjunction(Circuit& circuit, unsigned dest, unsigned src1, unsigned src2);                                                          // ||
        static bool  division(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2); // /
        static bool  equals(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2);                       // =
        static bool  greaterEquals(Circuit& circuit, unsigned dest, const std::vector<unsigned>& srcTwo, const std::vector<unsigned>& srcOne);            // >
        static bool  greaterThan(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src2, const std::vector<unsigned>& src1);                  // >// +=
        static bool  increaseWithCarry(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src, unsigned carry);
        static bool  lessEquals(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src2, const std::vector<unsigned>& src1);                         // <=
        static bool  lessThan(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2);                           // <
        static bool  modulo(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2);         // %
        static bool  multiplication(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2); // *
        static bool  notEquals(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2);                          // !=
        static bool  swap(Circuit& circuit, const std::vector<unsigned>& dest1, const std::vector<unsigned>& dest2);                                            // NOLINT(cppcoreguidelines-noexcept-swap, performance-noexcept-swap) <=>
        static bool  decrease(Circuit& circuit, const std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs);
        static bool  increase(Circuit& circuit, const std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs);
        virtual bool expressionOpInverse([[maybe_unused]] Circuit& circuit, [[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<unsigned>& expLhs, [[maybe_unused]] const std::vector<unsigned>& expRhs) const;
        bool         checkRepeats();

        // shift operations
        static bool leftShift(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, unsigned src2);  // <<
        static bool rightShift(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, unsigned src2); // >>

        static void addVariable(Circuit& circ, const std::vector<unsigned>& dimensions, const Variable::ptr& var, constant constant, bool garbage, const std::string& arraystr);
        void        getVariables(const VariableAccess::ptr& var, std::vector<unsigned>& lines);

        unsigned getConstantLine(Circuit& circuit, bool value);
        void     getConstantLines(Circuit& circuit, unsigned bitwidth, unsigned value, std::vector<unsigned>& lines);

        static bool synthesize(SyrecSynthesis* synthesizer, Circuit& circ, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics);

        std::stack<Statement::ptr>    stmts;
        Circuit&                      circ; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        Number::loop_variable_mapping loopMap;
        std::stack<Module::ptr>       modules;

    private:
        VarLinesMap                           varLines;
        std::map<bool, std::vector<unsigned>> freeConstLinesMap;
    };

} // namespace syrec
