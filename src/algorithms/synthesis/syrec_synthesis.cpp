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

#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stack>
#include <string>
#include <utility>
#include <vector>

namespace {
    /*
     * Prefer the usage of std::chrono::steady_clock instead of std::chrono::system_clock since the former cannot decrease (due to time zone changes, etc.) and is most suitable for measuring intervals according to (https://en.cppreference.com/w/cpp/chrono/steady_clock)
     */
    using TimeStamp = std::chrono::time_point<std::chrono::steady_clock>;
} // namespace

namespace syrec {
    // Helper Functions for the synthesis methods
    SyrecSynthesis::SyrecSynthesis(AnnotatableQuantumComputation& annotatableQuantumComputation):
        annotatableQuantumComputation(annotatableQuantumComputation) {
        freeConstLinesMap.try_emplace(false /* emplacing a default constructed object */);
        freeConstLinesMap.try_emplace(true /* emplacing a default constructed object */);
    }

    void SyrecSynthesis::setMainModule(const Module::ptr& mainModule) {
        assert(modules.empty());
        modules.push(mainModule);
    }

    bool SyrecSynthesis::addVariables(const Variable::vec& variables) {
        bool couldQubitsForVariablesBeAdded = true;
        for (std::size_t i = 0; i < variables.size() && couldQubitsForVariablesBeAdded; ++i) {
            const auto& variable = variables[i];
            // entry in var lines map
            varLines.try_emplace(variable, annotatableQuantumComputation.getNqubits());
            couldQubitsForVariablesBeAdded &= addVariable(annotatableQuantumComputation, variable->dimensions, variable, std::string());
        }
        return couldQubitsForVariablesBeAdded;
    }

    bool SyrecSynthesis::synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        // Settings parsing
        auto mainModule = get<std::string>(settings, "main_module", std::string());
        // Run-time measuring
        const TimeStamp simulationStartTime = std::chrono::steady_clock::now();

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
        if (!synthesizer->addVariables(main->parameters)) {
            std::cerr << "Failed to create qubits for parameters of main module of SyReC program";
            return false;
        }
        if (!synthesizer->addVariables(main->variables)) {
            std::cerr << "Failed to create qubits for local variables of main module of SyReC program";
            return false;
        }

        // synthesize the statements
        const auto synthesisOfMainModuleOk = synthesizer->onModule(main);
        for (const auto& ancillaryQubit: synthesizer->annotatableQuantumComputation.getAddedPreliminaryAncillaryQubitIndices()) {
            if (!synthesizer->annotatableQuantumComputation.promotePreliminaryAncillaryQubitToDefinitiveAncillary(ancillaryQubit)) {
                std::cerr << "Failed to mark qubit" << std::to_string(ancillaryQubit) << " as ancillary qubit";
                return false;
            }
        }

        if (statistics != nullptr) {
            const TimeStamp simulationEndTime = std::chrono::steady_clock::now();
            const auto      simulationRunTime = std::chrono::duration_cast<std::chrono::milliseconds>(simulationEndTime - simulationStartTime);
            statistics->set("runtime", static_cast<double>(simulationRunTime.count()));
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

        annotatableQuantumComputation.setOrUpdateGlobalQuantumOperationAnnotation(GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

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
        return swap(annotatableQuantumComputation, lhs, rhs);
    }

    bool SyrecSynthesis::onStatement(const UnaryStatement& statement) {
        // load variable
        std::vector<qc::Qubit> var;
        getVariables(statement.var, var);

        switch (statement.op) {
            case UnaryStatement::Invert:
                return bitwiseNegation(annotatableQuantumComputation, var);
            case UnaryStatement::Increment:
                return increment(annotatableQuantumComputation, var);
            case UnaryStatement::Decrement:
                return decrement(annotatableQuantumComputation, var);
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
        annotatableQuantumComputation.activateControlQubitPropagationScope();
        annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(helperLine);

        for (const Statement::ptr& stat: statement.thenStatements) {
            if (!processStatement(stat)) {
                return false;
            }
        }

        // Toggle helper line.
        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(helperLine);
        annotatableQuantumComputation.addOperationsImplementingNotGate(helperLine);
        annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(helperLine);

        for (const Statement::ptr& stat: statement.elseStatements) {
            if (!processStatement(stat)) {
                return false;
            }
        }

        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(helperLine);
        annotatableQuantumComputation.addOperationsImplementingNotGate(helperLine);
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
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
        if (!addVariables(statement.target->variables)) {
            return false;
        }

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
        if (!addVariables(statement.target->variables)) {
            return false;
        }

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

    bool SyrecSynthesis::onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op) {
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
        if (auto const* unary = dynamic_cast<UnaryExpression*>(expression.get())) {
            return onExpression(*unary, lines, lhsStat, op);
        }
        return false;
    }

    bool SyrecSynthesis::onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op) {
        std::vector<qc::Qubit> lhs;
        if (!onExpression(expression.lhs, lhs, lhsStat, op)) {
            return false;
        }

        const qc::Qubit rhs = expression.rhs->evaluate(loopMap);
        switch (expression.op) {
            case ShiftExpression::Left: // <<
                return getConstantLines(expression.bitwidth(), 0U, lines) && leftShift(annotatableQuantumComputation, lines, lhs, rhs);
            case ShiftExpression::Right: // <<
                return getConstantLines(expression.bitwidth(), 0U, lines) &&
                       rightShift(annotatableQuantumComputation, lines, lhs, rhs);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onExpression(const UnaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op) {
        std::vector<qc::Qubit> innerExprLines;
        if (!onExpression(expression.expr, innerExprLines, lhsStat, op)) {
            return false;
        }

        if (expression.op == UnaryExpression::LogicalNegation) {
            assert(innerExprLines.size() == 1U);
        }

        const auto innerExprBitwidth = expression.bitwidth();
        bool       synthesisOk       = getConstantLines(innerExprBitwidth, 0U, lines);

        // Transfer result of inner expression lines to ancillaes.
        for (std::size_t i = 0; i < innerExprLines.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(innerExprLines.at(i), lines.at(i));
        }
        return synthesisOk && bitwiseNegation(annotatableQuantumComputation, lines);
    }

    bool SyrecSynthesis::onExpression(const NumericExpression& expression, std::vector<qc::Qubit>& lines) {
        return getConstantLines(expression.bitwidth(), expression.value->evaluate(loopMap), lines);
    }

    bool SyrecSynthesis::onExpression(const VariableExpression& expression, std::vector<qc::Qubit>& lines) {
        getVariables(expression.var, lines);
        return true;
    }

    bool SyrecSynthesis::onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, unsigned op) {
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
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && multiplication(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::Divide: // /
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && division(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::Modulo: { // %
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines);
                std::vector<qc::Qubit> quot;
                synthesisOfExprOk &= getConstantLines(expression.bitwidth(), 0U, quot) && bitwiseCnot(annotatableQuantumComputation, lines, lhs) && modulo(annotatableQuantumComputation, quot, lines, rhs);
            } break;
            case BinaryExpression::LogicalAnd: { // &&
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = conjunction(annotatableQuantumComputation, lines.front(), lhs.front(), rhs.front());
                } else {
                    synthesisOfExprOk = false;
                }
                break;
            }
            case BinaryExpression::LogicalOr: { // ||
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = disjunction(annotatableQuantumComputation, lines.front(), lhs.front(), rhs.front());
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BitwiseAnd: // &
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && bitwiseAnd(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::BitwiseOr: // |
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && bitwiseOr(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::LessThan: { // <
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = lessThan(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::GreaterThan: { // >
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = greaterThan(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::Equals: { // =
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = equals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::NotEquals: { // !=
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = notEquals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::LessEquals: { // <=
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = lessEquals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::GreaterEquals: { // >=
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false);
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = greaterEquals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }
                break;
            }
            default:
                return false;
        }
        return synthesisOfExprOk;
    }

    /// Function when the assignment statements consist of binary expressions and does not include repeated input signals

    //**********************************************************************
    //*****                      Unary Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::bitwiseNegation(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest) {
        bool synthesisOk = true;
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::decrement(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest) {
        annotatableQuantumComputation.activateControlQubitPropagationScope();
        bool synthesisOk = true;
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[i]);
            annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(dest[i]);
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::increment(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest) {
        annotatableQuantumComputation.activateControlQubitPropagationScope();
        for (const auto line: dest) {
            annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(line);
        }

        bool synthesisOk = true;
        for (int i = static_cast<int>(dest.size()) - 1; i >= 0 && synthesisOk; --i) {
            annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(dest[static_cast<std::size_t>(i)]);
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[static_cast<std::size_t>(i)]);
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    //**********************************************************************
    //*****                     Binary Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::bitwiseAnd(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk = conjunction(annotatableQuantumComputation, dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseCnot(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src) {
        const bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            annotatableQuantumComputation.addOperationsImplementingCnotGate(src[i], dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::bitwiseOr(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        bool synthesisOk = src1.size() >= dest.size() && src2.size() >= dest.size();
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk = disjunction(annotatableQuantumComputation, dest[i], src1[i], src2[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::conjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, qc::Qubit src1, qc::Qubit src2) {
        return annotatableQuantumComputation.addOperationsImplementingToffoliGate(src1, src2, dest);
    }

    bool SyrecSynthesis::decreaseWithCarry(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry) {
        bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[i]);
        }

        synthesisOk &= increaseWithCarry(annotatableQuantumComputation, dest, src, carry);
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::disjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit dest, const qc::Qubit src1, const qc::Qubit src2) {
        return annotatableQuantumComputation.addOperationsImplementingCnotGate(src1, dest) && annotatableQuantumComputation.addOperationsImplementingCnotGate(src2, dest) && annotatableQuantumComputation.addOperationsImplementingToffoliGate(src1, src2, dest);
    }

    bool SyrecSynthesis::division(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (!modulo(annotatableQuantumComputation, dest, src1, src2)) {
            return false;
        }

        std::vector<qc::Qubit> sum;
        std::vector<qc::Qubit> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        bool synthesisOk = true;
        for (std::size_t i = 1; i < src1.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(src2[i]);
        }

        annotatableQuantumComputation.activateControlQubitPropagationScope();
        for (std::size_t i = 1U; i < src1.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src2[i]);
        }

        std::size_t helperIndex = 0;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[castedIndex]);
            annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(dest[castedIndex]);
            synthesisOk = increase(annotatableQuantumComputation, sum, partial);
            annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(dest[castedIndex]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src2[j]);
            }
            synthesisOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src2[j]);
            }
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::equals(AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (src2.size() < src1.size()) {
            return false;
        }

        bool synthesisOk = true;
        for (std::size_t i = 0; i < src1.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src2[i], src1[i]) && annotatableQuantumComputation.addOperationsImplementingNotGate(src1[i]);
        }

        synthesisOk &= annotatableQuantumComputation.addOperationsImplementingMultiControlToffoliGate(qc::Controls(src1.begin(), src1.end()), dest);

        for (std::size_t i = 0; i < src1.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src2[i], src1[i]) && annotatableQuantumComputation.addOperationsImplementingNotGate(src1[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::greaterEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& srcTwo, const std::vector<qc::Qubit>& srcOne) {
        return greaterThan(annotatableQuantumComputation, dest, srcOne, srcTwo) && annotatableQuantumComputation.addOperationsImplementingNotGate(dest);
    }

    bool SyrecSynthesis::greaterThan(AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        return lessThan(annotatableQuantumComputation, dest, src1, src2);
    }

    bool SyrecSynthesis::increase(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        if (rhs.empty()) {
            return true;
        }

        bool synthesisOk = true;
        if (rhs.size() == 1) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs.front(), rhs.front());
            return synthesisOk;
        }

        const std::size_t bitwidth = rhs.size();
        for (std::size_t i = 1; i <= bitwidth - 1 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }

        for (std::size_t i = bitwidth - 2; i >= 1 && synthesisOk; --i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }

        for (std::size_t i = 0; i <= bitwidth - 2 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingToffoliGate(rhs[i], lhs[i], lhs[i + 1]);
        }

        synthesisOk &= annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs[bitwidth - 1], rhs[bitwidth - 1]);
        for (std::size_t i = bitwidth - 2; i >= 1 && synthesisOk; --i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingToffoliGate(lhs[i], rhs[i], lhs[i + 1]) && annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }
        synthesisOk &= annotatableQuantumComputation.addOperationsImplementingToffoliGate(lhs.front(), rhs.front(), lhs[1]) && annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs.front(), rhs.front());

        for (std::size_t i = 1; i <= bitwidth - 2 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs[i], rhs[i + 1]);
        }

        for (std::size_t i = 1; i <= bitwidth - 1 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(lhs[i], rhs[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::decrease(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        bool synthesisOk = true;
        for (std::size_t i = 0; i < rhs.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(rhs[i]);
        }
        synthesisOk &= increase(annotatableQuantumComputation, rhs, lhs);
        for (std::size_t i = 0; i < rhs.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(rhs[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::increaseWithCarry(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src, qc::Qubit carry) {
        auto bitwidth = static_cast<int>(src.size());
        if (bitwidth == 0) {
            return true;
        }

        if (src.size() != dest.size()) {
            return false;
        }

        bool       synthesisOk      = true;
        const auto unsignedBitwidth = static_cast<std::size_t>(bitwidth);
        for (std::size_t i = 1U; i < unsignedBitwidth && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src.at(i), dest.at(i));
        }

        if (bitwidth > 1) {
            synthesisOk &= annotatableQuantumComputation.addOperationsImplementingCnotGate(src.at(unsignedBitwidth - 1), carry);
        }

        for (int i = bitwidth - 2; i > 0 && synthesisOk; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            synthesisOk            = annotatableQuantumComputation.addOperationsImplementingCnotGate(src.at(castedIndex), src.at(castedIndex + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth - 1 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingToffoliGate(src.at(i), dest.at(i), src.at(i + 1));
        }
        synthesisOk &= annotatableQuantumComputation.addOperationsImplementingToffoliGate(src.at(unsignedBitwidth - 1), dest.at(unsignedBitwidth - 1), carry);

        for (int i = bitwidth - 1; i > 0 && synthesisOk; --i) {
            const auto castedIndex = static_cast<std::size_t>(i);
            synthesisOk            = annotatableQuantumComputation.addOperationsImplementingCnotGate(src.at(castedIndex), dest.at(castedIndex)) && annotatableQuantumComputation.addOperationsImplementingToffoliGate(dest.at(castedIndex - 1), src.at(castedIndex - 1), src.at(castedIndex));
        }

        for (std::size_t i = 1U; i < unsignedBitwidth - 1 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src.at(i), src.at(i + 1));
        }

        for (std::size_t i = 0U; i < unsignedBitwidth && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src.at(i), dest.at(i));
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::lessEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        return lessThan(annotatableQuantumComputation, dest, src1, src2) && annotatableQuantumComputation.addOperationsImplementingNotGate(dest);
    }

    bool SyrecSynthesis::lessThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        return decreaseWithCarry(annotatableQuantumComputation, src1, src2, dest) && increase(annotatableQuantumComputation, src1, src2);
    }

    bool SyrecSynthesis::modulo(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        std::vector<qc::Qubit> sum;
        std::vector<qc::Qubit> partial;

        if (src2.size() < src1.size() || dest.size() < src1.size()) {
            return false;
        }

        bool synthesisOk = true;
        for (std::size_t i = 1; i < src1.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(src2[i]);
        }

        annotatableQuantumComputation.activateControlQubitPropagationScope();
        for (std::size_t i = 1; i < src1.size() && synthesisOk; ++i) {
            annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src2[i]);
        }

        std::size_t helperIndex = 0;
        for (int i = static_cast<int>(src1.size()) - 1; i >= 0 && synthesisOk; --i) {
            const auto unsignedLoopVariableValue = static_cast<std::size_t>(i);

            partial.push_back(src2[helperIndex++]);
            sum.insert(sum.begin(), src1[unsignedLoopVariableValue]);
            synthesisOk = decreaseWithCarry(annotatableQuantumComputation, sum, partial, dest[unsignedLoopVariableValue]);

            annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(dest[unsignedLoopVariableValue]);
            synthesisOk &= increase(annotatableQuantumComputation, sum, partial);
            annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(dest[unsignedLoopVariableValue]);

            synthesisOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(dest[unsignedLoopVariableValue]);
            if (i == 0) {
                continue;
            }

            for (std::size_t j = 1; j < src1.size() && synthesisOk; ++j) {
                annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src2[j]);
            }
            synthesisOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(src2[helperIndex]);

            for (std::size_t j = 2; j < src1.size() && synthesisOk; ++j) {
                annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src2[j]);
            }
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::multiplication(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        if (src1.empty() || dest.empty()) {
            return true;
        }

        if (src1.size() < dest.size() || src2.size() < dest.size()) {
            return false;
        }

        std::vector<qc::Qubit> sum     = dest;
        std::vector<qc::Qubit> partial = src2;

        annotatableQuantumComputation.activateControlQubitPropagationScope();
        annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src1.front());
        bool synthesisOk = bitwiseCnot(annotatableQuantumComputation, sum, partial);
        annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src1.front());

        for (std::size_t i = 1; i < dest.size() && synthesisOk; ++i) {
            sum.erase(sum.begin());
            partial.pop_back();
            annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src1[i]);
            synthesisOk = increase(annotatableQuantumComputation, sum, partial);
            annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src1[i]);
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::notEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        return equals(annotatableQuantumComputation, dest, src1, src2) && annotatableQuantumComputation.addOperationsImplementingNotGate(dest);
    }

    // NOLINTNEXTLINE(cppcoreguidelines-noexcept-swap, performance-noexcept-swap, bugprone-exception-escape)
    bool SyrecSynthesis::swap(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest1, const std::vector<qc::Qubit>& dest2) {
        bool synthesisOk = dest2.size() >= dest1.size();
        for (std::size_t i = 0; i < dest1.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingFredkinGate(dest1[i], dest2[i]);
        }
        return synthesisOk;
    }

    //**********************************************************************
    //*****                      Shift Operations                      *****
    //**********************************************************************

    bool SyrecSynthesis::leftShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2) {
        if (src2 > dest.size()) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        bool              synthesisOk          = true;
        const std::size_t targetLineBaseOffset = src2;
        for (std::size_t i = 0; i < nQubitsShifted && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src1[i], dest[targetLineBaseOffset + i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::rightShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& src1, qc::Qubit src2) {
        if (dest.size() < src2) {
            return false;
        }

        const std::size_t nQubitsShifted = dest.size() - src2;
        if (src1.size() < nQubitsShifted) {
            return false;
        }

        bool synthesisOk = true;
        for (std::size_t i = 0; i < nQubitsShifted && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src1[i], dest[i]);
        }
        return synthesisOk;
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

    std::optional<qc::Qubit> SyrecSynthesis::getConstantLine(bool value) {
        qc::Qubit constLine = 0U;

        if (!freeConstLinesMap[value].empty()) {
            constLine = freeConstLinesMap[value].back();
            freeConstLinesMap[value].pop_back();
        } else if (!freeConstLinesMap[!value].empty()) {
            constLine = freeConstLinesMap[!value].back();
            freeConstLinesMap[!value].pop_back();
            annotatableQuantumComputation.addOperationsImplementingNotGate(constLine);
        } else {
            const auto                     qubitIndex          = static_cast<qc::Qubit>(annotatableQuantumComputation.getNqubits());
            const std::string              qubitLabel          = "q_" + std::to_string(qubitIndex) + "_const_" + std::to_string(static_cast<int>(value));
            const std::optional<qc::Qubit> generatedQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit(qubitLabel, value);
            if (!generatedQubitIndex.has_value() || *generatedQubitIndex != qubitIndex) {
                return std::nullopt;
            }
            constLine = qubitIndex;
        }
        return constLine;
    }

    bool SyrecSynthesis::getConstantLines(unsigned bitwidth, qc::Qubit value, std::vector<qc::Qubit>& lines) {
        assert(bitwidth <= 32);

        bool couldQubitsForConstantLinesBeFetched = true;
        for (qc::Qubit i = 0U; i < bitwidth && couldQubitsForConstantLinesBeFetched; ++i) {
            const std::optional<qc::Qubit> ancillaryQubitIndex = getConstantLine((value & (1 << i)) != 0);
            if (ancillaryQubitIndex.has_value()) {
                lines.emplace_back(*ancillaryQubitIndex);
            } else {
                couldQubitsForConstantLinesBeFetched = false;
            }
        }
        return couldQubitsForConstantLinesBeFetched;
    }

    bool SyrecSynthesis::addVariable(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr) {
        bool couldQubitsForVariableBeAdded = true;
        if (dimensions.empty()) {
            for (qc::Qubit i = 0U; i < var->bitwidth && couldQubitsForVariableBeAdded; ++i) {
                const std::string qubitLabel     = var->name + arraystr + "." + std::to_string(i);
                const bool        isGarbageQubit = var->type == Variable::In || var->type == Variable::Wire;
                couldQubitsForVariableBeAdded &= annotatableQuantumComputation.addNonAncillaryQubit(qubitLabel, isGarbageQubit).has_value();
            }
        } else {
            const auto                   len = static_cast<std::size_t>(dimensions.front());
            const std::vector<qc::Qubit> newDimensions(dimensions.begin() + 1U, dimensions.end());

            for (qc::Qubit i = 0U; i < len && couldQubitsForVariableBeAdded; ++i) {
                couldQubitsForVariableBeAdded &= addVariable(annotatableQuantumComputation, newDimensions, var, arraystr + "[" + std::to_string(i) + "]");
            }
        }
        return couldQubitsForVariableBeAdded;
    }
} // namespace syrec
