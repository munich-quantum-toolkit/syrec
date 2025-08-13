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

#include "algorithms/synthesis/internal_qubit_label_builder.hpp"
#include "algorithms/synthesis/statement_execution_order_stack.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/qubit_inlining_stack.hpp"
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
#include <iterator>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
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
        statementExecutionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    }

    void SyrecSynthesis::setMainModule(const Module::ptr& mainModule) {
        assert(modules.empty());
        modules.push(mainModule);
    }

    bool SyrecSynthesis::addVariables(const Variable::vec& variables) {
        // We only want to record inlining information for qubits that are actually inlined (i.e. variables of type 'wire' and 'state').
        // Note that all variables added in this call shared the same inlining stack thus we reuse the latter when adding the former.
        const bool                                   isAnyVarALocalModuleVarBasedOnVarType = std::any_of(variables.cbegin(), variables.cend(), [](const Variable::ptr& variable) { return variable->type == Variable::Type::Wire || variable->type == Variable::Type::State; });
        const std::optional<QubitInliningStack::ptr> inlineStack                           = isAnyVarALocalModuleVarBasedOnVarType ? getLastCreatedModuleCallStackInstance() : std::nullopt;
        bool                                         couldQubitsForVariablesBeAdded        = shouldQubitInlineInformationBeRecorded() && isAnyVarALocalModuleVarBasedOnVarType ? inlineStack.has_value() : true;

        for (std::size_t i = 0; i < variables.size() && couldQubitsForVariablesBeAdded; ++i) {
            const auto& variable = variables[i];
            // entry in var lines map
            if (!varLines.try_emplace(variable, annotatableQuantumComputation.getNqubits()).second) {
                std::cerr << "Tried to add duplicate variable with identifier " << variable->name << " to internal lookup\n";
                return false;
            }
            couldQubitsForVariablesBeAdded &= addVariable(annotatableQuantumComputation, variable->dimensions, variable, std::string(), inlineStack);
        }
        return couldQubitsForVariablesBeAdded;
    }

    bool SyrecSynthesis::synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        if (synthesizer->statementExecutionOrderStack->getCurrentAggregateStatementExecutionOrderState() != StatementExecutionOrderStack::StatementExecutionOrder::Sequential) {
            std::cerr << "Execution order at start of synthesis should be sequential\n";
            return false;
        }

        // Settings parsing
        const auto& expectedMainModuleIdentifier = settings != nullptr ? settings->get<std::string>(MAIN_MODULE_IDENTIFIER_CONFIG_KEY, "") : "";
        // Run-time measuring
        const TimeStamp simulationStartTime = std::chrono::steady_clock::now();

        // get the main module
        Module::ptr main;

        if (!expectedMainModuleIdentifier.empty()) {
            main = program.findModule(expectedMainModuleIdentifier);
            if (!main) {
                std::cerr << "Program has no module: " << expectedMainModuleIdentifier << "\n";
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
        if (settings != nullptr && settings->get<bool>(GENERATE_INLINE_DEBUG_INFORMATION_CONFIG_KEY, false)) {
            auto mainModuleCallStackEntry         = QubitInliningStack::QubitInliningStackEntry();
            mainModuleCallStackEntry.targetModule = main;

            auto mainModuleInlineStack = std::make_shared<QubitInliningStack>();
            mainModuleInlineStack->push(mainModuleCallStackEntry);

            synthesizer->moduleCallStackInstances = std::vector<QubitInliningStack::ptr>();
            synthesizer->moduleCallStackInstances->emplace_back(mainModuleInlineStack);
        }

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
            for (std::size_t j = i + 1; j < checkRhsVec.size() && !foundRepeat; ++j) {
                foundRepeat = checkRhsVec[i] == checkRhsVec[j];
            }
            for (std::size_t k = 0; k < checkLhsVec.size() && !foundRepeat; ++k) {
                foundRepeat = checkLhsVec[k] == checkRhsVec[i];
            }
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
        if (auto const* swapStat = dynamic_cast<SwapStatement*>(statement.get()); swapStat != nullptr) {
            okay = onStatement(*swapStat);
        } else if (auto const* unaryStat = dynamic_cast<UnaryStatement*>(statement.get()); unaryStat != nullptr) {
            okay = onStatement(*unaryStat);
        } else if (auto const* assignStat = dynamic_cast<AssignStatement*>(statement.get()); assignStat != nullptr) {
            okay = onStatement(*assignStat);
        } else if (auto const* ifStat = dynamic_cast<IfStatement*>(statement.get()); ifStat != nullptr) {
            okay = onStatement(*ifStat);
        } else if (auto const* forStat = dynamic_cast<ForStatement*>(statement.get()); forStat != nullptr) {
            okay = onStatement(*forStat);
        } else if (auto const* callStat = dynamic_cast<CallStatement*>(statement.get()); callStat != nullptr) {
            if (!shouldQubitInlineInformationBeRecorded()) {
                okay = onStatement(*callStat);
            } else {
                const std::optional<QubitInliningStack::ptr> lastCreatedQubitInlineStack = getLastCreatedModuleCallStackInstance();
                // Our goal is to shared the current qubit inline stack for all qubits created for the local variables of the currently processed module as well as for all ancillary qubits generated while
                // synthesizing the statements of the current module, thus we proceed as follows:
                // I.   Create a copy of the current qubit inline stack
                // II.  Push a new entry on the inline stack for the new called module and synthesize the statements of said module with the new call stack instance created in I.
                // III. Discard the call stack instance of II. so the call stack prior to I. can be reused again for the remainder of the statements of the parent module that contained the currently processed CallStatement.
                if (const std::optional<QubitInliningStack::ptr> optionalCopyOfLastCreatedQubitInlineStack = lastCreatedQubitInlineStack.has_value() ? createInsertAndGetCopyOfLastCreatedCallStackInstance() : std::nullopt; optionalCopyOfLastCreatedQubitInlineStack.has_value() && optionalCopyOfLastCreatedQubitInlineStack->get()->size() > 0) {
                    const QubitInliningStack::ptr& copyOfLastCreatedQubitInlineStack = *optionalCopyOfLastCreatedQubitInlineStack;
                    if (auto* lastPushedEntryOnInlineStack = copyOfLastCreatedQubitInlineStack->getStackEntryAt(lastCreatedQubitInlineStack.value()->size() - 1); lastPushedEntryOnInlineStack != nullptr) {
                        lastPushedEntryOnInlineStack->lineNumberOfCallOfTargetModule    = statement->lineNumber;
                        lastPushedEntryOnInlineStack->isTargetModuleAccessedViaCallStmt = true;
                        okay                                                            = copyOfLastCreatedQubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, callStat->target})) && onStatement(*callStat);
                    } else {
                        // There must be at least one entry on the stack for the main module of the currently synthesized SyReC program
                        okay = false;
                    }
                    discardLastCreateModuleCallStackInstance();
                } else {
                    // There must be at least one entry on the stack for the main module of the currently synthesized SyReC program
                    okay = false;
                }
            }
        } else if (auto const* uncallStat = dynamic_cast<UncallStatement*>(statement.get()); uncallStat != nullptr) {
            if (!shouldQubitInlineInformationBeRecorded()) {
                okay = onStatement(*uncallStat);
            } else {
                const std::optional<QubitInliningStack::ptr> lastCreatedQubitInlineStack = getLastCreatedModuleCallStackInstance();
                // The same logic applied for the CallStatement regarding the reuse of CallStack instances also applies to the handling of UncallStatements (for further details check the comment defined for the handling of the CallStatement)
                if (const std::optional<QubitInliningStack::ptr> optionalCopyOfLastCreatedQubitInlineStack = lastCreatedQubitInlineStack.has_value() ? createInsertAndGetCopyOfLastCreatedCallStackInstance() : std::nullopt; optionalCopyOfLastCreatedQubitInlineStack.has_value() && optionalCopyOfLastCreatedQubitInlineStack->get()->size() > 0) {
                    const QubitInliningStack::ptr& copyOfLastCreatedQubitInlineStack = *optionalCopyOfLastCreatedQubitInlineStack;
                    if (auto* lastPushedEntryOnInlineStack = copyOfLastCreatedQubitInlineStack->getStackEntryAt(lastCreatedQubitInlineStack.value()->size() - 1); lastPushedEntryOnInlineStack != nullptr) {
                        lastPushedEntryOnInlineStack->lineNumberOfCallOfTargetModule    = statement->lineNumber;
                        lastPushedEntryOnInlineStack->isTargetModuleAccessedViaCallStmt = false;
                        okay                                                            = copyOfLastCreatedQubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({std::nullopt, std::nullopt, uncallStat->target})) && onStatement(*uncallStat);
                    } else {
                        // There must be at least one entry on the stack for the main module of the currently synthesized SyReC program
                        okay = false;
                    }
                    discardLastCreateModuleCallStackInstance();
                } else {
                    // There must be at least one entry on the stack for the main module of the currently synthesized SyReC program
                    okay = false;
                }
            }
        } else if (auto const* skipStat = dynamic_cast<SkipStatement*>(statement.get()); skipStat != nullptr) {
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

        const bool synthesisOk = getVariables(statement.lhs, lhs) && getVariables(statement.rhs, rhs);
        assert(lhs.size() == rhs.size());
        return synthesisOk && swap(annotatableQuantumComputation, lhs, rhs);
    }

    bool SyrecSynthesis::onStatement(const UnaryStatement& statement) {
        // load variable
        std::vector<qc::Qubit> var;
        bool                   synthesisOk = getVariables(statement.var, var);

        switch (statement.unaryOperation) {
            case UnaryStatement::UnaryOperation::Invert:
                synthesisOk &= bitwiseNegation(annotatableQuantumComputation, var);
                break;
            case UnaryStatement::UnaryOperation::Increment:
                synthesisOk &= increment(annotatableQuantumComputation, var);
                break;
            case UnaryStatement::UnaryOperation::Decrement:
                synthesisOk &= decrement(annotatableQuantumComputation, var);
                break;
            default:
                synthesisOk = false;
                break;
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::onStatement(const AssignStatement& statement) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;
        std::vector<qc::Qubit> d;

        bool synthesisOfAssignmentOk = getVariables(statement.lhs, lhs);
        // While a derviced class can fall back to the base class implementation to synthesis AssignStatements, the opRhsLhsExpression(...) call
        // of the derived class might not be able to handle the expression on the right-hand side of the assignment but since we are already using the base class
        // to synthesis the assignment (which should be able to handle all SyReC expression types) the return value of opRhsLhsExpression can be ignored.
        opRhsLhsExpression(statement.rhs, d);
        synthesisOfAssignmentOk &= SyrecSynthesis::onExpression(statement.rhs, rhs, lhs, statement.assignOperation);
        opVec.clear();

        switch (statement.assignOperation) {
            case AssignStatement::AssignOperation::Add: {
                synthesisOfAssignmentOk &= assignAdd(lhs, rhs, statement.assignOperation);
                break;
            }
            case AssignStatement::AssignOperation::Subtract: {
                synthesisOfAssignmentOk &= assignSubtract(lhs, rhs, statement.assignOperation);
                break;
            }
            case AssignStatement::AssignOperation::Exor: {
                synthesisOfAssignmentOk &= assignExor(lhs, rhs, statement.assignOperation);
                break;
            }
            default:
                return false;
        }
        return synthesisOfAssignmentOk;
    }

    bool SyrecSynthesis::onStatement(const IfStatement& statement) {
        OperationVariant guardExpressionTopLevelOperation = BinaryExpression::BinaryOperation::Add;
        if (auto const* binary = dynamic_cast<BinaryExpression*>(statement.condition.get()); binary != nullptr) {
            guardExpressionTopLevelOperation = binary->binaryOperation;
        } else if (auto const* shift = dynamic_cast<ShiftExpression*>(statement.condition.get()); shift != nullptr) {
            guardExpressionTopLevelOperation = shift->shiftOperation;
        } else if (auto const* unary = dynamic_cast<UnaryExpression*>(statement.condition.get()); unary != nullptr) {
            guardExpressionTopLevelOperation = unary->unaryOperation;
        }

        // calculate expression
        std::vector<qc::Qubit> guardExpressionQubits;
        bool                   synthesisOfGuardExprOk = onExpression(statement.condition, guardExpressionQubits, {}, guardExpressionTopLevelOperation);
        assert(guardExpressionQubits.size() == 1U);

        // We need to create the ancillary qubit used to store the synthesis result of the variable expression since the onExpression(...) function does not create this ancillary qubit
        // Additionally, a CNOT gate is required to transfer the value of the current qubit storing the synthesis result of the VariableExpression to the ancillary qubit.
        // The ancillary qubit is only required when the original qubit of the guard expression is used as a target qubit in any of the statements of the true
        // or false branch of the IfStatement but since we cannot determine whether this case will happen (at this point of the synthesis) we are 'forced' to use the ancillary qubit.
        if (auto const* variableExpr = dynamic_cast<VariableExpression*>(statement.condition.get()); variableExpr != nullptr && synthesisOfGuardExprOk) {
            if (const std::optional<qc::Qubit> generatedHelperLine = getConstantLine(false, getLastCreatedModuleCallStackInstance()); generatedHelperLine.has_value()) {
                synthesisOfGuardExprOk   = annotatableQuantumComputation.addOperationsImplementingCnotGate(guardExpressionQubits.front(), *generatedHelperLine);
                guardExpressionQubits[0] = *generatedHelperLine;
            } else {
                synthesisOfGuardExprOk = false;
            }
        }

        if (!synthesisOfGuardExprOk) {
            return false;
        }

        // add new helper line
        const qc::Qubit guardExpressionQubit = guardExpressionQubits.front();
        annotatableQuantumComputation.activateControlQubitPropagationScope();
        bool synthesisOfBranchStatementsOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(guardExpressionQubit) && std::all_of(statement.thenStatements.cbegin(), statement.thenStatements.cend(), [&](const Statement::ptr& trueBranchStatement) { return processStatement(trueBranchStatement); });

        // Toggle helper line.
        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        synthesisOfBranchStatementsOk &= annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(guardExpressionQubit) && annotatableQuantumComputation.addOperationsImplementingNotGate(guardExpressionQubit) && annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(guardExpressionQubit) && std::all_of(statement.elseStatements.cbegin(), statement.elseStatements.cend(), [&](const Statement::ptr& falseBranchStatement) { return processStatement(falseBranchStatement); });

        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        synthesisOfBranchStatementsOk &= annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(guardExpressionQubit) && annotatableQuantumComputation.addOperationsImplementingNotGate(guardExpressionQubit);
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOfBranchStatementsOk;
    }

    bool SyrecSynthesis::onStatement(const ForStatement& statement) {
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
        return synthesizeModuleCall(&statement);
    }

    bool SyrecSynthesis::onStatement(const UncallStatement& statement) {
        return synthesizeModuleCall(&statement);
    }

    bool SyrecSynthesis::onStatement(const SkipStatement& statement [[maybe_unused]]) {
        return true;
    }

    bool SyrecSynthesis::onExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, const OperationVariant operationVariant) {
        if (auto const* numeric = dynamic_cast<NumericExpression*>(expression.get()); numeric != nullptr) {
            return onExpression(*numeric, lines);
        }
        if (auto const* variable = dynamic_cast<VariableExpression*>(expression.get()); variable != nullptr) {
            return onExpression(*variable, lines);
        }
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get()); binary != nullptr) {
            return onExpression(*binary, lines, lhsStat, operationVariant);
        }
        if (auto const* shift = dynamic_cast<ShiftExpression*>(expression.get()); shift != nullptr) {
            return onExpression(*shift, lines, lhsStat, operationVariant);
        }
        if (auto const* unary = dynamic_cast<UnaryExpression*>(expression.get()); unary != nullptr) {
            return onExpression(*unary, lines, lhsStat, operationVariant);
        }
        return false;
    }

    bool SyrecSynthesis::onExpression(const ShiftExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, const OperationVariant operationVariant) {
        std::vector<qc::Qubit> lhs;
        if (!onExpression(expression.lhs, lhs, lhsStat, operationVariant)) {
            return false;
        }

        const unsigned qubitIndexShiftAmount = expression.rhs->evaluate(loopMap);
        switch (expression.shiftOperation) {
            case ShiftExpression::ShiftOperation::Left: // <<
                return getConstantLines(expression.bitwidth(), 0U, lines) && leftShift(annotatableQuantumComputation, lines, lhs, qubitIndexShiftAmount);
            case ShiftExpression::ShiftOperation::Right: // <<
                return getConstantLines(expression.bitwidth(), 0U, lines) &&
                       rightShift(annotatableQuantumComputation, lines, lhs, qubitIndexShiftAmount);
            default:
                return false;
        }
    }

    bool SyrecSynthesis::onExpression(const UnaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, const OperationVariant operationVariant) {
        std::vector<qc::Qubit> innerExprLines;
        if (!onExpression(expression.expr, innerExprLines, lhsStat, operationVariant)) {
            return false;
        }

        if (expression.unaryOperation == UnaryExpression::UnaryOperation::LogicalNegation && innerExprLines.size() != 1) {
            std::cerr << "Logical negation operation can only be used for expressions with a bitwidth of 1\n";
            return false;
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
        return getVariables(expression.var, lines);
    }

    bool SyrecSynthesis::onExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& lines, std::vector<qc::Qubit> const& lhsStat, const OperationVariant operationVariant) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;

        if (!onExpression(expression.lhs, lhs, lhsStat, operationVariant) || !onExpression(expression.rhs, rhs, lhsStat, operationVariant)) {
            return false;
        }

        expLhss.push(lhs);
        expRhss.push(rhs);
        expOpp.push(expression.binaryOperation);

        // The previous implementation used unscoped enum declarations for both the operations of a BinaryExpression as well as for an AssignStatement.
        // Additionally, the expOpp and opVec data structures used to store both types of operations as unsigned integers (with unscoped enums being implicitly convertible to unsigned integers)
        // thus the comparison between the elements was possible. Since we are now storing the scoped enum values instead, we need to separately handle binary and assignment operations when
        // comparing the two types with the latter requiring a conversion to determine its matching binary operation counterpart. While the scoped enum values can be converted to their underlying
        // numeric data type (or any other type), they require an explicit cast instead.
        if (expOpp.size() == opVec.size()) {
            if (std::holds_alternative<BinaryExpression::BinaryOperation>(operationVariant) && expOpp.top() == std::get<BinaryExpression::BinaryOperation>(operationVariant)) {
                return true;
            }
            if (std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperationFromAssignmentOperation = std::holds_alternative<AssignStatement::AssignOperation>(operationVariant) ? tryMapAssignmentToBinaryOperation(std::get<AssignStatement::AssignOperation>(operationVariant)) : std::nullopt; mappedToBinaryOperationFromAssignmentOperation.has_value() && expOpp.top() == *mappedToBinaryOperationFromAssignmentOperation) {
                return true;
            }
        }

        bool synthesisOfExprOk = true;
        switch (expression.binaryOperation) {
            case BinaryExpression::BinaryOperation::Add: // +
                synthesisOfExprOk = expAdd(expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::BinaryOperation::Subtract: // -
                synthesisOfExprOk = expSubtract(expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::BinaryOperation::Exor: // ^
                synthesisOfExprOk = expExor(expression.bitwidth(), lines, lhs, rhs);
                break;
            case BinaryExpression::BinaryOperation::Multiply: // *
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && multiplication(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::BinaryOperation::Divide: { // /
                std::vector<qc::Qubit>  remainder;
                std::vector<qc::Qubit>& quotient = lines;
                synthesisOfExprOk                = getConstantLines(expression.bitwidth(), 0U, remainder) && getConstantLines(expression.bitwidth(), 0U, quotient) && division(annotatableQuantumComputation, lhs, rhs, quotient, remainder);
                break;
            }
            case BinaryExpression::BinaryOperation::Modulo: { // %
                std::vector<qc::Qubit>& remainder = lines;
                std::vector<qc::Qubit>  quotient;
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, remainder) && getConstantLines(expression.bitwidth(), 0U, quotient) && modulo(annotatableQuantumComputation, lhs, rhs, quotient, remainder);
                break;
            }
            case BinaryExpression::BinaryOperation::LogicalAnd: { // &&
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = conjunction(annotatableQuantumComputation, lines.front(), lhs.front(), rhs.front());
                } else {
                    synthesisOfExprOk = false;
                }
                break;
            }
            case BinaryExpression::BinaryOperation::LogicalOr: { // ||
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = disjunction(annotatableQuantumComputation, lines.front(), lhs.front(), rhs.front());
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BinaryOperation::BitwiseAnd: // &
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && bitwiseAnd(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::BinaryOperation::BitwiseOr: // |
                synthesisOfExprOk = getConstantLines(expression.bitwidth(), 0U, lines) && bitwiseOr(annotatableQuantumComputation, lines, lhs, rhs);
                break;
            case BinaryExpression::BinaryOperation::LessThan: { // <
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = lessThan(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BinaryOperation::GreaterThan: { // >
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = greaterThan(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BinaryOperation::Equals: { // =
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = equals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BinaryOperation::NotEquals: { // !=
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = notEquals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BinaryOperation::LessEquals: { // <=
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
                if (ancillaryQubitForIntermediateResult.has_value()) {
                    lines.emplace_back(*ancillaryQubitForIntermediateResult);
                    synthesisOfExprOk = lessEquals(annotatableQuantumComputation, lines.front(), lhs, rhs);
                } else {
                    synthesisOfExprOk = false;
                }

                break;
            }
            case BinaryExpression::BinaryOperation::GreaterEquals: { // >=
                const std::optional<qc::Qubit> ancillaryQubitForIntermediateResult = getConstantLine(false, getLastCreatedModuleCallStackInstance());
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
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[i]) && annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(dest[i]);
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();
        return synthesisOk;
    }

    bool SyrecSynthesis::increment(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest) {
        annotatableQuantumComputation.activateControlQubitPropagationScope();

        bool synthesisOk = true;
        for (std::size_t i = 0; i < dest.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(dest.at(i));
        }

        for (int i = static_cast<int>(dest.size()) - 1; i >= 0 && synthesisOk; --i) {
            synthesisOk = annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(dest[static_cast<std::size_t>(i)]) && annotatableQuantumComputation.addOperationsImplementingNotGate(dest[static_cast<std::size_t>(i)]);
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
        bool synthesisOk = dest.size() >= src.size();
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(src[i], dest[i]);
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

        synthesisOk &= increase(annotatableQuantumComputation, dest, src, carry);
        for (std::size_t i = 0; i < src.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::disjunction(AnnotatableQuantumComputation& annotatableQuantumComputation, const qc::Qubit dest, const qc::Qubit src1, const qc::Qubit src2) {
        return annotatableQuantumComputation.addOperationsImplementingCnotGate(src1, dest) && annotatableQuantumComputation.addOperationsImplementingCnotGate(src2, dest) && annotatableQuantumComputation.addOperationsImplementingToffoliGate(src1, src2, dest);
    }

    bool SyrecSynthesis::division(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dividend, const std::vector<qc::Qubit>& divisor, const std::vector<qc::Qubit>& quotient, const std::vector<qc::Qubit>& remainder) {
        const std::size_t operandBitwidth = dividend.size();
        if (divisor.size() != operandBitwidth || quotient.size() != operandBitwidth || remainder.size() != operandBitwidth) {
            return false;
        }

        // Implementation of the division/modulo operation is based on the restoring division algorithm defined in the paper
        // 'Quantum Circuit Designs of Integer Division Optimizing T-count and T-depth (arXiv:1809.09732v1)'. The non-restoring
        // variant of the algorithm defined in the same paper requires less quantum gates in its implementation. Note that this algorithm
        // assumes that the dividend and divisor are positive two complement numbers.
        bool synthesisOk = true;
        for (std::size_t i = 0; i < operandBitwidth && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(dividend[i], quotient[i]);
        }

        std::vector<qc::Qubit> truncatedAggregateOfRemainderAndQuotientQubits(operandBitwidth, 0);
        // The aggregate variable V is a 'virtual' 2*N qubit variable that stores the combination of the remainder and quotient qubits in the form
        // R_(N-1), R_(N-2), ..., R_1, R_0, Q_(N-1), Q_(N-2), ..., Q_1, Q_0
        std::vector<qc::Qubit> aggregateOfRemainderAndQuotientQubits(operandBitwidth * 2, 0);
        std::copy(quotient.cbegin(), quotient.cend(), aggregateOfRemainderAndQuotientQubits.begin());
        std::copy(remainder.cbegin(), remainder.cend(), aggregateOfRemainderAndQuotientQubits.begin() + static_cast<std::ptrdiff_t>(operandBitwidth));
        std::reverse(aggregateOfRemainderAndQuotientQubits.begin(), aggregateOfRemainderAndQuotientQubits.end());

        annotatableQuantumComputation.activateControlQubitPropagationScope();
        for (std::size_t i = 1; i <= operandBitwidth && synthesisOk; ++i) {
            // Perform left shift of aggregate of remainder and quotient qubits and store them in 'virtual' variable Y (bitwidth: N)
            std::copy_n(aggregateOfRemainderAndQuotientQubits.begin() + static_cast<std::ptrdiff_t>(i), operandBitwidth, truncatedAggregateOfRemainderAndQuotientQubits.begin());

            // Since the operand for the subtraction and addition operation are expected to be in little endian qubit order (i.e. least significant qubit at index 0, ... , most significant qubit at index N - 1)
            // and our aggregate register stores the qubits in big endian qubit order, a reversal of the aggregate variable V needs to be performed after the shift was performed
            std::reverse(truncatedAggregateOfRemainderAndQuotientQubits.begin(), truncatedAggregateOfRemainderAndQuotientQubits.end());

            // The carry out bit of the subtraction operation is used to determine whether the resulting difference was < 0.
            const qc::Qubit signBitOfSubtraction = remainder[operandBitwidth - i];
            // Y = Y - b
            synthesisOk = decreaseWithCarry(annotatableQuantumComputation, truncatedAggregateOfRemainderAndQuotientQubits, divisor, signBitOfSubtraction);

            // The restore operation of the aggregate variable should only be performed when Y < 0.
            synthesisOk &= annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(signBitOfSubtraction);

            // Y = Y + divisor
            synthesisOk &= increase(annotatableQuantumComputation, truncatedAggregateOfRemainderAndQuotientQubits, divisor) && annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(signBitOfSubtraction);

            // After the 'restoring' operation for the variable V was performed, the final value of the remainder qubit can be set (remainder[i] = NOT(sign bit)).
            synthesisOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(signBitOfSubtraction);
        }
        annotatableQuantumComputation.deactivateControlQubitPropagationScope();

        // While the description of the reference algorithm states that the qubits of the quotient and remainder at this point store the values of the quotient and remainder respectively,
        // manual executions of the algorithm resulted in the quotient qubits storing the value of the remainder and vice versa, thus a final swap of the quotient and remainder qubits is required.
        for (std::size_t i = 0; i < operandBitwidth && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingFredkinGate(quotient[i], remainder[i]);
        }
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

    bool SyrecSynthesis::increase(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::optional<qc::Qubit>& optionalCarryOut) {
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
        const auto&       a        = lhs;
        const auto&       b        = rhs;

        // Implementation of the addition algorithm (a + b) mod N (N > 1) defined in the paper "Quantum Addition Circuits and Unbounded Fan-Out" (https://arxiv.org/abs/0910.2530v1)
        // based on a ripple-carry adder that requires no ancillary qubits. The sum of the two input operands 'a' and 'b' is stored in the qubits of the operand 'b'
        // (i.e. the right-hand side operand of the expression (a + b)). We will use N to denote the bitwidth of the operands in the description of the steps of the algorithm.

        // 1. Calculate the terms (a_i XOR b_i) for all 0 < i < N and store results in b_i as CNOT(control: a_i, target: b_i)
        for (std::size_t i = 1; i < bitwidth && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(a[i], b[i]);
        }

        // Optionally copy the value of the qubit a[N - 1] for the calculation of the carry out qubit
        synthesisOk &= !optionalCarryOut.has_value() || annotatableQuantumComputation.addOperationsImplementingCnotGate(a[bitwidth - 1], *optionalCarryOut);

        // 2. For every N > i > 0 store a backup of a_(i - 1) into a_i as CNOT(control: a_(i - 1), target: a_i)
        for (std::size_t i = bitwidth - 1; i > 1 && synthesisOk; --i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(a[i - 1], a[i]);
        }

        // 3. Calculate the carry bits and store them in a_i for every 0 <= i < (N - 1) as TOFFOLI(controls: {b_i, a_i}, target: a_(i + 1))
        for (std::size_t i = 0; i < bitwidth - 1 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingToffoliGate(b[i], a[i], a[i + 1]);
        }

        // Optionally calculate the value of carry out qubit
        synthesisOk &= !optionalCarryOut.has_value() || annotatableQuantumComputation.addOperationsImplementingToffoliGate(a[bitwidth - 1], b[bitwidth - 1], *optionalCarryOut);

        // 4. Calculate term (b_i XOR c_i) of the final sum terms (a_i XOR b_i XOR c_i) and "remove" the carry bit values from the lines (a_(i - 1)) storing the backup values of a_i for all N > i > 0:
        //    - CNOT(control: a_i, b_i)
        //    - TOFFOLI(controls: {a_(i - 1), b_(i - 1)}, target: a_i)
        for (std::size_t i = bitwidth - 1; i > 0 && synthesisOk; --i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(a[i], b[i]) && annotatableQuantumComputation.addOperationsImplementingToffoliGate(a[i - 1], b[i - 1], a[i]);
        }

        // 5. Restore the backup values storing in (a_(i - 1)) back to a_i as: 0 < i < N - 1: CNOT(control: a_i, target: a_(i + 1))
        for (std::size_t i = 1; i < bitwidth - 1 && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(a[i], a[i + 1]);
        }

        // 6. Calculate the final sum terms as: N > i > 0: CNOT(control: a_i, b_i)
        for (std::size_t i = bitwidth; i > 0 && synthesisOk; --i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(a[i - 1], b[i - 1]);
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

    bool SyrecSynthesis::lessEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        return lessThan(annotatableQuantumComputation, dest, src1, src2) && annotatableQuantumComputation.addOperationsImplementingNotGate(dest);
    }

    bool SyrecSynthesis::lessThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        return decreaseWithCarry(annotatableQuantumComputation, src1, src2, dest) && increase(annotatableQuantumComputation, src1, src2);
    }

    bool SyrecSynthesis::modulo(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dividend, const std::vector<qc::Qubit>& divisor, const std::vector<qc::Qubit>& quotient, const std::vector<qc::Qubit>& remainder) {
        return division(annotatableQuantumComputation, dividend, divisor, quotient, remainder);
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
        bool synthesisOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src1.front()) && bitwiseCnot(annotatableQuantumComputation, sum, partial) && annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src1.front());

        for (std::size_t i = 1; i < dest.size() && synthesisOk; ++i) {
            sum.erase(sum.begin());
            partial.pop_back();
            synthesisOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src1[i]) && increase(annotatableQuantumComputation, sum, partial) && annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src1[i]);
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

    bool SyrecSynthesis::leftShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& toBeShiftedQubits, const unsigned qubitIndexShiftAmount) {
        if (qubitIndexShiftAmount >= dest.size()) {
            return true;
        }

        const std::size_t nQubitsShifted       = dest.size() - qubitIndexShiftAmount;
        bool              synthesisOk          = toBeShiftedQubits.size() >= nQubitsShifted;
        const auto        targetLineBaseOffset = static_cast<std::size_t>(qubitIndexShiftAmount);
        for (std::size_t i = 0; i < nQubitsShifted && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(toBeShiftedQubits[i], dest[targetLineBaseOffset + i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::rightShift(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& dest, const std::vector<qc::Qubit>& toBeShiftedQubits, const unsigned qubitIndexShiftAmount) {
        if (qubitIndexShiftAmount >= dest.size()) {
            return true;
        }

        const std::size_t nQubitsShifted        = dest.size() - qubitIndexShiftAmount;
        bool              synthesisOk           = toBeShiftedQubits.size() >= nQubitsShifted;
        const auto        sourceQubitBaseOffset = static_cast<std::size_t>(qubitIndexShiftAmount);
        for (std::size_t i = 0; i < nQubitsShifted && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(toBeShiftedQubits[sourceQubitBaseOffset + i], dest[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::expressionOpInverse([[maybe_unused]] const BinaryExpression::BinaryOperation binaryOperation, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs) {
        return true;
    }

    bool SyrecSynthesis::getVariables(const VariableAccess::ptr& var, std::vector<qc::Qubit>& lines) {
        Variable::ptr referenceVariableData = var->getVar();
        // A chain of Call-/UncallStatements will also produce a chain of references set for a modules parameter
        // that needs to be resolved by walking up the chain until the first entry is reached to be able to determine
        // which qubit is actually referenced by the variable access
        while (referenceVariableData->reference != nullptr) {
            referenceVariableData = referenceVariableData->reference;
        }
        assert(referenceVariableData != nullptr);

        qc::Qubit offsetToFirstQubitOfVariable = 0;
        if (const auto& matchingLookupEntryForVariableIdentifier = varLines.find(referenceVariableData); matchingLookupEntryForVariableIdentifier != varLines.cend()) {
            offsetToFirstQubitOfVariable = matchingLookupEntryForVariableIdentifier->second;
        } else {
            std::cerr << "Failed to determine first qubit for variable with identifier " << referenceVariableData->name << "\n";
            return false;
        }

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
                    offsetToFirstQubitOfVariable += aggregateValue * referenceVariableData->bitwidth;
                }
            }
        }

        if (var->range) {
            auto [nfirst, nsecond] = *var->range;

            const unsigned first  = nfirst->evaluate(loopMap);
            const unsigned second = nsecond->evaluate(loopMap);

            if (first < second) {
                for (unsigned i = first; i <= second; ++i) {
                    lines.emplace_back(offsetToFirstQubitOfVariable + i);
                }
            } else {
                for (auto i = static_cast<int>(first); i >= static_cast<int>(second); --i) {
                    lines.emplace_back(offsetToFirstQubitOfVariable + static_cast<qc::Qubit>(i));
                }
            }
        } else {
            for (unsigned i = 0U; i < referenceVariableData->bitwidth; ++i) {
                lines.emplace_back(offsetToFirstQubitOfVariable + i);
            }
        }

        if (lines.empty()) {
            std::cerr << "Failed to determine accessed qubits for variable access on variable with identifier " << referenceVariableData->name << "\n";
            return false;
        }
        return true;
    }

    std::optional<qc::Qubit> SyrecSynthesis::getConstantLine(bool value, const std::optional<QubitInliningStack::ptr>& inlinedQubitModuleCallStack) {
        qc::Qubit constLine = 0U;

        if (!freeConstLinesMap[value].empty()) {
            constLine = freeConstLinesMap[value].back();
            freeConstLinesMap[value].pop_back();
        } else if (!freeConstLinesMap[!value].empty()) {
            constLine = freeConstLinesMap[!value].back();
            freeConstLinesMap[!value].pop_back();
            if (!annotatableQuantumComputation.addOperationsImplementingNotGate(constLine)) {
                return std::nullopt;
            }
        } else {
            const auto        qubitIndex          = static_cast<qc::Qubit>(annotatableQuantumComputation.getNqubits());
            const std::string qubitLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(annotatableQuantumComputation.getNqubits(), value);
            auto              inliningInformation = AnnotatableQuantumComputation::InlinedQubitInformation();
            if (shouldQubitInlineInformationBeRecorded()) {
                inliningInformation.inlineStack = inlinedQubitModuleCallStack;
            }

            const std::optional<qc::Qubit> generatedQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit(qubitLabel, value, inliningInformation);
            if (!generatedQubitIndex.has_value() || *generatedQubitIndex != qubitIndex) {
                return std::nullopt;
            }
            constLine = qubitIndex;
        }
        return constLine;
    }

    bool SyrecSynthesis::getConstantLines(unsigned bitwidth, qc::Qubit value, std::vector<qc::Qubit>& lines) {
        assert(bitwidth <= 32);

        // Ancillary qubits generated for an integer larger than 1 all share the same origin and thus will reuse the same module call stack in its inline information
        const std::optional<QubitInliningStack::ptr> sharedAncillaryQubitModuleCallStack  = bitwidth > 0 ? getLastCreatedModuleCallStackInstance() : std::nullopt;
        bool                                         couldQubitsForConstantLinesBeFetched = shouldQubitInlineInformationBeRecorded() ? sharedAncillaryQubitModuleCallStack.has_value() : true;

        for (unsigned i = 0U; i < bitwidth && couldQubitsForConstantLinesBeFetched; ++i) {
            const std::optional<qc::Qubit> ancillaryQubitIndex = getConstantLine((value & (1 << i)) != 0, sharedAncillaryQubitModuleCallStack);
            if (ancillaryQubitIndex.has_value()) {
                lines.emplace_back(*ancillaryQubitIndex);
            } else {
                couldQubitsForConstantLinesBeFetched = false;
            }
        }
        return couldQubitsForConstantLinesBeFetched;
    }

    std::optional<AssignStatement::AssignOperation> SyrecSynthesis::tryMapBinaryToAssignmentOperation(BinaryExpression::BinaryOperation binaryOperation) noexcept {
        switch (binaryOperation) {
            case BinaryExpression::BinaryOperation::Add:
                return AssignStatement::AssignOperation::Add;
            case BinaryExpression::BinaryOperation::Subtract:
                return AssignStatement::AssignOperation::Subtract;
            case BinaryExpression::BinaryOperation::Exor:
                return AssignStatement::AssignOperation::Exor;
            default:
                return std::nullopt;
        }
    }

    std::optional<BinaryExpression::BinaryOperation> SyrecSynthesis::tryMapAssignmentToBinaryOperation(AssignStatement::AssignOperation assignOperation) noexcept {
        switch (assignOperation) {
            case AssignStatement::AssignOperation::Add:
                return BinaryExpression::BinaryOperation::Add;

            case AssignStatement::AssignOperation::Subtract:
                return BinaryExpression::BinaryOperation::Subtract;

            case AssignStatement::AssignOperation::Exor:
                return BinaryExpression::BinaryOperation::Exor;
            default:
                return std::nullopt;
        }
    }

    bool SyrecSynthesis::addVariable(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr, const std::optional<QubitInliningStack::ptr>& currentModuleCallStack) {
        bool couldQubitsForVariableBeAdded = true;

        const auto currNumQubits = annotatableQuantumComputation.getNqubits();
        if (dimensions.empty()) {
            for (unsigned i = 0U; i < var->bitwidth && couldQubitsForVariableBeAdded; ++i) {
                std::string                                                           internalQubitLabel     = var->name;
                std::string                                                           userDeclaredQubitLabel = var->name;
                const bool                                                            isGarbageQubit         = var->type == Variable::Type::In || var->type == Variable::Type::Wire;
                std::optional<AnnotatableQuantumComputation::InlinedQubitInformation> optionalQubitInliningInformation;

                if (var->type == Variable::Type::Wire || var->type == Variable::Type::State) {
                    // To prevent name clashes when local module variables are inlined at the callsite, all local variable names are transformed to '__q<curr_num_qubits>' and an alias is created and stored
                    // in the annotatable quantum computation. The <curr_num_qubits> portion of the new variable name is the number of qubits prior to the addition of any variable in this call so that the qubits
                    // created for each value of a dimension of a variable share the same name prefix (i.e. the variable 'wire a[2](2)' will cause the generation of the qubits '__q0[0].0', '__q0[0].1','__q0[1].0', '__q0[1].0')
                    internalQubitLabel = InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(currNumQubits);
                }
                internalQubitLabel += arraystr + "." + std::to_string(i);
                userDeclaredQubitLabel += arraystr + "." + std::to_string(i);

                if (internalQubitLabel != userDeclaredQubitLabel) {
                    optionalQubitInliningInformation                         = AnnotatableQuantumComputation::InlinedQubitInformation();
                    optionalQubitInliningInformation->userDeclaredQubitLabel = userDeclaredQubitLabel;
                    optionalQubitInliningInformation->inlineStack            = currentModuleCallStack;
                }
                couldQubitsForVariableBeAdded &= annotatableQuantumComputation.addNonAncillaryQubit(internalQubitLabel, isGarbageQubit, optionalQubitInliningInformation).has_value();
            }
        } else {
            const auto        len = static_cast<std::size_t>(dimensions.front());
            const std::vector newDimensions(dimensions.begin() + 1U, dimensions.end());

            for (std::size_t i = 0U; i < len && couldQubitsForVariableBeAdded; ++i) {
                couldQubitsForVariableBeAdded &= addVariable(annotatableQuantumComputation, newDimensions, var, arraystr + "[" + std::to_string(i) + "]", currentModuleCallStack);
            }
        }
        return couldQubitsForVariableBeAdded;
    }

    std::optional<QubitInliningStack::ptr> SyrecSynthesis::getLastCreatedModuleCallStackInstance() const {
        if (!shouldQubitInlineInformationBeRecorded() || moduleCallStackInstances->empty()) {
            return std::nullopt;
        }
        return moduleCallStackInstances->back();
    }

    std::optional<QubitInliningStack::ptr> SyrecSynthesis::createInsertAndGetCopyOfLastCreatedCallStackInstance() {
        if (const std::optional<QubitInliningStack::ptr> lastCreatedCallStackInstance = shouldQubitInlineInformationBeRecorded() ? getLastCreatedModuleCallStackInstance() : std::nullopt; lastCreatedCallStackInstance.has_value()) {
            const auto newInlineStackInstance = std::make_shared<QubitInliningStack>(**lastCreatedCallStackInstance);
            moduleCallStackInstances->emplace_back(newInlineStackInstance);
            return moduleCallStackInstances->back();
        }
        return std::nullopt;
    }

    bool SyrecSynthesis::shouldQubitInlineInformationBeRecorded() const {
        return moduleCallStackInstances.has_value();
    }

    void SyrecSynthesis::discardLastCreateModuleCallStackInstance() {
        if (!shouldQubitInlineInformationBeRecorded() || !getLastCreatedModuleCallStackInstance().has_value()) {
            return;
        }
        moduleCallStackInstances->pop_back();
    }

    bool SyrecSynthesis::synthesizeModuleCall(const std::variant<const CallStatement*, const UncallStatement*>& callStmtVariant) {
        const CallStatement*   callStmt   = std::holds_alternative<const CallStatement*>(callStmtVariant) ? std::get<const CallStatement*>(callStmtVariant) : nullptr;
        const UncallStatement* uncallStmt = std::holds_alternative<const UncallStatement*>(callStmtVariant) ? std::get<const UncallStatement*>(callStmtVariant) : nullptr;
        assert(callStmt != nullptr || uncallStmt != nullptr);

        const std::vector<std::string>& moduleParameters = callStmt != nullptr ? callStmt->parameters : uncallStmt->parameters;
        const Module::ptr&              targetModule     = callStmt != nullptr ? callStmt->target : uncallStmt->target;

        // 1. Adjust the references module's parameters to the call arguments
        for (std::size_t i = 0U; i < moduleParameters.size(); ++i) {
            assert(!modules.empty());

            const std::string_view&             parameterIdentifier                        = moduleParameters.at(i);
            const std::optional<Variable::ptr>& matchingParameterOrVariableOfCurrentModule = modules.top()->findParameterOrVariable(parameterIdentifier);
            if (!matchingParameterOrVariableOfCurrentModule.has_value() || matchingParameterOrVariableOfCurrentModule.value() == nullptr) {
                std::cerr << "Failed to find matching parameter or variable of module " << modules.top()->name << " for parameter '" << parameterIdentifier << "' when setting references of parameters of " << (callStmt != nullptr ? "called" : "uncalled") << " module " << targetModule->name;
                return false;
            }
            const auto& moduleParameter = targetModule->parameters.at(i);
            moduleParameter->setReference(*matchingParameterOrVariableOfCurrentModule);
        }

        // 2. Create new lines for the module's variables
        if (!addVariables(targetModule->variables)) {
            return false;
        }

        modules.push(targetModule);
        const auto& statements              = targetModule->statements;
        bool        synthesisOfModuleBodyOk = true;

        const std::optional<StatementExecutionOrderStack::StatementExecutionOrder> currentStmtExecutionOrder = statementExecutionOrderStack->getCurrentAggregateStatementExecutionOrderState();
        if (!currentStmtExecutionOrder.has_value()) {
            std::cerr << "Failed to determine current statement execution order\n";
            return false;
        }

        // If the current statement execution order is set to execute a statement block by inverting all of its statements and traverse them in reverse order then any UncallStatement is transformed to a CallStatement in the statement block
        // thus the execution order added to the aggregate state for the Call-/UncallStatement needs to also take the current aggregate state into account.
        // An example:
        //   module main(inout a(3))
        //     uncall child(a)
        //
        //   module child(inout a(3))
        //     uncall grandChild(a)
        //
        //   module grandChild(inout a(3))
        //     ++= a
        // The aggregate statement execution order state when the UncallStatement in the 'child' module is processed will invert the UncallStatement to a CallStatement but when the latter is then synthesized the state added to the aggregate
        // should be the one of the 'original' UncallStatement and not the one of the inverted CallStatement.
        const auto                                                  defaultExecutionOrderOfModuleBody   = callStmt != nullptr ? StatementExecutionOrderStack::StatementExecutionOrder::Sequential : StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
        const auto                                                  executionOrderToAddToAggregateState = currentStmtExecutionOrder.value() == StatementExecutionOrderStack::StatementExecutionOrder::Sequential ? defaultExecutionOrderOfModuleBody : !defaultExecutionOrderOfModuleBody;
        const StatementExecutionOrderStack::StatementExecutionOrder currentAggregateExecutionOrderState = statementExecutionOrderStack->addStatementExecutionOrderToAggregateState(executionOrderToAddToAggregateState);

        if (currentAggregateExecutionOrderState == StatementExecutionOrderStack::StatementExecutionOrder::Sequential) {
            synthesisOfModuleBodyOk = std::all_of(statements.cbegin(), statements.cend(), [&](const Statement::ptr& stmt) { return processStatement(stmt); });
        } else {
            for (auto it = statements.rbegin(); it != statements.rend() && synthesisOfModuleBodyOk; ++it) {
                if (const auto& reverseStatement = (*it)->reverse(); reverseStatement.has_value()) {
                    synthesisOfModuleBodyOk = processStatement(*reverseStatement);
                } else {
                    const auto offsetFromLastStmtToCurrentlyProcessedOneInUncalledModule = static_cast<std::size_t>(std::distance(statements.rend(), it));
                    if (callStmt != nullptr) {
                        std::cerr << "Failed to create inverse of statement at index " << std::to_string(statements.size() - offsetFromLastStmtToCurrentlyProcessedOneInUncalledModule) << " in body of called module " << targetModule->name << "(CALL @ " << std::to_string(it->get()->lineNumber) << ")";
                    } else {
                        std::cerr << "Failed to create inverse of statement at index " << std::to_string(statements.size() - offsetFromLastStmtToCurrentlyProcessedOneInUncalledModule) << " in body of uncalled module " << targetModule->name << "(UNCALL @ " << std::to_string(it->get()->lineNumber) << ")";
                    }
                    synthesisOfModuleBodyOk = false;
                }
            }
        }

        if (!statementExecutionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState()) {
            std::cerr << "Failed to remove last added statement execution order from internal stack\n";
            synthesisOfModuleBodyOk = false;
        }
        modules.pop();
        return synthesisOfModuleBodyOk;
    }
} // namespace syrec
