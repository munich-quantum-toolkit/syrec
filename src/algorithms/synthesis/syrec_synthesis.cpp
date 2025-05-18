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

#include "core/gate.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "core/utils/timer.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

namespace syrec {
    // Helper Functions for the synthesis methods
    SyrecSynthesis::SyrecSynthesis(qc::QuantumComputation& quantumComputation):
        quantumComputation(quantumComputation) {
        freeConstLinesMap.try_emplace(false /* emplacing a default constructed object */);
        freeConstLinesMap.try_emplace(true /* emplacing a default constructed object */);
    }

    void SyrecSynthesis::setMainModule(const Module::ptr& mainModule) {
        assert(modules.empty());
        modules.push(mainModule);
    }

    bool SyrecSynthesis::onModule(qc::QuantumComputation& quantumComputation, const Module::ptr& main) {
        bool              synthesisOfModuleStatementOk = true;
        const std::size_t nModuleStatements            = main->statements.size();
        for (std::size_t i = 0; i < nModuleStatements && synthesisOfModuleStatementOk; ++i) {
            synthesisOfModuleStatementOk = processStatement(quantumComputation, main->statements[i]);
        }
        return synthesisOfModuleStatementOk;
    }

    /// If the input signals are repeated (i.e., rhs input signals are repeated)
    bool SyrecSynthesis::checkRepeats() {
        std::vector checkLhsVec(expLhsVector.cbegin(), expLhsVector.cend());
        std::vector checkRhsVec(expRhsVector.cbegin(), expRhsVector.cend());

        checkLhsVec.erase(std::remove_if(checkLhsVec.begin(), checkLhsVec.end(), [](const std::vector<qc::Qubit>& linesContainer) { return linesContainer.empty(); }), checkLhsVec.end());
        checkRhsVec.erase(std::remove_if(checkRhsVec.begin(), checkRhsVec.end(), [](const std::vector<qc::Qubit>& linesContainer) { return linesContainer.empty(); }), checkRhsVec.end());

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

    bool SyrecSynthesis::opRhsLhsExpression([[maybe_unused]] const Expression::ptr& expression, [[maybe_unused]] std::vector<qc::Qubit>& v) {
        return true;
    }
    bool SyrecSynthesis::opRhsLhsExpression([[maybe_unused]] const VariableExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v) {
        return true;
    }
    bool SyrecSynthesis::opRhsLhsExpression([[maybe_unused]] const BinaryExpression& expression, [[maybe_unused]] std::vector<qc::Qubit>& v) {
        return true;
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const Statement::ptr& statement) {
        stmts.push(statement);

        setOrUpdateGlobalGateAnnotation(GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

        bool okay = true;
        if (auto const* swapStat = dynamic_cast<SwapStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *swapStat);
        } else if (auto const* unaryStat = dynamic_cast<UnaryStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *unaryStat);
        } else if (auto const* assignStat = dynamic_cast<AssignStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *assignStat);
        } else if (auto const* ifStat = dynamic_cast<IfStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *ifStat);
        } else if (auto const* forStat = dynamic_cast<ForStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *forStat);
        } else if (auto const* callStat = dynamic_cast<CallStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *callStat);
        } else if (auto const* uncallStat = dynamic_cast<UncallStatement*>(statement.get())) {
            okay = onStatement(quantumComputation, *uncallStat);
        } else if (auto const* skipStat = statement.get()) {
            okay = onStatement(*skipStat);
        } else {
            okay = false;
        }

        stmts.pop();
        return okay;
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const SwapStatement& statement) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;

        getVariables(statement.lhs, lhs);
        getVariables(statement.rhs, rhs);

        assert(lhs.size() == rhs.size());

        return swap(quantumComputation, lhs, rhs);
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const UnaryStatement& statement) {
        // load variable
        std::vector<qc::Qubit> var;
        getVariables(statement.var, var);

        switch (statement.op) {
            case UnaryStatement::Invert:
                return bitwiseNegation(quantumComputation, var);
            case UnaryStatement::Increment:
                return increment(quantumComputation, var);
            case UnaryStatement::Decrement:
                return decrement(quantumComputation, var);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const AssignStatement& statement) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;
        std::vector<qc::Qubit> d;

        getVariables(statement.lhs, lhs);
        opRhsLhsExpression(statement.rhs, d);

        bool synthesisOfAssignmentOk = SyrecSynthesis::onExpression(quantumComputation, statement.rhs, rhs, lhs, statement.op);
        opVec.clear();

        switch (statement.op) {
            case AssignStatement::Add: {
                synthesisOfAssignmentOk &= assignAdd(quantumComputation, lhs, rhs, statement.op);
                break;
            }
            case AssignStatement::Subtract: {
                synthesisOfAssignmentOk &= assignSubtract(quantumComputation, lhs, rhs, statement.op);
                break;
            }
            case AssignStatement::Exor: {
                synthesisOfAssignmentOk &= assignExor(quantumComputation, lhs, rhs, statement.op);
                break;
            }
            default:
                return false;
        }
        return synthesisOfAssignmentOk;
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const IfStatement& statement) {
        // calculate expression
        std::vector<qc::Qubit> expressionResult;

        const bool synthesisOfStatementOk = onExpression(quantumComputation, statement.condition, expressionResult, {}, 0U);
        assert(expressionResult.size() == 1U);
        if (!synthesisOfStatementOk) {
            return false;
        }

        // add new helper line
        const qc::Qubit helperLine = expressionResult.front();
        activateControlLinePropagationScope();
        registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, helperLine);

        for (const Statement::ptr& stat: statement.thenStatements) {
            if (!processStatement(quantumComputation, stat)) {
                return false;
            }
        }

        // Toggle helper line.
        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        deregisterControlLineFromPropagationInCurrentScope(quantumComputation, helperLine);
        createAndAddNotGate(quantumComputation, helperLine);
        registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, helperLine);

        for (const Statement::ptr& stat: statement.elseStatements) {
            if (!processStatement(quantumComputation, stat)) {
                return false;
            }
        }

        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        deregisterControlLineFromPropagationInCurrentScope(quantumComputation, helperLine);
        createAndAddNotGate(quantumComputation, helperLine);
        deactivateControlLinePropagationScope();
        return true;
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const ForStatement& statement) {
        const auto& [nfrom, nTo] = statement.range;

        const qc::Qubit    from         = nfrom ? nfrom->evaluate(loopMap) : 1U; // default value is 1u
        const qc::Qubit    to           = nTo->evaluate(loopMap);
        const qc::Qubit    step         = statement.step ? statement.step->evaluate(loopMap) : 1U; // default step is +1
        const std::string& loopVariable = statement.loopVariable;

        if (from <= to) {
            for (qc::Qubit i = from; i <= to; i += step) {
                // adjust loop variable if necessary

                if (!loopVariable.empty()) {
                    loopMap[loopVariable] = i;
                }

                for (const auto& stat: statement.statements) {
                    if (!processStatement(quantumComputation, stat)) {
                        return false;
                    }
                }
            }
        }

        else if (from > to) {
            for (auto i = static_cast<int>(from); i >= static_cast<int>(to); i -= static_cast<int>(step)) {
                // adjust loop variable if necessary

                if (!loopVariable.empty()) {
                    loopMap[loopVariable] = static_cast<qc::Qubit>(i);
                }

                for (const auto& stat: statement.statements) {
                    if (!processStatement(quantumComputation, stat)) {
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

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const CallStatement& statement) {
        // 1. Adjust the references module's parameters to the call arguments
        for (qc::Qubit i = 0U; i < statement.parameters.size(); ++i) {
            const std::string&   parameter       = statement.parameters.at(i);
            const Variable::ptr& moduleParameter = statement.target->parameters.at(i);

            moduleParameter->setReference(modules.top()->findParameterOrVariable(parameter));
        }

        // 2. Create new lines for the module's variables
        addVariables(quantumComputation, statement.target->variables);

        modules.push(statement.target);
        for (const Statement::ptr& stat: statement.target->statements) {
            if (!processStatement(quantumComputation, stat)) {
                return false;
            }
        }
        modules.pop();

        return true;
    }

    bool SyrecSynthesis::onStatement(qc::QuantumComputation& quantumComputation, const UncallStatement& statement) {
        // 1. Adjust the references module's parameters to the call arguments
        for (qc::Qubit i = 0U; i < statement.parameters.size(); ++i) {
            const std::string& parameter       = statement.parameters.at(i);
            const auto&        moduleParameter = statement.target->parameters.at(i);

            moduleParameter->setReference(modules.top()->findParameterOrVariable(parameter));
        }

        // 2. Create new lines for the module's variables
        addVariables(quantumComputation, statement.target->variables);

        modules.push(statement.target);

        const auto statements = statement.target->statements;
        for (auto it = statements.rbegin(); it != statements.rend(); ++it) {
            const auto reverseStatement = (*it)->reverse();
            if (!processStatement(quantumComputation, reverseStatement)) {
                return false;
            }
        }

        modules.pop();

        return true;
    }

    bool SyrecSynthesis::onStatement(const SkipStatement& statement [[maybe_unused]]) {
        return true;
    }

    bool SyrecSynthesis::onExpression(qc::QuantumComputation& quantumComputation, const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, qc::Qubit op) {
        if (auto const* numeric = dynamic_cast<NumericExpression*>(expression.get())) {
            return onExpression(quantumComputation, *numeric, lines);
        }
        if (auto const* variable = dynamic_cast<VariableExpression*>(expression.get())) {
            return onExpression(*variable, lines);
        }
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return onExpression(quantumComputation, *binary, lines, lhsStat, op);
        }
        if (auto const* shift = dynamic_cast<ShiftExpression*>(expression.get())) {
            return onExpression(quantumComputation, *shift, lines, lhsStat, op);
        }
        return false;
    }

    bool SyrecSynthesis::onExpression(qc::QuantumComputation& quantumComputation, const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, qc::Qubit op) {
        std::vector<qc::Qubit> lhs;
        if (!onExpression(quantumComputation, expression.lhs, lhs, lhsStat, op)) {
            return false;
        }

        const qc::Qubit rhs = expression.rhs->evaluate(loopMap);
        switch (expression.op) {
            case ShiftExpression::Left: // <<
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                return leftShift(quantumComputation, lines, lhs, rhs);
            case ShiftExpression::Right: // <<
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                return rightShift(quantumComputation, lines, lhs, rhs);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onExpression(qc::QuantumComputation& quantumComputation, const NumericExpression& expression, std::vector<qc::Qubit>& lines) {
        getConstantLines(quantumComputation, expression.bitwidth(), expression.value->evaluate(loopMap), lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines) {
        getVariables(expression.var, lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(qc::QuantumComputation& quantumComputation, const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, qc::Qubit op) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;

        if (!onExpression(quantumComputation, expression.lhs, lhs, lhsStat, op) || !onExpression(quantumComputation, expression.rhs, rhs, lhsStat, op)) {
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
                synthesisOfExprOk = expAdd(quantumComputation, expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Subtract: // -
                synthesisOfExprOk = expSubtract(quantumComputation, expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Exor: // ^
                synthesisOfExprOk = expExor(quantumComputation, expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Multiply: // *
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = multiplication(quantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::Divide: // /
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = division(quantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::Modulo: { // %
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                std::vector<qc::Qubit> quot;
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, quot);

                synthesisOfExprOk = bitwiseCnot(quantumComputation, lines, lhs); // duplicate lhs
                synthesisOfExprOk &= modulo(quantumComputation, quot, lines, rhs);
            } break;
            case BinaryExpression::LogicalAnd: // &&
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = conjunction(quantumComputation, lines.front(), lhs.front(), rhs.front());
                break;
            case BinaryExpression::LogicalOr: // ||
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = disjunction(quantumComputation, lines.front(), lhs.front(), rhs.front());
                break;
            case BinaryExpression::BitwiseAnd: // &
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = bitwiseAnd(quantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::BitwiseOr: // |
                getConstantLines(quantumComputation, expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = bitwiseOr(quantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::LessThan: // <
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = lessThan(quantumComputation, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::GreaterThan: // >
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = greaterThan(quantumComputation, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::Equals: // =
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = equals(quantumComputation, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::NotEquals: // !=
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = notEquals(quantumComputation, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::LessEquals: // <=
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = lessEquals(quantumComputation, lines.front(), lhs, rhs);
                break;
            case BinaryExpression::GreaterEquals: // >=
                lines.emplace_back(getConstantLine(quantumComputation, false));
                synthesisOfExprOk = greaterEquals(quantumComputation, lines.front(), lhs, rhs);
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

    bool SyrecSynthesis::bitwiseNegation(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest) {
        for (const auto line: dest) {
            createAndAddNotGate(quantumComputation, line);
        }
        return true;
    }

    bool SyrecSynthesis::decrement(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest) {
        activateControlLinePropagationScope();
        for (const auto line: dest) {
            createAndAddNotGate(quantumComputation, line);
            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, line);
        }
        deactivateControlLinePropagationScope();
        return true;
    }

    bool SyrecSynthesis::increment(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest) {
        activateControlLinePropagationScope();
        for (const auto line: dest) {
            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, line);
        }

        for (int i = static_cast<int>(dest.size()) - 1; i >= 0; --i) {
            deregisterControlLineFromPropagationInCurrentScope(quantumComputation, dest[static_cast<std::size_t>(i)]);
            createAndAddNotGate(quantumComputation, dest[static_cast<std::size_t>(i)]);
        }
        deactivateControlLinePropagationScope();
        return true;
    }

    //**********************************************************************
    //*****                     Binary Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::bitwiseAnd(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk &= conjunction(quantumComputation, dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseCnot(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src) {
        const bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            createAndAddCnotGate(quantumComputation, src[i], dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseOr(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk &= disjunction(quantumComputation, dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::conjunction(qc::QuantumComputation& quantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2) {
        createAndAddToffoliGate(quantumComputation, src1, src2, dest);
        return true;
    }

    bool SyrecSynthesis::decreaseWithCarry(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry) {
        bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            createAndAddNotGate(quantumComputation, dest[i]);
        }

        synthesisOk &= increaseWithCarry(quantumComputation, dest, src, carry);
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            createAndAddNotGate(quantumComputation, dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::disjunction(qc::QuantumComputation& quantumComputation, const qc::Qubit dest, const qc::Qubit src1, const qc::Qubit src2) {
        createAndAddCnotGate(quantumComputation, src1, dest);
        createAndAddCnotGate(quantumComputation, src2, dest);
        createAndAddToffoliGate(quantumComputation, src1, src2, dest);
        return true;
    }

    bool SyrecSynthesis::division(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (!modulo(quantumComputation, dest, src1, src2)) {
            return false;
        }

        std::vector<qc::Qubit> sum;
        std::vector<qc::Qubit> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 1; i < src1.size(); ++i) {
            createAndAddNotGate(quantumComputation, src2[i]);
        }

        activateControlLinePropagationScope();
        for (std::size_t i = 1U; i < src1.size(); ++i) {
            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, src2[i]);
        }

        std::size_t helperIndex = 0;
        bool        synthesisOk = true;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[castedIndex]);
            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, dest[castedIndex]);
            synthesisOk = increase(quantumComputation, sum, partial);
            deregisterControlLineFromPropagationInCurrentScope(quantumComputation, dest[castedIndex]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                deregisterControlLineFromPropagationInCurrentScope(quantumComputation, src2[j]);
            }
            createAndAddNotGate(quantumComputation, src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, src2[j]);
            }
        }
        deactivateControlLinePropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::equals(qc::QuantumComputation& quantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (src2.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 0; i < src1.size(); ++i) {
            createAndAddCnotGate(quantumComputation, src2[i], src1[i]);
            createAndAddNotGate(quantumComputation, src1[i]);
        }

        createAndAddMultiControlToffoliGate(quantumComputation, std::unordered_set<qc::Qubit>(src1.begin(), src1.end()), dest);

        for (std::size_t i = 0; i < src1.size(); ++i) {
            createAndAddCnotGate(quantumComputation, src2[i], src1[i]);
            createAndAddNotGate(quantumComputation, src1[i]);
        }
        return true;
    }

    bool SyrecSynthesis::greaterEquals(qc::QuantumComputation& quantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne) {
        if (!greaterThan(quantumComputation, dest, srcOne, srcTwo)) {
            return false;
        }

        createAndAddNotGate(quantumComputation, dest);
        return true;
    }

    bool SyrecSynthesis::greaterThan(qc::QuantumComputation& quantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        return lessThan(quantumComputation, dest, src1, src2);
    }

    bool SyrecSynthesis::increase(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        if (rhs.empty()) {
            return true;
        }

        if (rhs.size() == 1) {
            createAndAddCnotGate(quantumComputation, lhs.front(), rhs.front());
            return true;
        }

        const std::size_t bitwidth = rhs.size();
        for (std::size_t i = 1; i <= bitwidth - 1; ++i) {
            createAndAddCnotGate(quantumComputation, lhs[i], rhs[i]);
        }

        for (std::size_t i = bitwidth - 2; i >= 1; --i) {
            createAndAddCnotGate(quantumComputation, lhs[i], rhs[i]);
        }

        for (std::size_t i = 0; i <= bitwidth - 2; ++i) {
            createAndAddToffoliGate(quantumComputation, rhs[i], lhs[i], lhs[i + 1]);
        }

        createAndAddCnotGate(quantumComputation, lhs[bitwidth - 1], rhs[bitwidth - 1]);
        for (std::size_t i = bitwidth - 2; i >= 1; --i) {
            createAndAddToffoliGate(quantumComputation, lhs[i], rhs[i], lhs[i + 1]);
            createAndAddCnotGate(quantumComputation, lhs[i], rhs[i]);
        }
        createAndAddToffoliGate(quantumComputation, lhs.front(), rhs.front(), lhs[1]);
        createAndAddCnotGate(quantumComputation, lhs.front(), rhs.front());

        for (std::size_t i = 1; i <= bitwidth - 2; ++i) {
            createAndAddCnotGate(quantumComputation, lhs[i], rhs[i + 1]);
        }

        for (std::size_t i = 1; i <= bitwidth - 1; ++i) {
            createAndAddCnotGate(quantumComputation, lhs[i], rhs[i]);
        }
        return true;
    }

    bool SyrecSynthesis::decrease(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        for (const auto rhsOperandLine: rhs) {
            createAndAddNotGate(quantumComputation, rhsOperandLine);
        }

        if (!increase(quantumComputation, rhs, lhs)) {
            return false;
        }

        for (const auto rhsOperandLine: rhs) {
            createAndAddNotGate(quantumComputation, rhsOperandLine);
        }
        return true;
    }

    bool SyrecSynthesis::increaseWithCarry(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry) {
        auto bitwidth = static_cast<int>(src.size());
        if (bitwidth == 0) {
            return true;
        }

        if (src.size() != dest.size()) {
            return false;
        }

        const auto unsignedBitwidth = static_cast<std::size_t>(bitwidth);
        for (std::size_t i = 1U; i < unsignedBitwidth; ++i) {
            createAndAddCnotGate(quantumComputation, src.at(i), dest.at(i));
        }

        if (bitwidth > 1) {
            createAndAddCnotGate(quantumComputation, src.at(unsignedBitwidth - 1), carry);
        }

        for (int i = bitwidth - 2; i > 0; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            createAndAddCnotGate(quantumComputation, src.at(castedIndex), src.at(castedIndex + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth - 1; ++i) {
            createAndAddToffoliGate(quantumComputation, src.at(i), dest.at(i), src.at(i + 1));
        }
        createAndAddToffoliGate(quantumComputation, src.at(unsignedBitwidth - 1), dest.at(unsignedBitwidth - 1), carry);

        for (int i = bitwidth - 1; i > 0; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            createAndAddCnotGate(quantumComputation, src.at(castedIndex), dest.at(castedIndex));
            createAndAddToffoliGate(quantumComputation, dest.at(castedIndex - 1), src.at(castedIndex - 1), src.at(castedIndex));
        }

        for (std::size_t i = 1U; i < unsignedBitwidth - 1; ++i) {
            createAndAddCnotGate(quantumComputation, src.at(i), src.at(i + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth; ++i) {
            createAndAddCnotGate(quantumComputation, src.at(i), dest.at(i));
        }
        return true;
    }

    bool SyrecSynthesis::lessEquals(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        if (!lessThan(quantumComputation, dest, src1, src2)) {
            return false;
        }
        createAndAddNotGate(quantumComputation, dest);
        return true;
    }

    bool SyrecSynthesis::lessThan(qc::QuantumComputation& quantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        return decreaseWithCarry(quantumComputation, src1, src2, dest) && increase(quantumComputation, src1, src2);
    }

    bool SyrecSynthesis::modulo(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        std::vector<qc::Qubit> sum;
        std::vector<qc::Qubit> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 1; i < src1.size(); ++i) {
            createAndAddNotGate(quantumComputation, src2[i]);
        }

        activateControlLinePropagationScope();
        for (std::size_t i = 1; i < src1.size(); ++i) {
            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, src2[i]);
        }

        std::size_t helperIndex = 0;
        bool        synthesisOk = true;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto unsignedLoopVariableValue = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[unsignedLoopVariableValue]);
            synthesisOk = decreaseWithCarry(quantumComputation, sum, partial, dest[unsignedLoopVariableValue]);

            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, dest[unsignedLoopVariableValue]);
            synthesisOk &= increase(quantumComputation, sum, partial);
            deregisterControlLineFromPropagationInCurrentScope(quantumComputation, dest[unsignedLoopVariableValue]);

            createAndAddNotGate(quantumComputation, dest[unsignedLoopVariableValue]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                deregisterControlLineFromPropagationInCurrentScope(quantumComputation, src2[j]);
            }
            createAndAddNotGate(quantumComputation, src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, src2[j]);
            }
        }
        deactivateControlLinePropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::multiplication(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (src1.empty() || dest.empty()) {
            return true;
        }

        if (src1.size() < dest.size() || src2.size() < dest.size()) {
            return false;
        }

        std::vector<qc::Qubit> sum     = dest;
        std::vector<qc::Qubit> partial = src2;

        bool synthesisOk = true;
        activateControlLinePropagationScope();
        registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, src1.front());
        synthesisOk = synthesisOk && bitwiseCnot(quantumComputation, sum, partial);
        deregisterControlLineFromPropagationInCurrentScope(quantumComputation, src1.front());

        for (std::size_t i = 1; i < dest.size() && synthesisOk; ++i) {
            sum.erase(sum.begin());
            partial.pop_back();
            registerControlLineForPropagationInCurrentAndNestedScopes(quantumComputation, src1[i]);
            synthesisOk &= increase(quantumComputation, sum, partial);
            deregisterControlLineFromPropagationInCurrentScope(quantumComputation, src1[i]);
        }
        deactivateControlLinePropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::notEquals(qc::QuantumComputation& quantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (!equals(quantumComputation, dest, src1, src2)) {
            return false;
        }

        createAndAddNotGate(quantumComputation, dest);
        return true;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-noexcept-swap, performance-noexcept-swap, bugprone-exception-escape)
    bool SyrecSynthesis::swap(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2) {
        if (dest2.size() < dest1.size()) {
            return false;
        }

        for (std::size_t i = 0; i < dest1.size(); ++i) {
            createAndAddFredkinGate(quantumComputation, dest1[i], dest2[i]);
        }
        return true;
    }

    //**********************************************************************
    //*****                      Shift Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::leftShift(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2) {
        if (src2 > dest.size()) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        const std::size_t targetLineBaseOffset = src2;
        for (std::size_t i = 0; i < nQubitsShifted; ++i) {
            createAndAddCnotGate(quantumComputation, src1[i], dest[targetLineBaseOffset + i]);
        }
        return true;
    }

    bool SyrecSynthesis::rightShift(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2) {
        if (dest.size() < src2) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        for (std::size_t i = 0; i < nQubitsShifted; ++i) {
            createAndAddCnotGate(quantumComputation, src1[i], dest[i]);
        }
        return true;
    }

    bool SyrecSynthesis::expressionOpInverse([[maybe_unused]] qc::QuantumComputation& quantumComputation, [[maybe_unused]] qc::Qubit op, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs) {
        return true;
    }

    void SyrecSynthesis::getVariables(const VariableAccess::ptr& var, std::vector<qc::Qubit>& lines) {
        const auto&       referenceVariableData           = var->getVar();
        qc::Qubit         offset                          = varLines[referenceVariableData];
        const std::size_t numDeclaredDimensionsOfVariable = referenceVariableData->dimensions.size();

        if (!var->indexes.empty()) {
            // check if it is all numeric_expressions
            if (static_cast<std::size_t>(std::count_if(var->indexes.cbegin(), var->indexes.cend(), [&](const auto& p) { return dynamic_cast<NumericExpression*>(p.get()); })) == numDeclaredDimensionsOfVariable) {
                for (std::size_t i = 0U; i < numDeclaredDimensionsOfVariable; ++i) {
                    const auto evaluatedDimensionIndexValue = dynamic_cast<NumericExpression*>(var->indexes.at(i).get())->value->evaluate(loopMap);
                    qc::Qubit  aggregateValue               = evaluatedDimensionIndexValue;
                    for (std::size_t j = i + 1; j < numDeclaredDimensionsOfVariable; ++j) {
                        aggregateValue *= referenceVariableData->dimensions[i];
                    }
                    offset += aggregateValue * referenceVariableData->bitwidth;
                }
            }
        }

        if (var->range) {
            auto [nfirst, nsecond] = *var->range;

            const qc::Qubit first  = nfirst->evaluate(loopMap);
            const qc::Qubit second = nsecond->evaluate(loopMap);

            if (first < second) {
                for (qc::Qubit i = first; i <= second; ++i) {
                    lines.emplace_back(offset + i);
                }
            } else {
                for (auto i = static_cast<int>(first); i >= static_cast<int>(second); --i) {
                    lines.emplace_back(offset + static_cast<qc::Qubit>(i));
                }
            }
        } else {
            for (qc::Qubit i = 0U; i < referenceVariableData->bitwidth; ++i) {
                lines.emplace_back(offset + i);
            }
        }
    }

    qc::Qubit SyrecSynthesis::getConstantLine(qc::QuantumComputation& quantumComputation, bool value) {
        qc::Qubit constLine = 0U;

        if (!freeConstLinesMap[value].empty()) {
            constLine = freeConstLinesMap[value].back();
            freeConstLinesMap[value].pop_back();
        } else if (!freeConstLinesMap[!value].empty()) {
            constLine = freeConstLinesMap[!value].back();
            freeConstLinesMap[!value].pop_back();
            createAndAddNotGate(quantumComputation, constLine);
        } else {
            //constLine = addLine((std::string("const_") + std::to_string(static_cast<int>(value))), "garbage", value, true);
        }

        return constLine;
    }

    void SyrecSynthesis::getConstantLines(qc::QuantumComputation& quantumComputation, qc::Qubit bitwidth, qc::Qubit value, std::vector<qc::Qubit>& lines) {
        assert(bitwidth <= 32);
        for (qc::Qubit i = 0U; i < bitwidth; ++i) {
            lines.emplace_back(getConstantLine(quantumComputation, (value & (1 << i)) != 0));
        }
    }

    void SyrecSynthesis::addVariable(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& dimensions, const Variable::ptr& var,
                                     bool areVariableLinesConstants, bool areLinesOfVariableGarbageLines, const std::string& arraystr) {
        if (dimensions.empty()) {
            for (qc::Qubit i = 0U; i < var->bitwidth; ++i) {
                const std::string name = var->name + arraystr + "." + std::to_string(i);
                if (areVariableLinesConstants) {
                    quantumComputation.setLogicalQubitAncillary(i);
                }
                if (areLinesOfVariableGarbageLines) {
                    quantumComputation.setLogicalQubitGarbage(i);
                }
            }
        } else {
            const qc::Qubit              len = dimensions.front();
            const std::vector<qc::Qubit> newDimensions(dimensions.begin() + 1U, dimensions.end());

            for (qc::Qubit i = 0U; i < len; ++i) {
                addVariable(quantumComputation, newDimensions, var, areVariableLinesConstants, areLinesOfVariableGarbageLines, arraystr + "[" + std::to_string(i) + "]");
            }
        }
    }

    void SyrecSynthesis::addVariables(qc::QuantumComputation& quantumComputation, const Variable::vec& variables) {
        for (const auto& var: variables) {
            // entry in var lines map
            varLines.try_emplace(var, quantumComputation.getNqubits());

            // types of constant and garbage
            constexpr bool doVariablesLinesStoreConstantValue = false;
            const bool     areLinesOfVariableGarbageLines     = var->type == Variable::In || var->type == Variable::Wire;

            /*const constant constVar = (var->type == Variable::Out || var->type == Variable::Wire) ? constant(false) : constant();
            const bool     garbage  = (var->type == Variable::In || var->type == Variable::Wire);*/

            addVariable(quantumComputation, var->dimensions, var, doVariablesLinesStoreConstantValue, areLinesOfVariableGarbageLines, std::string());
        }
    }

    bool SyrecSynthesis::synthesize(SyrecSynthesis* synthesizer, qc::QuantumComputation& quantumComputation, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
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
        synthesizer->addVariables(quantumComputation, main->parameters);
        synthesizer->addVariables(quantumComputation, main->variables);

        // synthesize the statements
        const auto synthesisOfMainModuleOk = synthesizer->onModule(quantumComputation, main);
        if (statistics) {
            t.stop();
        }
        return synthesisOfMainModuleOk;
    }

    bool SyrecSynthesis::createAndAddNotGate(qc::QuantumComputation& quantumComputation, const qc::Qubit targetQubit) {
        if (!isQubitWithinRange(quantumComputation, targetQubit) || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
            return false;
        }

        const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
        const std::size_t  prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcx(gateControlQubits, targetQubit);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::createAndAddCnotGate(qc::QuantumComputation& quantumComputation, const qc::Qubit controlQubit, const qc::Qubit targetQubit) {
        if (!isQubitWithinRange(quantumComputation, controlQubit) || !isQubitWithinRange(quantumComputation, targetQubit) || controlQubit == targetQubit || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
            return false;
        }

        qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
        gateControlQubits.emplace(controlQubit);

        const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcx(gateControlQubits, targetQubit);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::createAndAddToffoliGate(qc::QuantumComputation& quantumComputation, const qc::Qubit controlQubitOne, const qc::Qubit controlQubitTwo, const qc::Qubit targetQubit) {
        if (!isQubitWithinRange(quantumComputation, controlQubitOne) || !isQubitWithinRange(quantumComputation, controlQubitTwo) || !isQubitWithinRange(quantumComputation, targetQubit) || controlQubitOne == targetQubit || controlQubitTwo == targetQubit || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
            return false;
        }

        qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
        gateControlQubits.emplace(controlQubitOne);
        gateControlQubits.emplace(controlQubitTwo);

        const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcx(gateControlQubits, targetQubit);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::createAndAddMultiControlToffoliGate(qc::QuantumComputation& quantumComputation, const std::unordered_set<qc::Qubit>& controlQubits, const qc::Qubit targetQubit) {
        if (std::any_of(controlQubits.cbegin(), controlQubits.cend(), [&quantumComputation](const qc::Qubit& controlQubit) { return !isQubitWithinRange(quantumComputation, controlQubit); }) || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
            return false;
        }

        qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
        gateControlQubits.insert(controlQubits.cbegin(), controlQubits.cend());

        const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcx(gateControlQubits, targetQubit);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::createAndAddFredkinGate(qc::QuantumComputation& quantumComputation, const qc::Qubit targetQubitOne, const qc::Qubit targetQubitTwo) {
        if (targetQubitOne == targetQubitTwo || aggregateOfPropagatedControlQubits.count(targetQubitOne) != 0 || aggregateOfPropagatedControlQubits.count(targetQubitTwo) != 0) {
            return false;
        }
        const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());

        const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcswap(gateControlQubits, targetQubitOne, targetQubitTwo);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::isQubitWithinRange(const qc::QuantumComputation& quantumComputation, const qc::Qubit qubit) noexcept {
        return qubit < quantumComputation.getNqubits();
    }

    bool SyrecSynthesis::areQubitsWithinRange(const qc::QuantumComputation& quantumComputation, const std::unordered_set<qc::Qubit>& qubitsToCheck) noexcept {
        return std::all_of(qubitsToCheck.cbegin(), qubitsToCheck.cend(), [&](const qc::Qubit qubit) { return isQubitWithinRange(quantumComputation, qubit); });
    }

    void SyrecSynthesis::activateControlLinePropagationScope() {
        controlQubitPropgationScopes.emplace_back();
    }

    void SyrecSynthesis::deactivateControlLinePropagationScope() {
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

    bool SyrecSynthesis::deregisterControlLineFromPropagationInCurrentScope(const qc::QuantumComputation& quantumComputation, const qc::Qubit controlQubit) {
        if (controlQubitPropgationScopes.empty() || !isQubitWithinRange(quantumComputation, controlQubit)) {
            return false;
        }

        auto& localControlLineScope = controlQubitPropgationScopes.back();
        if (localControlLineScope.count(controlQubit) == 0) {
            return false;
        }

        aggregateOfPropagatedControlQubits.erase(controlQubit);
        return true;
    }

    bool SyrecSynthesis::registerControlLineForPropagationInCurrentAndNestedScopes(const qc::QuantumComputation& quantumComputation, const qc::Qubit controlQubit) {
        if (!isQubitWithinRange(quantumComputation, controlQubit)) {
            return false;
        }

        if (controlQubitPropgationScopes.empty()) {
            activateControlLinePropagationScope();
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

    bool SyrecSynthesis::setOrUpdateGlobalGateAnnotation(const std::string_view& key, const std::string& value) {
        auto existingAnnotationForKey = activateGlobalQuantumOperationAnnotations.find(key);
        if (existingAnnotationForKey != activateGlobalQuantumOperationAnnotations.end()) {
            existingAnnotationForKey->second = value;
            return true;
        }
        activateGlobalQuantumOperationAnnotations.emplace(static_cast<std::string>(key), value);
        return false;
    }

    bool SyrecSynthesis::removeGlobalGateAnnotation(const std::string_view& key) {
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

    bool SyrecSynthesis::annotateAllQuantumOperationsAtPositions(std::size_t fromQuantumOperationIndex, std::size_t toQuantumOperationIndex, const GateAnnotationsLookup& userProvidedAnnotationsPerQuantumOperation) {
        if (fromQuantumOperationIndex > userProvidedAnnotationsPerQuantumOperation.size() || fromQuantumOperationIndex > toQuantumOperationIndex) {
            return false;
        }
        annotationsPerQuantumOperation.resize(toQuantumOperationIndex);

        GateAnnotationsLookup gateAnnotations = userProvidedAnnotationsPerQuantumOperation;
        for (const auto& [annotationKey, annotationValue]: activateGlobalQuantumOperationAnnotations) {
            gateAnnotations[annotationKey] = annotationValue;    
        }

        for (std::size_t i = fromQuantumOperationIndex; i <= toQuantumOperationIndex; ++i) {
            annotationsPerQuantumOperation[i] = userProvidedAnnotationsPerQuantumOperation;
        }
        return true;
    }
} // namespace syrec
