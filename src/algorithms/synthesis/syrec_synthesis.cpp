/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/syrec_synthesis.hpp"

#include "core/circuit.hpp"
#include "core/gate.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "core/utils/timer.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

namespace syrec {
    // Helper Functions for the synthesis methods
    SyrecSynthesis::SyrecSynthesis(Circuit& circ):
        circ(circ) {
        freeConstLinesMap.try_emplace(false /* emplacing a default constructed object */);
        freeConstLinesMap.try_emplace(true /* emplacing a default constructed object */);
    }

    void SyrecSynthesis::setMainModule(const Module::ptr& mainModule) {
        assert(modules.empty());
        modules.push(mainModule);
    }

    bool SyrecSynthesis::onModule(Circuit& circuit, const Module::ptr& main) {
        bool              synthesisOfModuleStatementOk = true;
        const std::size_t nModuleStatements            = main->statements.size();
        for (std::size_t i = 0; i < nModuleStatements && synthesisOfModuleStatementOk; ++i) {
            synthesisOfModuleStatementOk = processStatement(circuit, main->statements[i]);
        }
        return synthesisOfModuleStatementOk;
    }

    /// If the input signals are repeated (i.e., rhs input signals are repeated)
    bool SyrecSynthesis::checkRepeats() {
        std::vector checkLhsVec(expLhsVector.cbegin(), expLhsVector.cend());
        std::vector checkRhsVec(expRhsVector.cbegin(), expRhsVector.cend());

        checkLhsVec.erase(std::remove_if(checkLhsVec.begin(), checkLhsVec.end(), [](const std::vector<unsigned>& linesContainer) { return linesContainer.empty(); }), checkLhsVec.end());
        checkRhsVec.erase(std::remove_if(checkRhsVec.begin(), checkRhsVec.end(), [](const std::vector<unsigned>& linesContainer) { return linesContainer.empty(); }), checkRhsVec.end());

        bool foundRepeat = false;
        for (std::size_t i = 0; i < checkRhsVec.size() && !foundRepeat; ++i) {
            for (std::size_t j = 0; j < checkRhsVec.size() && !foundRepeat; ++j) {
                foundRepeat = i != j && checkRhsVec[i] == checkRhsVec[j];
            }
        }

        for (std::size_t i = 0; i < checkLhsVec.size() && !foundRepeat; ++i) {
            foundRepeat = checkLhsVec[i] == checkRhsVec[i];
        }

        expOpVector.clear();
        expLhsVector.clear();
        expRhsVector.clear();
        return foundRepeat;
    }

    bool SyrecSynthesis::opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<unsigned>& v) {
        return true;
    }
    bool SyrecSynthesis::opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<unsigned>& v) {
        return true;
    }
    bool SyrecSynthesis::opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<unsigned>& v) {
        return true;
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const Statement::ptr& statement) {
        stmts.push(statement);

        // To be able to associate which gates are associated with a statement in the syrec-editor we need to set the appropriate annotation that will be added for each created gate
        circuit.setOrUpdateGlobalGateAnnotation(Circuit::GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

        bool okay = true;
        if (auto const* swapStat = dynamic_cast<SwapStatement*>(statement.get())) {
            okay = onStatement(circuit, *swapStat);
        } else if (auto const* unaryStat = dynamic_cast<UnaryStatement*>(statement.get())) {
            okay = onStatement(circuit, *unaryStat);
        } else if (auto const* assignStat = dynamic_cast<AssignStatement*>(statement.get())) {
            okay = onStatement(circuit, *assignStat);
        } else if (auto const* ifStat = dynamic_cast<IfStatement*>(statement.get())) {
            okay = onStatement(circuit, *ifStat);
        } else if (auto const* forStat = dynamic_cast<ForStatement*>(statement.get())) {
            okay = onStatement(circuit, *forStat);
        } else if (auto const* callStat = dynamic_cast<CallStatement*>(statement.get())) {
            okay = onStatement(circuit, *callStat);
        } else if (auto const* uncallStat = dynamic_cast<UncallStatement*>(statement.get())) {
            okay = onStatement(circuit, *uncallStat);
        } else if (auto const* skipStat = statement.get()) {
            okay = onStatement(*skipStat);
        } else {
            okay = false;
        }

        stmts.pop();
        return okay;
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const SwapStatement& statement) {
        std::vector<unsigned> lhs;
        std::vector<unsigned> rhs;

        getVariables(statement.lhs, lhs);
        getVariables(statement.rhs, rhs);

        assert(lhs.size() == rhs.size());

        return swap(circuit, lhs, rhs);
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const UnaryStatement& statement) {
        // load variable
        std::vector<unsigned> var;
        getVariables(statement.var, var);

        switch (statement.op) {
            case UnaryStatement::Invert:
                return bitwiseNegation(circuit, var);
            case UnaryStatement::Increment:
                return increment(circuit, var);
            case UnaryStatement::Decrement:
                return decrement(circuit, var);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const AssignStatement& statement) {
        std::vector<unsigned> lhs;
        std::vector<unsigned> rhs;
        std::vector<unsigned> d;

        getVariables(statement.lhs, lhs);
        opRhsLhsExpression(statement.rhs, d);

        bool synthesisOfAssignmentOk = SyrecSynthesis::onExpression(circuit, statement.rhs, rhs, lhs, statement.op);
        opVec.clear();

        switch (statement.op) {
            case AssignStatement::Add: {
                synthesisOfAssignmentOk &= assignAdd(circuit, lhs, rhs, statement.op);
                break;
            }
            case AssignStatement::Subtract: {
                synthesisOfAssignmentOk &= assignSubtract(circuit, lhs, rhs, statement.op);
                break;
            }
            case AssignStatement::Exor: {
                synthesisOfAssignmentOk &= assignExor(circuit, lhs, rhs, statement.op);
                break;
            }
            default:
                return false;
        }
        return synthesisOfAssignmentOk;
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const IfStatement& statement) {
        // calculate expression
        std::vector<unsigned> expressionResult;

        const bool synthesisOfStatementOk = onExpression(circuit, statement.condition, expressionResult, {}, 0U);
        assert(expressionResult.size() == 1U);
        if (!synthesisOfStatementOk) {
            return false;
        }

        // add new helper line
        const unsigned helperLine = expressionResult.front();
        circuit.activateLocalControlLineScope();
        circuit.registerControlLineInCurrentScope(helperLine);

        for (const Statement::ptr& stat: statement.thenStatements) {
            if (!processStatement(circuit, stat)) {
                return false;
            }
        }

        // Toggle helper line.
        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        circuit.deregisterControlLineInCurrentScope(helperLine);
        circuit.createAndAddNotGate(helperLine);
        circuit.registerControlLineInCurrentScope(helperLine);

        for (const Statement::ptr& stat: statement.elseStatements) {
            if (!processStatement(circuit, stat)) {
                return false;
            }
        }

        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        circuit.deregisterControlLineInCurrentScope(helperLine);
        circuit.createAndAddNotGate(helperLine);
        circuit.deactivateCurrLocalControlLineScope();
        return true;
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const ForStatement& statement) {
        const auto& [nfrom, nTo] = statement.range;

        const unsigned     from         = nfrom ? nfrom->evaluate(loopMap) : 1U; // default value is 1u
        const unsigned     to           = nTo->evaluate(loopMap);
        const unsigned     step         = statement.step ? statement.step->evaluate(loopMap) : 1U; // default step is +1
        const std::string& loopVariable = statement.loopVariable;

        if (from <= to) {
            for (unsigned i = from; i <= to; i += step) {
                // adjust loop variable if necessary

                if (!loopVariable.empty()) {
                    loopMap[loopVariable] = i;
                }

                for (const auto& stat: statement.statements) {
                    if (!processStatement(circuit, stat)) {
                        return false;
                    }
                }
            }
        }

        else if (from > to) {
            for (auto i = static_cast<int>(from); i >= static_cast<int>(to); i -= static_cast<int>(step)) {
                // adjust loop variable if necessary

                if (!loopVariable.empty()) {
                    loopMap[loopVariable] = static_cast<unsigned>(i);
                }

                for (const auto& stat: statement.statements) {
                    if (!processStatement(circuit, stat)) {
                        return false;
                    }
                }
            }
        }
        // clear loop variable if necessary
        if (!loopVariable.empty()) {
            assert(loopMap.erase(loopVariable) == 1U);
        }

        return true;
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const CallStatement& statement) {
        // 1. Adjust the references module's parameters to the call arguments
        for (unsigned i = 0U; i < statement.parameters.size(); ++i) {
            const std::string&   parameter       = statement.parameters.at(i);
            const Variable::ptr& moduleParameter = statement.target->parameters.at(i);

            moduleParameter->setReference(modules.top()->findParameterOrVariable(parameter));
        }

        // 2. Create new lines for the module's variables
        addVariables(circuit, statement.target->variables);

        modules.push(statement.target);
        for (const Statement::ptr& stat: statement.target->statements) {
            if (!processStatement(circuit, stat)) {
                return false;
            }
        }
        modules.pop();

        return true;
    }

    bool SyrecSynthesis::onStatement(Circuit& circuit, const UncallStatement& statement) {
        // 1. Adjust the references module's parameters to the call arguments
        for (unsigned i = 0U; i < statement.parameters.size(); ++i) {
            const std::string& parameter       = statement.parameters.at(i);
            const auto&        moduleParameter = statement.target->parameters.at(i);

            moduleParameter->setReference(modules.top()->findParameterOrVariable(parameter));
        }

        // 2. Create new lines for the module's variables
        addVariables(circuit, statement.target->variables);

        modules.push(statement.target);

        const auto statements = statement.target->statements;
        for (auto it = statements.rbegin(); it != statements.rend(); ++it) {
            const auto reverseStatement = (*it)->reverse();
            if (!processStatement(circuit, reverseStatement)) {
                return false;
            }
        }

        modules.pop();

        return true;
    }

    bool SyrecSynthesis::onStatement(const SkipStatement& statement [[maybe_unused]]) {
        return true;
    }

    bool SyrecSynthesis::onExpression(Circuit& circuit, const Expression::ptr& expression, std::vector<unsigned>& lines, std::vector<unsigned> const& lhsStat, unsigned op) {
        if (auto const* numeric = dynamic_cast<NumericExpression*>(expression.get())) {
            return onExpression(circuit, *numeric, lines);
        }
        if (auto const* variable = dynamic_cast<VariableExpression*>(expression.get())) {
            return onExpression(*variable, lines);
        }
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return onExpression(circuit, *binary, lines, lhsStat, op);
        }
        if (auto const* shift = dynamic_cast<ShiftExpression*>(expression.get())) {
            return onExpression(circuit, *shift, lines, lhsStat, op);
        }
        return false;
    }

    bool SyrecSynthesis::onExpression(Circuit& circuit, const ShiftExpression& expression, std::vector<unsigned>& lines, std::vector<unsigned> const& lhsStat, unsigned op) {
        std::vector<unsigned> lhs;
        if (!onExpression(circuit, expression.lhs, lhs, lhsStat, op)) {
            return false;
        }

        const unsigned rhs = expression.rhs->evaluate(loopMap);
        switch (expression.op) {
            case ShiftExpression::Left: // <<
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                return leftShift(circuit, lines, lhs, rhs);
            case ShiftExpression::Right: // <<
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                return rightShift(circuit, lines, lhs, rhs);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onExpression(Circuit& circuit, const NumericExpression& expression, std::vector<unsigned>& lines) {
        getConstantLines(circuit, expression.bitwidth(), expression.value->evaluate(loopMap), lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(const VariableExpression& expression, std::vector<unsigned>& lines) {
        getVariables(expression.var, lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(Circuit& circuit, const BinaryExpression& expression, std::vector<unsigned>& lines, std::vector<unsigned> const& lhsStat, unsigned op) {
        std::vector<unsigned> lhs;
        std::vector<unsigned> rhs;

        if (!onExpression(circuit, expression.lhs, lhs, lhsStat, op) || !onExpression(circuit, expression.rhs, rhs, lhsStat, op)) {
            return false;
        }

        expLhss.push(lhs);
        expRhss.push(rhs);
        expOpp.push(expression.op);

        if ((expOpp.size() == opVec.size()) && (expOpp.top() == op)) {
            return true;
        }

        bool synthesisOfExprOk = true;
        switch (expression.op) {
            case BinaryExpression::Add: // +
                synthesisOfExprOk = expAdd(circuit, expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Subtract: // -
                synthesisOfExprOk = expSubtract(circuit, expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Exor: // ^
                synthesisOfExprOk = expExor(circuit, expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Multiply: // *
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = multiplication(circuit, lines, lhs, rhs);
                break;
            case BinaryExpression::Divide: // /
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = division(circuit, lines, lhs, rhs);
                break;
            case BinaryExpression::Modulo: { // %
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                std::vector<unsigned> quot;
                getConstantLines(circuit, expression.bitwidth(), 0U, quot);

                synthesisOfExprOk = bitwiseCnot(circuit, lines, lhs); // duplicate lhs
                synthesisOfExprOk &= modulo(circuit, quot, lines, rhs);
            } break;
            case BinaryExpression::LogicalAnd: // &&
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = conjunction(circuit, lines.front(), lhs.front(), rhs.front());
                break;
            case BinaryExpression::LogicalOr: // ||
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = disjunction(circuit, lines.front(), lhs.front(), rhs.front());
                break;
            case BinaryExpression::BitwiseAnd: // &
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = bitwiseAnd(circuit, lines, lhs, rhs);
                break;
            case BinaryExpression::BitwiseOr: // |
                getConstantLines(circuit, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = bitwiseOr(circuit, lines, lhs, rhs);
                break;
            case BinaryExpression::LessThan: // <
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = lessThan(circuit, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::GreaterThan: // >
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = greaterThan(circuit, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::Equals: // =
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = equals(circuit, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::NotEquals: // !=
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = notEquals(circuit, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::LessEquals: // <=
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = lessEquals(circuit, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::GreaterEquals: // >=
                lines.emplace_back(getConstantLine(circuit, false));
                synthesisOfExprOk = greaterEquals(circuit, lines.front(), lhs, rhs);
                break;
            default:
                return false;
        }

        return synthesisOfExprOk;
    }

    /// Function when the assignment statements consist of binary expressions and does not include repeated input signals

    //**********************************************************************
    //*****                      Unary Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::bitwiseNegation(Circuit& circuit, const std::vector<unsigned>& dest) {
        for (const auto line: dest) {
            circuit.createAndAddNotGate(line);
        }
        return true;
    }

    bool SyrecSynthesis::decrement(Circuit& circuit, const std::vector<unsigned>& dest) {
        circuit.activateLocalControlLineScope();
        for (const auto line: dest) {
            circuit.createAndAddNotGate(line);
            circuit.registerControlLineInCurrentScope(line);
        }
        circuit.deactivateCurrLocalControlLineScope();
        return true;
    }

    bool SyrecSynthesis::increment(Circuit& circuit, const std::vector<unsigned>& dest) {
        circuit.activateLocalControlLineScope();
        for (const auto line: dest) {
            circuit.registerControlLineInCurrentScope(line);
        }

        for (int i = static_cast<int>(dest.size()) - 1; i >= 0; --i) {
            circuit.deregisterControlLineInCurrentScope(dest[static_cast<std::size_t>(i)]);
            circuit.createAndAddNotGate(dest[static_cast<std::size_t>(i)]);
        }
        circuit.deactivateCurrLocalControlLineScope();
        return true;
    }

    //**********************************************************************
    //*****                     Binary Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::bitwiseAnd(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk &= conjunction(circuit, dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseCnot(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src) {
        const bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            circuit.createAndAddCnotGate(src[i], dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseOr(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk &= disjunction(circuit, dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::conjunction(Circuit& circuit, unsigned dest, unsigned src1, unsigned src2) {
        circuit.createAndAddToffoliGate(src1, src2, dest);
        return true;
    }

    bool SyrecSynthesis::decreaseWithCarry(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src, unsigned carry) {
        bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            circuit.createAndAddNotGate(dest[i]);
        }

        synthesisOk &= increaseWithCarry(circuit, dest, src, carry);
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            circuit.createAndAddNotGate(dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::disjunction(Circuit& circuit, const unsigned dest, const unsigned src1, const unsigned src2) {
        circuit.createAndAddCnotGate(src1, dest);
        circuit.createAndAddCnotGate(src2, dest);
        circuit.createAndAddToffoliGate(src1, src2, dest);
        return true;
    }

    bool SyrecSynthesis::division(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        if (!modulo(circuit, dest, src1, src2)) {
            return false;
        }

        std::vector<unsigned> sum;
        std::vector<unsigned> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 1; i < src1.size(); ++i) {
            circuit.createAndAddNotGate(src2[i]);
        }

        circuit.activateLocalControlLineScope();
        for (std::size_t i = 1U; i < src1.size(); ++i) {
            circuit.registerControlLineInCurrentScope(src2[i]);
        }

        std::size_t helperIndex = 0;
        bool        synthesisOk = true;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[castedIndex]);
            circuit.registerControlLineInCurrentScope(dest[castedIndex]);
            synthesisOk = increase(circuit, sum, partial);
            circuit.deregisterControlLineInCurrentScope(dest[castedIndex]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                circuit.deregisterControlLineInCurrentScope(src2[j]);
            }
            circuit.createAndAddNotGate(src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                circuit.registerControlLineInCurrentScope(src2[j]);
            }
        }
        circuit.deactivateCurrLocalControlLineScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::equals(Circuit& circuit, const unsigned dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        if (src2.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 0; i < src1.size(); ++i) {
            circuit.createAndAddCnotGate(src2[i], src1[i]);
            circuit.createAndAddNotGate(src1[i]);
        }

        circuit.createAndAddMultiControlToffoliGate(Gate::LinesLookup(src1.begin(), src1.end()), dest);

        for (std::size_t i = 0; i < src1.size(); ++i) {
            circuit.createAndAddCnotGate(src2[i], src1[i]);
            circuit.createAndAddNotGate(src1[i]);
        }
        return true;
    }

    bool SyrecSynthesis::greaterEquals(Circuit& circuit, const unsigned dest, const std::vector<unsigned>& srcTwo, const std::vector<unsigned>& srcOne) {
        if (!greaterThan(circuit, dest, srcOne, srcTwo)) {
            return false;
        }

        circuit.createAndAddNotGate(dest);
        return true;
    }

    bool SyrecSynthesis::greaterThan(Circuit& circuit, const unsigned dest, const std::vector<unsigned>& src2, const std::vector<unsigned>& src1) {
        return lessThan(circuit, dest, src1, src2);
    }

    bool SyrecSynthesis::increase(Circuit& circuit, const std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        if (rhs.empty()) {
            return true;
        }

        if (rhs.size() == 1) {
            circuit.createAndAddCnotGate(lhs.front(), rhs.front());
            return true;
        }

        const std::size_t bitwidth = rhs.size();
        for (std::size_t i = 1; i <= bitwidth - 1; ++i) {
            circuit.createAndAddCnotGate(lhs[i], rhs[i]);
        }

        for (std::size_t i = bitwidth - 2; i >= 1; --i) {
            circuit.createAndAddCnotGate(lhs[i], rhs[i]);
        }

        for (std::size_t i = 0; i <= bitwidth - 2; ++i) {
            circuit.createAndAddToffoliGate(rhs[i], lhs[i], lhs[i + 1]);
        }

        circuit.createAndAddCnotGate(lhs[bitwidth - 1], rhs[bitwidth - 1]);
        for (std::size_t i = bitwidth - 2; i >= 1; --i) {
            circuit.createAndAddToffoliGate(lhs[i], rhs[i], lhs[i + 1]);
            circuit.createAndAddCnotGate(lhs[i], rhs[i]);
        }
        circuit.createAndAddToffoliGate(lhs.front(), rhs.front(), lhs[1]);
        circuit.createAndAddCnotGate(lhs.front(), rhs.front());

        for (std::size_t i = 1; i <= bitwidth - 2; ++i) {
            circuit.createAndAddCnotGate(lhs[i], rhs[i + 1]);
        }

        for (std::size_t i = 1; i <= bitwidth - 1; ++i) {
            circuit.createAndAddCnotGate(lhs[i], rhs[i]);
        }
        return true;
    }

    bool SyrecSynthesis::decrease(Circuit& circuit, const std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs) {
        for (const auto rhsOperandLine: rhs) {
            circuit.createAndAddNotGate(rhsOperandLine);
        }

        if (!increase(circuit, rhs, lhs)) {
            return false;
        }

        for (const auto rhsOperandLine: rhs) {
            circuit.createAndAddNotGate(rhsOperandLine);
        }
        return true;
    }

    bool SyrecSynthesis::increaseWithCarry(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src, unsigned carry) {
        auto bitwidth = static_cast<int>(src.size());
        if (bitwidth == 0) {
            return true;
        }

        if (src.size() != dest.size()) {
            return false;
        }

        const auto unsignedBitwidth = static_cast<std::size_t>(bitwidth);
        for (std::size_t i = 1U; i < unsignedBitwidth; ++i) {
            circuit.createAndAddCnotGate(src.at(i), dest.at(i));
        }

        if (bitwidth > 1) {
            circuit.createAndAddCnotGate(src.at(unsignedBitwidth - 1), carry);
        }

        for (int i = bitwidth - 2; i > 0; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            circuit.createAndAddCnotGate(src.at(castedIndex), src.at(castedIndex + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth - 1; ++i) {
            circuit.createAndAddToffoliGate(src.at(i), dest.at(i), src.at(i + 1));
        }
        circuit.createAndAddToffoliGate(src.at(unsignedBitwidth - 1), dest.at(unsignedBitwidth - 1), carry);

        for (int i = bitwidth - 1; i > 0; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            circuit.createAndAddCnotGate(src.at(castedIndex), dest.at(castedIndex));
            circuit.createAndAddToffoliGate(dest.at(castedIndex - 1), src.at(castedIndex - 1), src.at(castedIndex));
        }

        for (std::size_t i = 1U; i < unsignedBitwidth - 1; ++i) {
            circuit.createAndAddCnotGate(src.at(i), src.at(i + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth; ++i) {
            circuit.createAndAddCnotGate(src.at(i), dest.at(i));
        }
        return true;
    }

    bool SyrecSynthesis::lessEquals(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src2, const std::vector<unsigned>& src1) {
        if (!lessThan(circuit, dest, src1, src2)) {
            return false;
        }
        circuit.createAndAddNotGate(dest);
        return true;
    }

    bool SyrecSynthesis::lessThan(Circuit& circuit, unsigned dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        return decreaseWithCarry(circuit, src1, src2, dest) && increase(circuit, src1, src2);
    }

    bool SyrecSynthesis::modulo(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        std::vector<unsigned> sum;
        std::vector<unsigned> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 1; i < src1.size(); ++i) {
            circuit.createAndAddNotGate(src2[i]);
        }

        circuit.activateLocalControlLineScope();
        for (std::size_t i = 1; i < src1.size(); ++i) {
            circuit.registerControlLineInCurrentScope(src2[i]);
        }

        std::size_t helperIndex = 0;
        bool        synthesisOk = true;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto unsignedLoopVariableValue = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[unsignedLoopVariableValue]);
            synthesisOk = decreaseWithCarry(circuit, sum, partial, dest[unsignedLoopVariableValue]);

            circuit.registerControlLineInCurrentScope(dest[unsignedLoopVariableValue]);
            synthesisOk &= increase(circuit, sum, partial);
            circuit.deregisterControlLineInCurrentScope(dest[unsignedLoopVariableValue]);

            circuit.createAndAddNotGate(dest[unsignedLoopVariableValue]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                circuit.deregisterControlLineInCurrentScope(src2[j]);
            }
            circuit.createAndAddNotGate(src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                circuit.registerControlLineInCurrentScope(src2[j]);
            }
        }
        circuit.deactivateCurrLocalControlLineScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::multiplication(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        if (src1.empty() || dest.empty()) {
            return true;
        }

        if (src1.size() < dest.size() || src2.size() < dest.size()) {
            return false;
        }

        std::vector<unsigned> sum     = dest;
        std::vector<unsigned> partial = src2;

        bool synthesisOk = true;
        circuit.activateLocalControlLineScope();
        circuit.registerControlLineInCurrentScope(src1.front());
        synthesisOk = synthesisOk && bitwiseCnot(circuit, sum, partial);
        circuit.deregisterControlLineInCurrentScope(src1.front());

        for (std::size_t i = 1; i < dest.size() && synthesisOk; ++i) {
            sum.erase(sum.begin());
            partial.pop_back();
            circuit.registerControlLineInCurrentScope(src1[i]);
            synthesisOk &= increase(circuit, sum, partial);
            circuit.deregisterControlLineInCurrentScope(src1[i]);
        }
        circuit.deactivateCurrLocalControlLineScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::notEquals(Circuit& circuit, const unsigned dest, const std::vector<unsigned>& src1, const std::vector<unsigned>& src2) {
        if (!equals(circuit, dest, src1, src2)) {
            return false;
        }

        circuit.createAndAddNotGate(dest);
        return true;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-noexcept-swap, performance-noexcept-swap, bugprone-exception-escape)
    bool SyrecSynthesis::swap(Circuit& circuit, const std::vector<unsigned>& dest1, const std::vector<unsigned>& dest2) {
        if (dest2.size() < dest1.size()) {
            return false;
        }

        for (std::size_t i = 0; i < dest1.size(); ++i) {
            circuit.createAndAddFredkinGate(dest1[i], dest2[i]);
        }
        return true;
    }

    //**********************************************************************
    //*****                      Shift Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::leftShift(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, unsigned src2) {
        if (src2 > dest.size()) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        const std::size_t targetLineBaseOffset = src2;
        for (std::size_t i = 0; i < nQubitsShifted; ++i) {
            circuit.createAndAddCnotGate(src1[i], dest[targetLineBaseOffset + i]);
        }
        return true;
    }

    bool SyrecSynthesis::rightShift(Circuit& circuit, const std::vector<unsigned>& dest, const std::vector<unsigned>& src1, unsigned src2) {
        if (dest.size() < src2) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        for (std::size_t i = 0; i < nQubitsShifted; ++i) {
            circuit.createAndAddCnotGate(src1[i], dest[i]);
        }
        return true;
    }

    bool SyrecSynthesis::expressionOpInverse([[maybe_unused]] Circuit& circuit, [[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<unsigned>& expLhs, [[maybe_unused]] const std::vector<unsigned>& expRhs) const {
        return true;
    }

    void SyrecSynthesis::getVariables(const VariableAccess::ptr& var, std::vector<unsigned>& lines) {
        const auto&       referenceVariableData           = var->getVar();
        unsigned          offset                          = varLines[referenceVariableData];
        const std::size_t numDeclaredDimensionsOfVariable = referenceVariableData->dimensions.size();

        if (!var->indexes.empty()) {
            // check if it is all numeric_expressions
            if (static_cast<std::size_t>(std::count_if(var->indexes.cbegin(), var->indexes.cend(), [&](const auto& p) { return dynamic_cast<NumericExpression*>(p.get()); })) == numDeclaredDimensionsOfVariable) {
                for (std::size_t i = 0U; i < numDeclaredDimensionsOfVariable; ++i) {
                    const auto   evaluatedDimensionIndexValue = dynamic_cast<NumericExpression*>(var->indexes.at(i).get())->value->evaluate(loopMap);
                    unsigned int aggregateValue               = evaluatedDimensionIndexValue;
                    for (std::size_t j = i + 1; j < numDeclaredDimensionsOfVariable; ++j) {
                        aggregateValue *= referenceVariableData->dimensions[i];
                    }
                    offset += aggregateValue * referenceVariableData->bitwidth;
                }
            }
        }

        if (var->range) {
            auto [nfirst, nsecond] = *var->range;

            const unsigned first  = nfirst->evaluate(loopMap);
            const unsigned second = nsecond->evaluate(loopMap);

            if (first < second) {
                for (unsigned i = first; i <= second; ++i) {
                    lines.emplace_back(offset + i);
                }
            } else {
                for (auto i = static_cast<int>(first); i >= static_cast<int>(second); --i) {
                    lines.emplace_back(offset + static_cast<unsigned>(i));
                }
            }
        } else {
            for (unsigned i = 0U; i < referenceVariableData->bitwidth; ++i) {
                lines.emplace_back(offset + i);
            }
        }
    }

    unsigned SyrecSynthesis::getConstantLine(Circuit& circuit, bool value) {
        unsigned constLine = 0U;

        if (!freeConstLinesMap[value].empty()) {
            constLine = freeConstLinesMap[value].back();
            freeConstLinesMap[value].pop_back();
        } else if (!freeConstLinesMap[!value].empty()) {
            constLine = freeConstLinesMap[!value].back();
            freeConstLinesMap[!value].pop_back();
            circuit.createAndAddNotGate(constLine);
        } else {
            constLine = circuit.addLine((std::string("const_") + std::to_string(static_cast<int>(value))), "garbage", value, true);
        }

        return constLine;
    }

    void SyrecSynthesis::getConstantLines(Circuit& circuit, unsigned bitwidth, unsigned value, std::vector<unsigned>& lines) {
        assert(bitwidth <= 32);
        for (unsigned i = 0U; i < bitwidth; ++i) {
            lines.emplace_back(getConstantLine(circuit, (value & (1 << i)) != 0));
        }
    }

    void SyrecSynthesis::addVariable(Circuit& circ, const std::vector<unsigned>& dimensions, const Variable::ptr& var,
                                     const constant constant, bool garbage, const std::string& arraystr) {
        if (dimensions.empty()) {
            for (unsigned i = 0U; i < var->bitwidth; ++i) {
                const std::string name = var->name + arraystr + "." + std::to_string(i);
                circ.addLine(name, name, constant, garbage);
            }
        } else {
            const unsigned              len = dimensions.front();
            const std::vector<unsigned> newDimensions(dimensions.begin() + 1U, dimensions.end());

            for (unsigned i = 0U; i < len; ++i) {
                addVariable(circ, newDimensions, var, constant, garbage, arraystr + "[" + std::to_string(i) + "]");
            }
        }
    }

    void SyrecSynthesis::addVariables(Circuit& circVar, const Variable::vec& variables) {
        for (const auto& var: variables) {
            // entry in var lines map
            varLines.try_emplace(var, circVar.getLines());

            // types of constant and garbage
            const constant constVar = (var->type == Variable::Out || var->type == Variable::Wire) ? constant(false) : constant();
            const bool     garbage  = (var->type == Variable::In || var->type == Variable::Wire);

            addVariable(circVar, var->dimensions, var, constVar, garbage, std::string());
        }
    }

    bool SyrecSynthesis::synthesize(SyrecSynthesis* synthesizer, Circuit& circ, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        // Settings parsing
        auto mainModule = get<std::string>(settings, "main_module", std::string());
        // Run-time measuring
        Timer<PropertiesTimer> t;

        if (statistics) {
            const PropertiesTimer rt(statistics);
            t.start(rt);
        }

        // get the main module
        Module::ptr main;

        if (!mainModule.empty()) {
            main = program.findModule(mainModule);
            if (!main) {
                std::cerr << "Program has no module: " << mainModule << "\n";
                return false;
            }
        } else {
            main = program.findModule("main");
            if (!main) {
                main = program.modules().front();
            }
        }

        // declare as top module
        synthesizer->setMainModule(main);

        // create lines for global variables
        synthesizer->addVariables(circ, main->parameters);
        synthesizer->addVariables(circ, main->variables);

        // synthesize the statements
        const auto synthesisOfMainModuleOk = synthesizer->onModule(circ, main);
        if (statistics) {
            t.stop();
        }
        return synthesisOfMainModuleOk;
    }
} // namespace syrec
