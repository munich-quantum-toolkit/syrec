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

    void SyrecSynthesis::addVariables(const Variable::vec& variables) {
        for (const auto& var: variables) {
            // entry in var lines map
            varLines.try_emplace(var, quantumComputation.getNqubits());
            addVariable(var->dimensions, var, std::string());
        }
    }

    bool SyrecSynthesis::synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
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
        synthesizer->addVariables(main->parameters);
        synthesizer->addVariables(main->variables);

        // synthesize the statements
        const auto synthesisOfMainModuleOk = synthesizer->onModule(main);
        for (const auto& ancillaryQubit: synthesizer->getCreatedAncillaryQubits()) {
            synthesizer->quantumComputation.setLogicalQubitAncillary(ancillaryQubit);
        }

        if (statistics) {
            t.stop();
        }
        return synthesisOfMainModuleOk;
    }

    bool SyrecSynthesis::onModule(const Module::ptr& main) {
        bool              synthesisOfModuleStatementOk = true;
        const std::size_t nModuleStatements            = main->statements.size();
        for (std::size_t i = 0; i < nModuleStatements && synthesisOfModuleStatementOk; ++i) {
            synthesisOfModuleStatementOk = processStatement(main->statements[i]);
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

    bool SyrecSynthesis::onStatement(const Statement::ptr& statement) {
        stmts.push(statement);

        setOrUpdateGlobalQuantumOperationAnnotation(GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

        bool okay = true;
        if (auto const* swapStat = dynamic_cast<SwapStatement*>(statement.get())) {
            okay = onStatement(*swapStat);
        } else if (auto const* unaryStat = dynamic_cast<UnaryStatement*>(statement.get())) {
            okay = onStatement(*unaryStat);
        } else if (auto const* assignStat = dynamic_cast<AssignStatement*>(statement.get())) {
            okay = onStatement(*assignStat);
        } else if (auto const* ifStat = dynamic_cast<IfStatement*>(statement.get())) {
            okay = onStatement(*ifStat);
        } else if (auto const* forStat = dynamic_cast<ForStatement*>(statement.get())) {
            okay = onStatement(*forStat);
        } else if (auto const* callStat = dynamic_cast<CallStatement*>(statement.get())) {
            okay = onStatement(*callStat);
        } else if (auto const* uncallStat = dynamic_cast<UncallStatement*>(statement.get())) {
            okay = onStatement(*uncallStat);
        } else if (auto const* skipStat = statement.get()) {
            okay = onStatement(*skipStat);
        } else {
            okay = false;
        }

        stmts.pop();
        return okay;
    }

    bool SyrecSynthesis::onStatement(const SwapStatement& statement) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;

        getVariables(statement.lhs, lhs);
        getVariables(statement.rhs, rhs);

        assert(lhs.size() == rhs.size());

        return swap(lhs, rhs);
    }

    bool SyrecSynthesis::onStatement(const UnaryStatement& statement) {
        // load variable
        std::vector<qc::Qubit> var;
        getVariables(statement.var, var);

        switch (statement.op) {
            case UnaryStatement::Invert:
                return bitwiseNegation(var);
            case UnaryStatement::Increment:
                return increment(var);
            case UnaryStatement::Decrement:
                return decrement(var);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onStatement(const AssignStatement& statement) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;
        std::vector<qc::Qubit> d;

        getVariables(statement.lhs, lhs);
        opRhsLhsExpression(statement.rhs, d);

        bool synthesisOfAssignmentOk = SyrecSynthesis::onExpression(statement.rhs, rhs, lhs, statement.op);
        opVec.clear();

        switch (statement.op) {
            case AssignStatement::Add: {
                synthesisOfAssignmentOk &= assignAdd(lhs, rhs, statement.op);
                break;
            }
            case AssignStatement::Subtract: {
                synthesisOfAssignmentOk &= assignSubtract(lhs, rhs, statement.op);
                break;
            }
            case AssignStatement::Exor: {
                synthesisOfAssignmentOk &= assignExor(lhs, rhs, statement.op);
                break;
            }
            default:
                return false;
        }
        return synthesisOfAssignmentOk;
    }

    bool SyrecSynthesis::onStatement(const IfStatement& statement) {
        // calculate expression
        std::vector<qc::Qubit> expressionResult;

        const bool synthesisOfStatementOk = onExpression(statement.condition, expressionResult, {}, 0U);
        assert(expressionResult.size() == 1U);
        if (!synthesisOfStatementOk) {
            return false;
        }

        // add new helper line
        const qc::Qubit helperLine = expressionResult.front();
        activateControlQubitPropagationScope();
        registerControlQubitForPropagationInCurrentAndNestedScopes(helperLine);

        for (const Statement::ptr& stat: statement.thenStatements) {
            if (!processStatement(stat)) {
                return false;
            }
        }

        // Toggle helper line.
        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        deregisterControlQubitFromPropagationInCurrentScope(helperLine);
        addOperationsImplementingNotGate(helperLine);
        registerControlQubitForPropagationInCurrentAndNestedScopes(helperLine);

        for (const Statement::ptr& stat: statement.elseStatements) {
            if (!processStatement(stat)) {
                return false;
            }
        }

        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        deregisterControlQubitFromPropagationInCurrentScope(helperLine);
        addOperationsImplementingNotGate(helperLine);
        deactivateControlQubitPropagationScope();
        return true;
    }

    bool SyrecSynthesis::onStatement(const ForStatement& statement) {
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
                    if (!processStatement(stat)) {
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
                    if (!processStatement(stat)) {
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

    bool SyrecSynthesis::onStatement(const CallStatement& statement) {
        // 1. Adjust the references module's parameters to the call arguments
        for (qc::Qubit i = 0U; i < statement.parameters.size(); ++i) {
            const std::string&   parameter       = statement.parameters.at(i);
            const Variable::ptr& moduleParameter = statement.target->parameters.at(i);

            moduleParameter->setReference(modules.top()->findParameterOrVariable(parameter));
        }

        // 2. Create new lines for the module's variables
        addVariables(statement.target->variables);

        modules.push(statement.target);
        for (const Statement::ptr& stat: statement.target->statements) {
            if (!processStatement(stat)) {
                return false;
            }
        }
        modules.pop();

        return true;
    }

    bool SyrecSynthesis::onStatement(const UncallStatement& statement) {
        // 1. Adjust the references module's parameters to the call arguments
        for (qc::Qubit i = 0U; i < statement.parameters.size(); ++i) {
            const std::string& parameter       = statement.parameters.at(i);
            const auto&        moduleParameter = statement.target->parameters.at(i);

            moduleParameter->setReference(modules.top()->findParameterOrVariable(parameter));
        }

        // 2. Create new lines for the module's variables
        addVariables(statement.target->variables);

        modules.push(statement.target);

        const auto statements = statement.target->statements;
        for (auto it = statements.rbegin(); it != statements.rend(); ++it) {
            const auto reverseStatement = (*it)->reverse();
            if (!processStatement(reverseStatement)) {
                return false;
            }
        }

        modules.pop();

        return true;
    }

    bool SyrecSynthesis::onStatement(const SkipStatement& statement [[maybe_unused]]) {
        return true;
    }

    bool SyrecSynthesis::onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, qc::Qubit op) {
        if (auto const* numeric = dynamic_cast<NumericExpression*>(expression.get())) {
            return onExpression(*numeric, lines);
        }
        if (auto const* variable = dynamic_cast<VariableExpression*>(expression.get())) {
            return onExpression(*variable, lines);
        }
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return onExpression(*binary, lines, lhsStat, op);
        }
        if (auto const* shift = dynamic_cast<ShiftExpression*>(expression.get())) {
            return onExpression(*shift, lines, lhsStat, op);
        }
        return false;
    }

    bool SyrecSynthesis::onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, qc::Qubit op) {
        std::vector<qc::Qubit> lhs;
        if (!onExpression(expression.lhs, lhs, lhsStat, op)) {
            return false;
        }

        const qc::Qubit rhs = expression.rhs->evaluate(loopMap);
        switch (expression.op) {
            case ShiftExpression::Left: // <<
                getConstantLines(expression.bitwidth(), 0U, lines);
                return leftShift(lines, lhs, rhs);
            case ShiftExpression::Right: // <<
                getConstantLines(expression.bitwidth(), 0U, lines);
                return rightShift(lines, lhs, rhs);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onExpression(const NumericExpression& expression, std::vector<qc::Qubit>& lines) {
        getConstantLines(expression.bitwidth(), expression.value->evaluate(loopMap), lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines) {
        getVariables(expression.var, lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, qc::Qubit op) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;

        if (!onExpression(expression.lhs, lhs, lhsStat, op) || !onExpression(expression.rhs, rhs, lhsStat, op)) {
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
                synthesisOfExprOk = expAdd(expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Subtract: // -
                synthesisOfExprOk = expSubtract(expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Exor: // ^
                synthesisOfExprOk = expExor(expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::Multiply: // *
                getConstantLines(expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = multiplication(lines, lhs, rhs);
                break;
            case BinaryExpression::Divide: // /
                getConstantLines(expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = division(lines, lhs, rhs);
                break;
            case BinaryExpression::Modulo: { // %
                getConstantLines(expression.bitwidth(), 0U, lines);
                std::vector<qc::Qubit> quot;
                getConstantLines(expression.bitwidth(), 0U, quot);

                synthesisOfExprOk = bitwiseCnot(lines, lhs); // duplicate lhs
                synthesisOfExprOk &= modulo(quot, lines, rhs);
            } break;
            case BinaryExpression::LogicalAnd: // &&
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = conjunction(lines.front(), lhs.front(), rhs.front());
                break;
            case BinaryExpression::LogicalOr: // ||
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = disjunction(lines.front(), lhs.front(), rhs.front());
                break;
            case BinaryExpression::BitwiseAnd: // &
                getConstantLines(expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = bitwiseAnd(lines, lhs, rhs);
                break;
            case BinaryExpression::BitwiseOr: // |
                getConstantLines(expression.bitwidth(), 0U, lines);
                synthesisOfExprOk = bitwiseOr(lines, lhs, rhs);
                break;
            case BinaryExpression::LessThan: // <
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = lessThan(lines.front(), lhs, rhs);
                break;
            case BinaryExpression::GreaterThan: // >
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = greaterThan(lines.front(), lhs, rhs);
                break;
            case BinaryExpression::Equals: // =
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = equals(lines.front(), lhs, rhs);
                break;
            case BinaryExpression::NotEquals: // !=
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = notEquals(lines.front(), lhs, rhs);
                break;
            case BinaryExpression::LessEquals: // <=
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = lessEquals(lines.front(), lhs, rhs);
                break;
            case BinaryExpression::GreaterEquals: // >=
                lines.emplace_back(getConstantLine(false));
                synthesisOfExprOk = greaterEquals(lines.front(), lhs, rhs);
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

    bool SyrecSynthesis::bitwiseNegation(const std::vector<qc::Qubit>& dest) {
        for (const auto line: dest) {
            addOperationsImplementingNotGate(line);
        }
        return true;
    }

    bool SyrecSynthesis::decrement(const std::vector<qc::Qubit>& dest) {
        activateControlQubitPropagationScope();
        for (const auto line: dest) {
            addOperationsImplementingNotGate(line);
            registerControlQubitForPropagationInCurrentAndNestedScopes(line);
        }
        deactivateControlQubitPropagationScope();
        return true;
    }

    bool SyrecSynthesis::increment(const std::vector<qc::Qubit>& dest) {
        activateControlQubitPropagationScope();
        for (const auto line: dest) {
            registerControlQubitForPropagationInCurrentAndNestedScopes(line);
        }

        for (int i = static_cast<int>(dest.size()) - 1; i >= 0; --i) {
            deregisterControlQubitFromPropagationInCurrentScope(dest[static_cast<std::size_t>(i)]);
            addOperationsImplementingNotGate(dest[static_cast<std::size_t>(i)]);
        }
        deactivateControlQubitPropagationScope();
        return true;
    }

    //**********************************************************************
    //*****                     Binary Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::bitwiseAnd(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk &= conjunction(dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseCnot(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src) {
        const bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            addOperationsImplementingCnotGate(src[i], dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseOr(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk &= disjunction(dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::conjunction(qc::Qubit dest, qc::Qubit src1, qc::Qubit src2) {
        addOperationsImplementingToffoliGate(src1, src2, dest);
        return true;
    }

    bool SyrecSynthesis::decreaseWithCarry(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry) {
        bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            addOperationsImplementingNotGate(dest[i]);
        }

        synthesisOk &= increaseWithCarry(dest, src, carry);
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            addOperationsImplementingNotGate(dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::disjunction(const qc::Qubit dest, const qc::Qubit src1, const qc::Qubit src2) {
        addOperationsImplementingCnotGate(src1, dest);
        addOperationsImplementingCnotGate(src2, dest);
        addOperationsImplementingToffoliGate(src1, src2, dest);
        return true;
    }

    bool SyrecSynthesis::division(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (!modulo(dest, src1, src2)) {
            return false;
        }

        std::vector<qc::Qubit> sum;
        std::vector<qc::Qubit> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 1; i < src1.size(); ++i) {
            addOperationsImplementingNotGate(src2[i]);
        }

        activateControlQubitPropagationScope();
        for (std::size_t i = 1U; i < src1.size(); ++i) {
            registerControlQubitForPropagationInCurrentAndNestedScopes(src2[i]);
        }

        std::size_t helperIndex = 0;
        bool        synthesisOk = true;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[castedIndex]);
            registerControlQubitForPropagationInCurrentAndNestedScopes(dest[castedIndex]);
            synthesisOk = increase(sum, partial);
            deregisterControlQubitFromPropagationInCurrentScope(dest[castedIndex]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                deregisterControlQubitFromPropagationInCurrentScope(src2[j]);
            }
            addOperationsImplementingNotGate(src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                registerControlQubitForPropagationInCurrentAndNestedScopes(src2[j]);
            }
        }
        deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::equals(const qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (src2.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 0; i < src1.size(); ++i) {
            addOperationsImplementingCnotGate(src2[i], src1[i]);
            addOperationsImplementingNotGate(src1[i]);
        }

        addOperationsImplementingMultiControlToffoliGate(std::unordered_set<qc::Qubit>(src1.begin(), src1.end()), dest);

        for (std::size_t i = 0; i < src1.size(); ++i) {
            addOperationsImplementingCnotGate(src2[i], src1[i]);
            addOperationsImplementingNotGate(src1[i]);
        }
        return true;
    }

    bool SyrecSynthesis::greaterEquals(const qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne) {
        if (!greaterThan(dest, srcOne, srcTwo)) {
            return false;
        }

        addOperationsImplementingNotGate(dest);
        return true;
    }

    bool SyrecSynthesis::greaterThan(const qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        return lessThan(dest, src1, src2);
    }

    bool SyrecSynthesis::increase(const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        if (rhs.empty()) {
            return true;
        }

        if (rhs.size() == 1) {
            addOperationsImplementingCnotGate(lhs.front(), rhs.front());
            return true;
        }

        const std::size_t bitwidth = rhs.size();
        for (std::size_t i = 1; i <= bitwidth - 1; ++i) {
            addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }

        for (std::size_t i = bitwidth - 2; i >= 1; --i) {
            addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }

        for (std::size_t i = 0; i <= bitwidth - 2; ++i) {
            addOperationsImplementingToffoliGate(rhs[i], lhs[i], lhs[i + 1]);
        }

        addOperationsImplementingCnotGate(lhs[bitwidth - 1], rhs[bitwidth - 1]);
        for (std::size_t i = bitwidth - 2; i >= 1; --i) {
            addOperationsImplementingToffoliGate(lhs[i], rhs[i], lhs[i + 1]);
            addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }
        addOperationsImplementingToffoliGate(lhs.front(), rhs.front(), lhs[1]);
        addOperationsImplementingCnotGate(lhs.front(), rhs.front());

        for (std::size_t i = 1; i <= bitwidth - 2; ++i) {
            addOperationsImplementingCnotGate(lhs[i], rhs[i + 1]);
        }

        for (std::size_t i = 1; i <= bitwidth - 1; ++i) {
            addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }
        return true;
    }

    bool SyrecSynthesis::decrease(const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        for (const auto rhsOperandLine: rhs) {
            addOperationsImplementingNotGate(rhsOperandLine);
        }

        if (!increase(rhs, lhs)) {
            return false;
        }

        for (const auto rhsOperandLine: rhs) {
            addOperationsImplementingNotGate(rhsOperandLine);
        }
        return true;
    }

    bool SyrecSynthesis::increaseWithCarry(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry) {
        auto bitwidth = static_cast<int>(src.size());
        if (bitwidth == 0) {
            return true;
        }

        if (src.size() != dest.size()) {
            return false;
        }

        const auto unsignedBitwidth = static_cast<std::size_t>(bitwidth);
        for (std::size_t i = 1U; i < unsignedBitwidth; ++i) {
            addOperationsImplementingCnotGate(src.at(i), dest.at(i));
        }

        if (bitwidth > 1) {
            addOperationsImplementingCnotGate(src.at(unsignedBitwidth - 1), carry);
        }

        for (int i = bitwidth - 2; i > 0; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            addOperationsImplementingCnotGate(src.at(castedIndex), src.at(castedIndex + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth - 1; ++i) {
            addOperationsImplementingToffoliGate(src.at(i), dest.at(i), src.at(i + 1));
        }
        addOperationsImplementingToffoliGate(src.at(unsignedBitwidth - 1), dest.at(unsignedBitwidth - 1), carry);

        for (int i = bitwidth - 1; i > 0; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            addOperationsImplementingCnotGate(src.at(castedIndex), dest.at(castedIndex));
            addOperationsImplementingToffoliGate(dest.at(castedIndex - 1), src.at(castedIndex - 1), src.at(castedIndex));
        }

        for (std::size_t i = 1U; i < unsignedBitwidth - 1; ++i) {
            addOperationsImplementingCnotGate(src.at(i), src.at(i + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth; ++i) {
            addOperationsImplementingCnotGate(src.at(i), dest.at(i));
        }
        return true;
    }

    bool SyrecSynthesis::lessEquals(qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        if (!lessThan(dest, src1, src2)) {
            return false;
        }
        addOperationsImplementingNotGate(dest);
        return true;
    }

    bool SyrecSynthesis::lessThan(qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        return decreaseWithCarry(src1, src2, dest) && increase(src1, src2);
    }

    bool SyrecSynthesis::modulo(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        std::vector<qc::Qubit> sum;
        std::vector<qc::Qubit> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        for (std::size_t i = 1; i < src1.size(); ++i) {
            addOperationsImplementingNotGate(src2[i]);
        }

        activateControlQubitPropagationScope();
        for (std::size_t i = 1; i < src1.size(); ++i) {
            registerControlQubitForPropagationInCurrentAndNestedScopes(src2[i]);
        }

        std::size_t helperIndex = 0;
        bool        synthesisOk = true;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto unsignedLoopVariableValue = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[unsignedLoopVariableValue]);
            synthesisOk = decreaseWithCarry(sum, partial, dest[unsignedLoopVariableValue]);

            registerControlQubitForPropagationInCurrentAndNestedScopes(dest[unsignedLoopVariableValue]);
            synthesisOk &= increase(sum, partial);
            deregisterControlQubitFromPropagationInCurrentScope(dest[unsignedLoopVariableValue]);

            addOperationsImplementingNotGate(dest[unsignedLoopVariableValue]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                deregisterControlQubitFromPropagationInCurrentScope(src2[j]);
            }
            addOperationsImplementingNotGate(src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                registerControlQubitForPropagationInCurrentAndNestedScopes(src2[j]);
            }
        }
        deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::multiplication(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (src1.empty() || dest.empty()) {
            return true;
        }

        if (src1.size() < dest.size() || src2.size() < dest.size()) {
            return false;
        }

        std::vector<qc::Qubit> sum     = dest;
        std::vector<qc::Qubit> partial = src2;

        bool synthesisOk = true;
        activateControlQubitPropagationScope();
        registerControlQubitForPropagationInCurrentAndNestedScopes(src1.front());
        synthesisOk = synthesisOk && bitwiseCnot(sum, partial);
        deregisterControlQubitFromPropagationInCurrentScope(src1.front());

        for (std::size_t i = 1; i < dest.size() && synthesisOk; ++i) {
            sum.erase(sum.begin());
            partial.pop_back();
            registerControlQubitForPropagationInCurrentAndNestedScopes(src1[i]);
            synthesisOk &= increase(sum, partial);
            deregisterControlQubitFromPropagationInCurrentScope(src1[i]);
        }
        deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::notEquals(const qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (!equals(dest, src1, src2)) {
            return false;
        }

        addOperationsImplementingNotGate(dest);
        return true;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-noexcept-swap, performance-noexcept-swap, bugprone-exception-escape)
    bool SyrecSynthesis::swap(const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2) {
        if (dest2.size() < dest1.size()) {
            return false;
        }

        for (std::size_t i = 0; i < dest1.size(); ++i) {
            addOperationsImplementingFredkinGate(dest1[i], dest2[i]);
        }
        return true;
    }

    //**********************************************************************
    //*****                      Shift Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::leftShift(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2) {
        if (src2 > dest.size()) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        const std::size_t targetLineBaseOffset = src2;
        for (std::size_t i = 0; i < nQubitsShifted; ++i) {
            addOperationsImplementingCnotGate(src1[i], dest[targetLineBaseOffset + i]);
        }
        return true;
    }

    bool SyrecSynthesis::rightShift(const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2) {
        if (dest.size() < src2) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        for (std::size_t i = 0; i < nQubitsShifted; ++i) {
            addOperationsImplementingCnotGate(src1[i], dest[i]);
        }
        return true;
    }

    bool SyrecSynthesis::expressionOpInverse([[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs) {
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

    qc::Qubit SyrecSynthesis::getConstantLine(bool value) {
        qc::Qubit constLine = 0U;

        if (!freeConstLinesMap[value].empty()) {
            constLine = freeConstLinesMap[value].back();
            freeConstLinesMap[value].pop_back();
        } else if (!freeConstLinesMap[!value].empty()) {
            constLine = freeConstLinesMap[!value].back();
            freeConstLinesMap[!value].pop_back();
            addOperationsImplementingNotGate(constLine);
        } else {
            const qc::Qubit       qubitIndex = static_cast<qc::Qubit>(quantumComputation.getNqubits());
            const std::string     qubitName  = "const_" + std::to_string(static_cast<int>(value)) + "_qubit_" + std::to_string(qubitIndex);
            constexpr std::size_t qubitSize  = 1;

            quantumComputation.addQubitRegister(qubitSize, qubitName);
            addedAncillaryQubitIndices.emplace(qubitIndex);

            if (value) {
                // Since ancillary qubits are assumed to have an initial value of
                // zero, we need to add an inversion gate to derive the correct
                // initial value of 1.
                // We can either use a simple X quantum operation to initialize the qubit with '1' but we should
                // probably also consider the active control qubits set in the currently active control qubit propagation scopes.
                addOperationsImplementingNotGate(qubitIndex);
            }
            constLine = qubitIndex;
        }
        return constLine;
    }

    void SyrecSynthesis::getConstantLines(unsigned bitwidth, qc::Qubit value, std::vector<qc::Qubit>& lines) {
        assert(bitwidth <= 32);
        for (qc::Qubit i = 0U; i < bitwidth; ++i) {
            lines.emplace_back(getConstantLine((value & (1 << i)) != 0));
        }
    }

    void SyrecSynthesis::addVariable(const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr) {
        if (dimensions.empty()) {
            for (qc::Qubit i = 0U; i < var->bitwidth; ++i) {
                const qc::Qubit       qubitIndex = static_cast<qc::Qubit>(quantumComputation.getNqubits());
                const std::string     qubitName  = var->name + arraystr + "." + std::to_string(i);
                constexpr std::size_t qubitSize  = 1;
                quantumComputation.addQubitRegister(qubitSize, qubitName);
                // TODO: Should we also add classical registers here?

                if (var->type == Variable::In || var->type == Variable::Wire) {
                    quantumComputation.setLogicalQubitGarbage(qubitIndex);
                }
            }
        } else {
            const auto                   len = static_cast<std::size_t>(dimensions.front());
            const std::vector<qc::Qubit> newDimensions(dimensions.begin() + 1U, dimensions.end());

            for (qc::Qubit i = 0U; i < len; ++i) {
                addVariable(newDimensions, var, arraystr + "[" + std::to_string(i) + "]");
            }
        }
    }

    bool SyrecSynthesis::addOperationsImplementingNotGate(const qc::Qubit targetQubit) {
        if (!isQubitWithinRange(quantumComputation, targetQubit) || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
            return false;
        }

        const qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
        const std::size_t  prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcx(gateControlQubits, targetQubit);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::addOperationsImplementingCnotGate(const qc::Qubit controlQubit, const qc::Qubit targetQubit) {
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

    bool SyrecSynthesis::addOperationsImplementingToffoliGate(const qc::Qubit controlQubitOne, const qc::Qubit controlQubitTwo, const qc::Qubit targetQubit) {
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

    bool SyrecSynthesis::addOperationsImplementingMultiControlToffoliGate(const std::unordered_set<qc::Qubit>& controlQubits, const qc::Qubit targetQubit) {
        if (std::any_of(controlQubits.cbegin(), controlQubits.cend(), [&](const qc::Qubit& controlQubit) { return !isQubitWithinRange(quantumComputation, controlQubit); }) || aggregateOfPropagatedControlQubits.count(targetQubit) != 0) {
            return false;
        }

        qc::Controls gateControlQubits(aggregateOfPropagatedControlQubits.cbegin(), aggregateOfPropagatedControlQubits.cend());
        gateControlQubits.insert(controlQubits.cbegin(), controlQubits.cend());

        const std::size_t prevNumQuantumOperations = quantumComputation.getNops();
        quantumComputation.mcx(gateControlQubits, targetQubit);

        const std::size_t currNumQuantumOperations = quantumComputation.getNops();
        return currNumQuantumOperations > prevNumQuantumOperations && annotateAllQuantumOperationsAtPositions(prevNumQuantumOperations + 1, currNumQuantumOperations, {});
    }

    bool SyrecSynthesis::addOperationsImplementingFredkinGate(const qc::Qubit targetQubitOne, const qc::Qubit targetQubitTwo) {
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

    void SyrecSynthesis::activateControlQubitPropagationScope() {
        controlQubitPropgationScopes.emplace_back();
    }

    void SyrecSynthesis::deactivateControlQubitPropagationScope() {
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

    bool SyrecSynthesis::deregisterControlQubitFromPropagationInCurrentScope(const qc::Qubit controlQubit) {
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

    bool SyrecSynthesis::registerControlQubitForPropagationInCurrentAndNestedScopes(const qc::Qubit controlQubit) {
        if (!isQubitWithinRange(quantumComputation, controlQubit)) {
            return false;
        }

        if (controlQubitPropgationScopes.empty()) {
            activateControlQubitPropagationScope();
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

    bool SyrecSynthesis::setOrUpdateGlobalQuantumOperationAnnotation(const std::string_view& key, const std::string& value) {
        auto existingAnnotationForKey = activateGlobalQuantumOperationAnnotations.find(key);
        if (existingAnnotationForKey != activateGlobalQuantumOperationAnnotations.end()) {
            existingAnnotationForKey->second = value;
            return true;
        }
        activateGlobalQuantumOperationAnnotations.emplace(static_cast<std::string>(key), value);
        return false;
    }

    bool SyrecSynthesis::removeGlobalQuantumOperationAnnotation(const std::string_view& key) {
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
