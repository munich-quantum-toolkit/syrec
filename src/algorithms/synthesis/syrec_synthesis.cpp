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

#include "algorithms/synthesis/first_variable_qubit_offset_lookup.hpp"
#include "algorithms/synthesis/internal_qubit_label_builder.hpp"
#include "algorithms/synthesis/statement_execution_order_stack.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/qubit_inlining_stack.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <regex>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {
    /*
     * Prefer the usage of std::chrono::steady_clock instead of std::chrono::system_clock since the former cannot decrease (due to time zone changes, etc.) and is most suitable for measuring intervals according to (https://en.cppreference.com/w/cpp/chrono/steady_clock)
     */
    using TimeStamp = std::chrono::time_point<std::chrono::steady_clock>;

    [[nodiscard]] bool isMoreThanOneModuleMatchingIdentifierDeclared(const syrec::Module::vec& modulesToCheck, const std::string_view& moduleIdentifierToFind) {
        return std::ranges::count_if(modulesToCheck, [moduleIdentifierToFind](const syrec::Module::ptr& moduleToCheck) { return moduleToCheck->name == moduleIdentifierToFind; }) > 1;
    }

    [[nodiscard]] std::optional<std::vector<bool>> convertIntegerToBinary(const std::size_t resultBitwidth, unsigned integerToConvert) {
        if (resultBitwidth == 0) {
            return std::nullopt;
        }

        std::vector resultContainer(resultBitwidth, false);
        for (std::size_t i = 0; i < resultBitwidth; ++i) {
            resultContainer[i] = static_cast<bool>(integerToConvert % 2);
            integerToConvert >>= 1;
        }
        return resultContainer;
    }

    [[nodiscard]] bool moveIntegerValueToAncillaryQubits(syrec::AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& ancillaryQubitIndices, const unsigned integerValue) {
        bool synthesisOk = false;
        if (const std::optional<std::vector<bool>> generatedBitsOfIntegerValue = convertIntegerToBinary(ancillaryQubitIndices.size(), integerValue); generatedBitsOfIntegerValue.has_value()) {
            synthesisOk                            = true;
            const std::vector<bool>& bitsOfInteger = *generatedBitsOfIntegerValue;
            for (std::size_t i = 0; i < ancillaryQubitIndices.size() && synthesisOk; ++i) {
                if (bitsOfInteger[i]) {
                    synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(ancillaryQubitIndices[i]);
                }
            }
        }
        return synthesisOk;
    }

    [[nodiscard]] bool clearIntegerValueFromAncillaryQubits(syrec::AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& ancillaryQubitIndices, const unsigned integerValue) {
        // Since we are assuming that the ancillary qubits currently storing the value of the integer were initially set to zero, we can simply apply
        // the same gate sequence that was used to move the integer value to the ancillaries to reset the latter.
        return moveIntegerValueToAncillaryQubits(annotatableQuantumComputation, ancillaryQubitIndices, integerValue);
    }

    [[nodiscard]] bool checkIfQubitsMatchAndStoreResultInRhsOperandQubits(syrec::AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& lhsOperand, const std::vector<qc::Qubit>& rhsOperand, bool clearResultFromRhsOperand = false) {
        if (lhsOperand.size() != rhsOperand.size()) {
            std::cerr << "Can only compare two qubit sequences if they contained the same number of qubits, lhs operand contained: " << std::to_string(lhsOperand.size()) << " qubits while the rhs operand contained " << std::to_string(rhsOperand.size()) << "\n";
            return false;
        }
        bool synthesisOk = true;
        if (!clearResultFromRhsOperand) {
            for (std::size_t i = 0; i < lhsOperand.size() && synthesisOk; ++i) {
                synthesisOk = annotatableQuantumComputation.addOperationsImplementingCnotGate(lhsOperand.at(i), rhsOperand.at(i)) && annotatableQuantumComputation.addOperationsImplementingNotGate(rhsOperand.at(i));
            }
        } else {
            for (std::size_t i = 0; i < lhsOperand.size() && synthesisOk; ++i) {
                synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(rhsOperand.at(i)) &&
                              annotatableQuantumComputation.addOperationsImplementingCnotGate(lhsOperand.at(i), rhsOperand.at(i));
            }
        }
        return synthesisOk;
    }

    [[nodiscard]] unsigned determineNumberOfElementsInVariable(const syrec::Variable& variableAccess) {
        return std::accumulate(variableAccess.dimensions.cbegin(), variableAccess.dimensions.cend(), 1U, std::multiplies());
    }

    [[nodiscard]] unsigned determineNumberOfBitsRequiredToStoreValue(const unsigned value) {
        return static_cast<unsigned>(std::bit_width(value));
    }
} // namespace

namespace syrec {
    // Helper Functions for the synthesis methods
    SyrecSynthesis::SyrecSynthesis(AnnotatableQuantumComputation& annotatableQuantumComputation):
        annotatableQuantumComputation(annotatableQuantumComputation) {
        freeConstLinesMap.try_emplace(false /* emplacing a default constructed object */);
        freeConstLinesMap.try_emplace(true /* emplacing a default constructed object */);
        statementExecutionOrderStack   = std::make_unique<StatementExecutionOrderStack>();
        firstVariableQubitOffsetLookup = std::make_unique<FirstVariableQubitOffsetLookup>();
    }

    void SyrecSynthesis::setMainModule(const Module::ptr& mainModule) {
        assert(modules.empty());
        modules.push(mainModule);
    }

    bool SyrecSynthesis::addVariables(const Variable::vec& variables) const {
        // We only want to record inlining information for qubits that are actually inlined (i.e. variables of type 'wire' and 'state').
        // Note that all variables added in this call shared the same inlining stack thus we reuse the latter when adding the former.
        const bool                                   isAnyVarALocalModuleVarBasedOnVarType = std::ranges::any_of(variables, [](const Variable::ptr& variable) { return variable->type == Variable::Type::Wire || variable->type == Variable::Type::State; });
        const std::optional<QubitInliningStack::ptr> inlineStack                           = isAnyVarALocalModuleVarBasedOnVarType ? getLastCreatedModuleCallStackInstance() : std::nullopt;
        bool                                         couldQubitsForVariablesBeAdded        = shouldQubitInlineInformationBeRecorded() && isAnyVarALocalModuleVarBasedOnVarType ? inlineStack.has_value() : true;

        for (std::size_t i = 0; i < variables.size() && couldQubitsForVariablesBeAdded; ++i) {
            const auto& variable = variables[i];
            if (const std::optional<qc::Qubit> firstQubitOfVariable = variable != nullptr ? addVariable(annotatableQuantumComputation, variable->dimensions, variable, std::string(), inlineStack) : std::nullopt; firstQubitOfVariable.has_value()) {
                if (firstVariableQubitOffsetLookup == nullptr || !firstVariableQubitOffsetLookup->registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(variable->name, *firstQubitOfVariable)) {
                    std::cerr << "Failed to register offset to first qubit of variable " << variable->name << "\n";
                    couldQubitsForVariablesBeAdded = false;
                }
            } else {
                std::cerr << "Failed to register offset to first qubit of variable due to variable being null\n";
                couldQubitsForVariablesBeAdded = false;
            }
        }
        return couldQubitsForVariablesBeAdded;
    }

    bool SyrecSynthesis::synthesize(SyrecSynthesis* synthesizer, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        if (synthesizer->statementExecutionOrderStack->getCurrentAggregateStatementExecutionOrderState() != StatementExecutionOrderStack::StatementExecutionOrder::Sequential) {
            std::cerr << "Execution order at start of synthesis should be sequential\n";
            return false;
        }

        const Module::vec& programModules = program.modules();
        if (programModules.empty()) {
            std::cerr << "A SyReC program must consist of at least one module\n";
            return false;
        }

        // Validation of optional defined main module identifier of synthesis settings
        const std::string&         defaultMainModuleIdentifier = "main";
        std::optional<std::string> expectedMainModuleIdentifier;
        if (settings != nullptr && settings->get<std::string>(MAIN_MODULE_IDENTIFIER_CONFIG_KEY).has_value()) {
            expectedMainModuleIdentifier = settings->get<std::string>(MAIN_MODULE_IDENTIFIER_CONFIG_KEY);
            if (expectedMainModuleIdentifier.value().empty()) {
                std::cerr << "Expected main module identifier defined in synthesis settings must have a value\n";
                return false;
            }
            const std::regex expectedMainModuleIdentifierValidationRegex("^(_|[a-zA-Z])+\\w*");
            if (!std::regex_match(*expectedMainModuleIdentifier, expectedMainModuleIdentifierValidationRegex)) {
                std::cerr << "Expected main module identifier defined in synthesis settings '" << *expectedMainModuleIdentifier << "' did not defined a valid identifier according to the SyReC grammar, check your inputs!\n";
                return false;
            }
        } else {
            if (program.findModule(defaultMainModuleIdentifier) != nullptr) {
                expectedMainModuleIdentifier = defaultMainModuleIdentifier;
            } else {
                expectedMainModuleIdentifier = program.modules().back()->name;
            }
        }

        // Run-time measuring
        const TimeStamp simulationStartTime = std::chrono::steady_clock::now();

        // get the main module
        Module::ptr main;
        if (expectedMainModuleIdentifier.has_value()) {
            if (isMoreThanOneModuleMatchingIdentifierDeclared(programModules, *expectedMainModuleIdentifier)) {
                std::cerr << "There can be at most one module named '" << *expectedMainModuleIdentifier << "' that shall be used as the entry point of the SyReC program\n";
                return false;
            }
            const auto& lastModuleMatchingIdentifier = std::ranges::find_if(std::ranges::reverse_view(programModules), [&expectedMainModuleIdentifier](const Module::ptr& programModule) { return programModule->name == *expectedMainModuleIdentifier; });
            if (lastModuleMatchingIdentifier == programModules.crend()) {
                std::cerr << "If the expected main module identifier is defined using the synthesis settings ('" << *expectedMainModuleIdentifier << "') then there must be at least one module matching the defined identifier\n";
                return false;
            }
            main = *lastModuleMatchingIdentifier;
        } else {
            main = program.findModule(defaultMainModuleIdentifier);
            if (main != nullptr) {
                if (isMoreThanOneModuleMatchingIdentifierDeclared(programModules, defaultMainModuleIdentifier)) {
                    std::cerr << "There can be at most one module named 'main'\n";
                    return false;
                }
            } else {
                main = programModules.back();
            }
        }
        assert(main != nullptr);

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

        if (synthesizer->firstVariableQubitOffsetLookup == nullptr) {
            std::cerr << "Internal lookup for offsets to first qubits of variables was not correctly initialized\n";
            return false;
        }

        synthesizer->firstVariableQubitOffsetLookup->openNewVariableQubitOffsetScope();
        // create lines for global variables
        if (!synthesizer->addVariables(main->parameters)) {
            std::cerr << "Failed to create qubits for parameters of main module of SyReC program\n";
            return false;
        }
        if (!synthesizer->addVariables(main->variables)) {
            std::cerr << "Failed to create qubits for local variables of main module of SyReC program\n";
            return false;
        }

        // synthesize the statements
        const auto synthesisOfMainModuleOk = synthesizer->onModule(main);
        for (const auto& ancillaryQubit: synthesizer->annotatableQuantumComputation.getAddedPreliminaryAncillaryQubitIndices()) {
            if (!synthesizer->annotatableQuantumComputation.promotePreliminaryAncillaryQubitToDefinitiveAncillary(ancillaryQubit)) {
                std::cerr << "Failed to mark qubit" << std::to_string(ancillaryQubit) << " as ancillary qubit\n";
                return false;
            }
        }

        if (!synthesizer->firstVariableQubitOffsetLookup->closeVariableQubitOffsetScope()) {
            std::cerr << "Failed to close qubit offset scope for parameters and local variables during cleanup after synthesis of main module " << main->name << "\n";
            return false;
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

        std::erase_if(checkLhsVec, [](const std::vector<qc::Qubit>& linesContainer) { return linesContainer.empty(); });
        std::erase_if(checkRhsVec, [](const std::vector<qc::Qubit>& linesContainer) { return linesContainer.empty(); });

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
                        okay                                                            = copyOfLastCreatedQubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = callStat->target})) && onStatement(*callStat);
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
                        okay                                                            = copyOfLastCreatedQubitInlineStack->push(QubitInliningStack::QubitInliningStackEntry({.lineNumberOfCallOfTargetModule = std::nullopt, .isTargetModuleAccessedViaCallStmt = std::nullopt, .targetModule = uncallStat->target})) && onStatement(*uncallStat);
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

    // If both variable accesses of the SwapStatement contained only expressions evaluable at compile time in its dimension access component
    // then the accessed qubits of the both variables can be determined a compile time and the procedure below can be ignored for the synthesis of the swap statement.
    //
    // Otherwise, the following steps need to be performed for both parts of the swap statement (the same procedure with small changes is also used for the synthesis of Unary- and AssignStatements):
    // However, instead of two potential cases (a variable access with non-compile time constant expressions [CTCEs] and one without) we have to consider four different cases
    // The four different cases are:
    //   I.   Both operands of the SwapStatement contained only compile time constant expressions in the accessed variable parts.
    //   II.  The left hand operand of the SwapStatement contained only both CTCEs and non-CTCEs while the right hand side operand contained only CTCEs.
    //   III. The left hand operand of the SwapStatement contained only CTCEs while the right hand side operand contained both CTCEs and non-CTCEs.
    //   IV.  Both operands of the SwapStatement contained both CTCEs and non-CTCEs in their accessed variable parts.
    // and need to update the procedure below accordingly.
    //
    // The procedure applied for the cases II-II involves the following steps:
    // I.   Calculate the index of the accessed index in the unrolled variable and store the value in ancillary qubits (note that the calculated value is not available at compile time).
    // II.  Iterate through all possible index values and compare them to the index calculated in I. Use the result of this operation as control qubits to perform a conditional swap of the
    //      qubits at the current index in the accessed variable. A swap needs to be performed since the assignment needs to update the qubits that store the current value of the accessed element and not on the
    //      current value itself. Note that for a variable access containing non-compile time constant expressions in its dimension access component that is used as an operand in an expression it is sufficient
    //      to copy the value of the qubits of the accessed element of the variable to the ancillary qubits storing the "extracted" qubits of the variable since we are only interesting in the value of the qubits of
    //      the accessed element and not the qubits themself.
    //
    //      module main(inout a[2][3](2)) wire b(2)
    //        a[b][0] += (a[(b + 1)][0] + 2)
    //      In this example it is sufficient to determine the value of the qubits accessed by a[(b + 1)][0] to synthesize the expression on the right hand side of the expression while to correctly update the value of the
    //      qubits of a[b][0] the assignment operation '+=' needs to operate on the qubits of a[b][0] and not only the value of a[b][0].
    //
    // III. After the accessed qubits of both variable were determined, perform the synthesis of the swap operation.
    // IV.  Swap the qubits storing the accessed qubits of the variables back to the qubits of the accessed element in the variable for both operands of the swap operation.
    bool SyrecSynthesis::onStatement(const SwapStatement& statement) {
        const std::optional<EvaluatedVariableAccess> evaluatedLhsOperand = evaluateAndValidateVariableAccess(statement.lhs, loopMap, firstVariableQubitOffsetLookup);
        const std::optional<EvaluatedVariableAccess> evaluatedRhsOperand = evaluateAndValidateVariableAccess(statement.rhs, loopMap, firstVariableQubitOffsetLookup);
        if (!evaluatedLhsOperand.has_value() || !evaluatedRhsOperand.has_value()) {
            return false;
        }

        const EvaluatedVariableAccess& dataOfEvaluatedLhsOperand = *evaluatedLhsOperand;
        const EvaluatedVariableAccess& dataOfEvaluatedRhsOperand = *evaluatedRhsOperand;

        const std::size_t     aggregateOfWhetherOperandsContainedOnlyNumericExpressions                = static_cast<std::size_t>(dataOfEvaluatedLhsOperand.evaluatedDimensionAccess.containedOnlyNumericExpressions) + (static_cast<std::size_t>(dataOfEvaluatedRhsOperand.evaluatedDimensionAccess.containedOnlyNumericExpressions) << 2);
        constexpr std::size_t onlyLhsOperandContainedCompileTimeConstantExpressionsInDimensionAccess   = 1U;
        constexpr std::size_t onlyRhsOperandContainedCompileTimeConstantExpressionsInDimensionAccess   = 4U;
        constexpr std::size_t noOperandContainedOnlyCompileTimeConstantExpressionsInDimensionAccess    = 0U;
        constexpr std::size_t bothOperandsContainedOnlyCompileTimeConstantExpressionsInDimensionAccess = 5U;

        bool           synthesisOk      = true;
        const unsigned numQubitsSwapped = static_cast<unsigned>(dataOfEvaluatedLhsOperand.evaluatedBitrangeAccess.getIndicesOfAccessedBits().size());
        switch (aggregateOfWhetherOperandsContainedOnlyNumericExpressions) {
            case bothOperandsContainedOnlyCompileTimeConstantExpressionsInDimensionAccess: {
                std::vector<qc::Qubit> qubitsOfLhsOperand;
                std::vector<qc::Qubit> qubitsOfRhsOperand;
                synthesisOk = getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(dataOfEvaluatedLhsOperand, qubitsOfLhsOperand) && getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(dataOfEvaluatedRhsOperand, qubitsOfRhsOperand) && swap(annotatableQuantumComputation, qubitsOfLhsOperand, qubitsOfRhsOperand);
                break;
            }
            case onlyRhsOperandContainedCompileTimeConstantExpressionsInDimensionAccess: {
                std::vector<qc::Qubit> qubitsStoringUnrolledIndexOfLhsOperand;
                std::vector<qc::Qubit> qubitsOfRhsOperand;
                synthesisOk = calculateSymbolicUnrolledIndexForElementInVariable(dataOfEvaluatedLhsOperand, qubitsStoringUnrolledIndexOfLhsOperand);

                std::vector<qc::Qubit> containerStoringExtractedQubitsOfLhsOperand;
                synthesisOk &= getConstantLines(numQubitsSwapped, 0U, containerStoringExtractedQubitsOfLhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedLhsOperand, qubitsStoringUnrolledIndexOfLhsOperand, containerStoringExtractedQubitsOfLhsOperand, QubitTransferOperation::SwapQubits) && getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(dataOfEvaluatedRhsOperand, qubitsOfRhsOperand) && swap(annotatableQuantumComputation, containerStoringExtractedQubitsOfLhsOperand, qubitsOfRhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedLhsOperand, qubitsStoringUnrolledIndexOfLhsOperand, containerStoringExtractedQubitsOfLhsOperand, QubitTransferOperation::SwapQubits);
                break;
            }
            case onlyLhsOperandContainedCompileTimeConstantExpressionsInDimensionAccess: {
                std::vector<qc::Qubit> qubitsOfLhsOperand;
                synthesisOk = getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(dataOfEvaluatedLhsOperand, qubitsOfLhsOperand);

                std::vector<qc::Qubit> qubitsStoringUnrolledIndexOfRhsOperand;
                synthesisOk &= calculateSymbolicUnrolledIndexForElementInVariable(dataOfEvaluatedRhsOperand, qubitsStoringUnrolledIndexOfRhsOperand);

                std::vector<qc::Qubit> containerStoringExtractedQubitsOfRhsOperand;
                synthesisOk &= getConstantLines(numQubitsSwapped, 0U, containerStoringExtractedQubitsOfRhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedRhsOperand, qubitsStoringUnrolledIndexOfRhsOperand, containerStoringExtractedQubitsOfRhsOperand, QubitTransferOperation::SwapQubits) && swap(annotatableQuantumComputation, qubitsOfLhsOperand, containerStoringExtractedQubitsOfRhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedRhsOperand, qubitsStoringUnrolledIndexOfRhsOperand, containerStoringExtractedQubitsOfRhsOperand, QubitTransferOperation::SwapQubits);
                break;
            }
            case noOperandContainedOnlyCompileTimeConstantExpressionsInDimensionAccess: {
                std::vector<qc::Qubit> qubitsStoringUnrolledIndexOfLhsOperand;
                synthesisOk = calculateSymbolicUnrolledIndexForElementInVariable(dataOfEvaluatedLhsOperand, qubitsStoringUnrolledIndexOfLhsOperand);

                std::vector<qc::Qubit> containerStoringExtractedQubitsOfLhsOperand;
                synthesisOk &= getConstantLines(numQubitsSwapped, 0U, containerStoringExtractedQubitsOfLhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedLhsOperand, qubitsStoringUnrolledIndexOfLhsOperand, containerStoringExtractedQubitsOfLhsOperand, QubitTransferOperation::SwapQubits);

                std::vector<qc::Qubit> qubitsStoringUnrolledIndexOfRhsOperand;
                synthesisOk &= calculateSymbolicUnrolledIndexForElementInVariable(dataOfEvaluatedRhsOperand, qubitsStoringUnrolledIndexOfRhsOperand);

                std::vector<qc::Qubit> containerStoringExtractedQubitsOfRhsOperand;
                synthesisOk &= getConstantLines(numQubitsSwapped, 0U, containerStoringExtractedQubitsOfRhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedRhsOperand, qubitsStoringUnrolledIndexOfRhsOperand, containerStoringExtractedQubitsOfRhsOperand, QubitTransferOperation::SwapQubits);

                synthesisOk &= swap(annotatableQuantumComputation, containerStoringExtractedQubitsOfLhsOperand, containerStoringExtractedQubitsOfRhsOperand) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedLhsOperand, qubitsStoringUnrolledIndexOfLhsOperand, containerStoringExtractedQubitsOfLhsOperand, QubitTransferOperation::SwapQubits) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedRhsOperand, qubitsStoringUnrolledIndexOfRhsOperand, containerStoringExtractedQubitsOfRhsOperand, QubitTransferOperation::SwapQubits);
                break;
            }
            default:
                return false;
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::onStatement(const UnaryStatement& statement) {
        const std::optional<EvaluatedVariableAccess> evaluatedVariableAccess = evaluateAndValidateVariableAccess(statement.var, loopMap, firstVariableQubitOffsetLookup);
        if (!evaluatedVariableAccess.has_value()) {
            return false;
        }

        bool                           synthesisOk                   = true;
        const EvaluatedVariableAccess& dataOfEvaluatedVariableAccess = *evaluatedVariableAccess;
        const unsigned                 numAccessedQubits             = static_cast<unsigned>(dataOfEvaluatedVariableAccess.evaluatedBitrangeAccess.getIndicesOfAccessedBits().size());

        // If the variable access defining the assigned to variable parts of the UnaryStatement contains only expressions evaluable at compile time in its dimension access component
        // then the accessed qubits of the variable can be determined a compile time and the procedure below can be ignored for the synthesis of the assignment.
        //
        // Otherwise, the following steps need to be performed and are almost identical to the ones performed for a syrec::AssignStatement with the difference that for an syrec::UnaryStatement no extra expression needs to be handled:
        // I.   Calculate the index of the accessed index in the unrolled variable and store the value in ancillary qubits (note that the calculated value is not available at compile time).
        // II.  Iterate through all possible index values and compare them to the index calculated in I. Use the result of this operation as control qubits to perform a conditional swap of the
        //      qubits at the current index in the accessed variable. A swap needs to be performed since the assignment needs to update the qubits that store the current value of the accessed element and not on the
        //      current value itself. Note that for a variable access containing non-compile time constant expressions in its dimension access component that is used as an operand in an expression it is sufficient
        //      to copy the value of the qubits of the accessed element of the variable to the ancillary qubits storing the "extracted" qubits of the variable since we are only interesting in the value of the qubits of
        //      the accessed element and not the qubits themself.
        //
        //      module main(inout a[2][3](2)) wire b(2)
        //        a[b][0] += (a[(b + 1)][0] + 2)
        //      In this example it is sufficient to determine the value of the qubits accessed by a[(b + 1)][0] to synthesize the expression on the right hand side of the expression while to correctly update the value of the
        //      qubits of a[b][0] the assignment operation '+=' needs to operate on the qubits of a[b][0] and not only the value of a[b][0].
        //
        // III. Perform the synthesis of the assignment operation.
        // IV.  Swap the qubits storing the result of the assignment back to the qubits of the accessed element in the variable.
        std::vector<qc::Qubit> qubitsStoringUnrolledIndex;
        std::vector<qc::Qubit> qubitsStoringAccessedValueOfVariable;
        if (dataOfEvaluatedVariableAccess.evaluatedDimensionAccess.containedOnlyNumericExpressions) {
            synthesisOk = getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(dataOfEvaluatedVariableAccess, qubitsStoringAccessedValueOfVariable);
        } else {
            synthesisOk = calculateSymbolicUnrolledIndexForElementInVariable(dataOfEvaluatedVariableAccess, qubitsStoringUnrolledIndex) && getConstantLines(numAccessedQubits, 0U, qubitsStoringAccessedValueOfVariable) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedVariableAccess, qubitsStoringUnrolledIndex, qubitsStoringAccessedValueOfVariable, QubitTransferOperation::SwapQubits);
        }
        switch (statement.unaryOperation) {
            case UnaryStatement::UnaryOperation::Invert:
                synthesisOk &= bitwiseNegation(annotatableQuantumComputation, qubitsStoringAccessedValueOfVariable);
                break;
            case UnaryStatement::UnaryOperation::Increment:
                synthesisOk &= increment(annotatableQuantumComputation, qubitsStoringAccessedValueOfVariable);
                break;
            case UnaryStatement::UnaryOperation::Decrement:
                synthesisOk &= decrement(annotatableQuantumComputation, qubitsStoringAccessedValueOfVariable);
                break;
            default:
                synthesisOk = false;
                break;
        }

        if (synthesisOk && !dataOfEvaluatedVariableAccess.evaluatedDimensionAccess.containedOnlyNumericExpressions) {
            synthesisOk &= transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedVariableAccess, qubitsStoringUnrolledIndex, qubitsStoringAccessedValueOfVariable, QubitTransferOperation::SwapQubits);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::onStatement(const AssignStatement& statement) {
        const std::optional<EvaluatedVariableAccess> evaluatedLhsOperand = evaluateAndValidateVariableAccess(statement.lhs, loopMap, firstVariableQubitOffsetLookup);
        if (!evaluatedLhsOperand.has_value()) {
            return false;
        }

        bool                           synthesisOfAssignmentOk   = true;
        const EvaluatedVariableAccess& dataOfEvaluatedLhsOperand = evaluatedLhsOperand.value();
        std::vector<qc::Qubit>         qubitsStoringIndexInUnrolledVariable;
        std::vector<qc::Qubit>         qubitsStoringSelectedValueOfVariable;

        // If the variable access on the left hand side of the assignment contains only expressions evaluable at compile time in its dimension access component
        // then the accessed qubits of the variable can be determined a compile time and the procedure below can be ignored for the synthesis of the assignment.
        //
        // Otherwise, the following steps need to be performed.
        // I.   Calculate the index of the accessed index in the unrolled variable and store the value in ancillary qubits (note that the calculated value is not available at compile time).
        // II.  Iterate through all possible index values and compare them to the index calculated in I. Use the result of this operation as control qubits to perform a conditional swap of the
        //      qubits at the current index in the accessed variable. A swap needs to be performed since the assignment needs to update the qubits that store the current value of the accessed element and not on the
        //      current value itself. Note that for a variable access containing non-compile time constant expressions in its dimension access component that is used as an operand in an expression it is sufficient
        //      to copy the value of the qubits of the accessed element of the variable to the ancillary qubits storing the "extracted" qubits of the variable since we are only interesting in the value of the qubits of
        //      the accessed element and not the qubits themself.
        //
        //      module main(inout a[2][3](2)) wire b(2)
        //        a[b][0] += (a[(b + 1)][0] + 2)
        //      In this example it is sufficient to determine the value of the qubits accessed by a[(b + 1)][0] to synthesize the expression on the right hand side of the expression while to correctly update the value of the
        //      qubits of a[b][0] the assignment operation '+=' needs to operate on the qubits of a[b][0] and not only the value of a[b][0].
        //
        // III. Perform the synthesis of the assignment operation.
        // IV.  Swap the qubits storing the result of the assignment back to the qubits of the accessed element in the variable.
        if (evaluatedLhsOperand->evaluatedDimensionAccess.containedOnlyNumericExpressions) {
            synthesisOfAssignmentOk = getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(dataOfEvaluatedLhsOperand, qubitsStoringSelectedValueOfVariable);
        } else {
            synthesisOfAssignmentOk &= calculateSymbolicUnrolledIndexForElementInVariable(dataOfEvaluatedLhsOperand, qubitsStoringIndexInUnrolledVariable) && getConstantLines(static_cast<unsigned>(dataOfEvaluatedLhsOperand.evaluatedBitrangeAccess.getIndicesOfAccessedBits().size()), 0U, qubitsStoringSelectedValueOfVariable) && transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedLhsOperand, qubitsStoringIndexInUnrolledVariable, qubitsStoringSelectedValueOfVariable, QubitTransferOperation::SwapQubits);
        }

        // While a derviced class can fall back to the base class implementation to synthesis AssignStatements, the opRhsLhsExpression(...) call
        // of the derived class might not be able to handle the expression on the right-hand side of the assignment but since we are already using the base class
        // to synthesis the assignment (which should be able to handle all SyReC expression types) the return value of opRhsLhsExpression can be ignored.
        std::vector<qc::Qubit> d;
        opRhsLhsExpression(statement.rhs, d);

        std::vector<qc::Qubit> rhs;
        synthesisOfAssignmentOk &= SyrecSynthesis::onExpression(statement.rhs, rhs, qubitsStoringSelectedValueOfVariable, statement.assignOperation);
        opVec.clear();

        switch (statement.assignOperation) {
            case AssignStatement::AssignOperation::Add: {
                synthesisOfAssignmentOk &= assignAdd(qubitsStoringSelectedValueOfVariable, rhs, statement.assignOperation);
                break;
            }
            case AssignStatement::AssignOperation::Subtract: {
                synthesisOfAssignmentOk &= assignSubtract(qubitsStoringSelectedValueOfVariable, rhs, statement.assignOperation);
                break;
            }
            case AssignStatement::AssignOperation::Exor: {
                synthesisOfAssignmentOk &= assignExor(qubitsStoringSelectedValueOfVariable, rhs, statement.assignOperation);
                break;
            }
            default:
                return false;
        }

        // We need to swap back the value of the ancillary qubits currently storing the result of the assignment statement back to the qubits of the selected qubits of the variable accessed on the left hand side of the assignment.
        if (synthesisOfAssignmentOk && !dataOfEvaluatedLhsOperand.evaluatedDimensionAccess.containedOnlyNumericExpressions) {
            synthesisOfAssignmentOk &= transferQubitsOfElementAtIndexInVariableToOtherQubits(dataOfEvaluatedLhsOperand, qubitsStoringIndexInUnrolledVariable, qubitsStoringSelectedValueOfVariable, QubitTransferOperation::SwapQubits);
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
        bool synthesisOfBranchStatementsOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(guardExpressionQubit) && std::ranges::all_of(statement.thenStatements, [&](const Statement::ptr& trueBranchStatement) { return processStatement(trueBranchStatement); });

        // Toggle helper line.
        // We do not want to use the current helper line controlling the conditional execution of the statements
        // of both branches of the current IfStatement when negating the value of said helper line
        synthesisOfBranchStatementsOk &= annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(guardExpressionQubit) && annotatableQuantumComputation.addOperationsImplementingNotGate(guardExpressionQubit) && annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(guardExpressionQubit) && std::ranges::all_of(statement.elseStatements, [&](const Statement::ptr& falseBranchStatement) { return processStatement(falseBranchStatement); });

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
            for (auto i = static_cast<int>(from); std::cmp_greater_equal(i, to); i -= static_cast<int>(step)) {
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

        synthesisOk &= inplaceAdd(annotatableQuantumComputation, src, dest, carry);
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
        std::ranges::copy(quotient, aggregateOfRemainderAndQuotientQubits.begin());
        std::ranges::copy(remainder, aggregateOfRemainderAndQuotientQubits.begin() + static_cast<std::ptrdiff_t>(operandBitwidth));
        std::ranges::reverse(aggregateOfRemainderAndQuotientQubits);

        annotatableQuantumComputation.activateControlQubitPropagationScope();
        for (std::size_t i = 1; i <= operandBitwidth && synthesisOk; ++i) {
            // Perform left shift of aggregate of remainder and quotient qubits and store them in 'virtual' variable Y (bitwidth: N)
            std::copy_n(aggregateOfRemainderAndQuotientQubits.begin() + static_cast<std::ptrdiff_t>(i), operandBitwidth, truncatedAggregateOfRemainderAndQuotientQubits.begin());

            // Since the operand for the subtraction and addition operation are expected to be in little endian qubit order (i.e. least significant qubit at index 0, ... , most significant qubit at index N - 1)
            // and our aggregate register stores the qubits in big endian qubit order, a reversal of the aggregate variable V needs to be performed after the shift was performed
            std::ranges::reverse(truncatedAggregateOfRemainderAndQuotientQubits);

            // The carry out bit of the subtraction operation is used to determine whether the resulting difference was < 0.
            const qc::Qubit signBitOfSubtraction = remainder[operandBitwidth - i];
            // Y = Y - b
            synthesisOk = decreaseWithCarry(annotatableQuantumComputation, truncatedAggregateOfRemainderAndQuotientQubits, divisor, signBitOfSubtraction);

            // The restore operation of the aggregate variable should only be performed when Y < 0.
            synthesisOk &= annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(signBitOfSubtraction);

            // Y = Y + divisor
            synthesisOk &= inplaceAdd(annotatableQuantumComputation, divisor, truncatedAggregateOfRemainderAndQuotientQubits) && annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(signBitOfSubtraction);

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

    bool SyrecSynthesis::inplaceAdd(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs, const std::optional<qc::Qubit>& optionalCarryOut) {
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

    bool SyrecSynthesis::inplaceSubtract(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        bool synthesisOk = true;
        for (std::size_t i = 0; i < rhs.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(rhs[i]);
        }
        synthesisOk &= inplaceAdd(annotatableQuantumComputation, lhs, rhs);
        for (std::size_t i = 0; i < rhs.size() && synthesisOk; ++i) {
            synthesisOk = annotatableQuantumComputation.addOperationsImplementingNotGate(rhs[i]);
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::lessEquals(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src2, const std::vector<qc::Qubit>& src1) {
        return lessThan(annotatableQuantumComputation, dest, src1, src2) && annotatableQuantumComputation.addOperationsImplementingNotGate(dest);
    }

    bool SyrecSynthesis::lessThan(AnnotatableQuantumComputation& annotatableQuantumComputation, qc::Qubit dest, const std::vector<qc::Qubit>& src1, const std::vector<qc::Qubit>& src2) {
        return decreaseWithCarry(annotatableQuantumComputation, src1, src2, dest) && inplaceAdd(annotatableQuantumComputation, src2, src1);
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
            synthesisOk = annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(src1[i]) && inplaceAdd(annotatableQuantumComputation, partial, sum) && annotatableQuantumComputation.deregisterControlQubitFromPropagationInCurrentScope(src1[i]);
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

    bool SyrecSynthesis::getVariables(const VariableAccess::ptr& variableAccess, std::vector<qc::Qubit>& lines) {
        const std::optional<EvaluatedVariableAccess> evaluatedVariableAccess = evaluateAndValidateVariableAccess(variableAccess, loopMap, firstVariableQubitOffsetLookup);
        if (!evaluatedVariableAccess.has_value()) {
            return false;
        }

        // Bitrange and dimension access only contained expressions that could be evaluated at compile time.
        bool synthesisOfVariableAccessOk = evaluatedVariableAccess->evaluatedDimensionAccess.containedOnlyNumericExpressions ? getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(*evaluatedVariableAccess, lines) : getQubitsForVariableAccessContainingIndicesNotEvaluableAtCompileTime(*evaluatedVariableAccess, lines);

        // Check post condition that any qubit for variable access was fetched
        if (synthesisOfVariableAccessOk && lines.empty()) {
            std::cerr << "Failed to determine accessed qubits for variable access on variable with identifier " << variableAccess->var->name << "\n";
            synthesisOfVariableAccessOk = false;
        }
        return synthesisOfVariableAccessOk;
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
            const auto        expectedQubitIndex  = static_cast<qc::Qubit>(annotatableQuantumComputation.getNqubits());
            const std::string qubitLabel          = InternalQubitLabelBuilder::buildAncillaryQubitLabel(expectedQubitIndex, value);
            auto              inliningInformation = AnnotatableQuantumComputation::InlinedQubitInformation();
            if (shouldQubitInlineInformationBeRecorded()) {
                inliningInformation.inlineStack = inlinedQubitModuleCallStack;
            }

            const std::optional<qc::Qubit> actualQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit(qubitLabel, value, inliningInformation);
            if (!actualQubitIndex.has_value() || *actualQubitIndex != expectedQubitIndex) {
                return std::nullopt;
            }
            constLine = expectedQubitIndex;
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

    std::optional<qc::Qubit> SyrecSynthesis::addVariable(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<unsigned>& dimensions, const Variable::ptr& var, const std::string& arraystr, const std::optional<QubitInliningStack::ptr>& currentModuleCallStack) {
        bool                     couldQubitsForVariableBeAdded = true;
        std::optional<qc::Qubit> firstQubitOfVariable;

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
                const std::optional<qc::Qubit> addedQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit(internalQubitLabel, isGarbageQubit, optionalQubitInliningInformation);
                couldQubitsForVariableBeAdded                  = addedQubitIndex.has_value();
                firstQubitOfVariable                           = (!firstQubitOfVariable.has_value() && couldQubitsForVariableBeAdded) ? addedQubitIndex : firstQubitOfVariable;
            }
        } else {
            const auto        len = static_cast<std::size_t>(dimensions.front());
            const std::vector newDimensions(dimensions.begin() + 1U, dimensions.end());

            for (std::size_t i = 0U; i < len && couldQubitsForVariableBeAdded; ++i) {
                const std::optional<qc::Qubit> addedQubitIndexForValueOfDimension = addVariable(annotatableQuantumComputation, newDimensions, var, arraystr + "[" + std::to_string(i) + "]", currentModuleCallStack);
                couldQubitsForVariableBeAdded                                     = addedQubitIndexForValueOfDimension.has_value();
                firstQubitOfVariable                                              = (!firstQubitOfVariable.has_value() && couldQubitsForVariableBeAdded) ? addedQubitIndexForValueOfDimension : firstQubitOfVariable;
            }
        }
        return firstQubitOfVariable;
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
        if (callStmt == nullptr && uncallStmt == nullptr) {
            std::cerr << "Failed to synthesize module call/uncall due to IR entity of corresponding CallStatement/UncallStatement being null\n";
            return false;
        }

        if (firstVariableQubitOffsetLookup == nullptr) {
            std::cerr << "Internal lookup of offsets to first qubits of variables was null\n";
            return false;
        }

        const std::vector<std::string>& callerProvidedParameterValues = callStmt != nullptr ? callStmt->parameters : uncallStmt->parameters;
        const Module::ptr&              targetModule                  = callStmt != nullptr ? callStmt->target : uncallStmt->target;

        std::unordered_map<std::string_view, qc::Qubit> offsetToFirstQubitPerFormalParameterOfTargetModule;

        // 1. Adjust the references module's parameters to the call arguments
        for (std::size_t i = 0U; i < callerProvidedParameterValues.size(); ++i) {
            assert(!modules.empty());

            const std::string_view&             callerProvidedParameterVariableIdentifier  = callerProvidedParameterValues.at(i);
            const std::optional<Variable::ptr>& matchingParameterOrVariableOfCurrentModule = modules.top()->findParameterOrVariable(callerProvidedParameterVariableIdentifier);
            if (!matchingParameterOrVariableOfCurrentModule.has_value() || matchingParameterOrVariableOfCurrentModule.value() == nullptr) {
                std::cerr << "Failed to find matching parameter or variable of module " << modules.top()->name << " for parameter '" << callerProvidedParameterVariableIdentifier << "' when setting references of parameters of " << (callStmt != nullptr ? "called" : "uncalled") << " module " << targetModule->name;
                return false;
            }

            const auto& formalModuleParameter                                               = targetModule->parameters.at(i);
            offsetToFirstQubitPerFormalParameterOfTargetModule[formalModuleParameter->name] = 0;
            // Since we have not opened a new variable qubit offset scope to register the offsets for the parameters as well as for the local variables of the called/uncalled module (target module) our search for the first qubits
            // of the caller provided arguments of the target module can be restricted to the current activate variable qubit offset lookup scope.
            // Additionally, due to the parser already verifying that all variable declarations inside of the target module are unique allows one to simply create the lookup information for the first qubits of the
            // parameters of the target module as the mapping (caller argument -> parameter). To determine the first qubit of a parameter used in a VariableAccess in any statement of the target module is then
            // equal to a simple lookup in the syrec::FirstVariableQubitOffsetLookup using the parameter identifier thus any potential name clashes arising between the local variable identifiers and the mapping caller argument -> parameter
            // is prevented. An example for a name clash if we were to use the reference chain from caller argument -> parameter in a nested call/uncall of a module is:
            //
            // module add(inout a(4))
            //   wire x(4)
            //   a += x
            //
            //  module main(inout x(4))
            //    call add(x) // Using the identifier of the caller argument to determine the first qubit of the formal parameter 'a' in the called module would result in a name clash between the local variable and caller argument
            const std::optional<qc::Qubit> offsetToFirstQubitOfParameterValue = firstVariableQubitOffsetLookup->getOffsetToFirstQubitOfVariableInCurrentScope(callerProvidedParameterVariableIdentifier);
            if (!offsetToFirstQubitOfParameterValue.has_value()) {
                std::cerr << "Failed to determine offset to first qubit of variable '" << callerProvidedParameterVariableIdentifier << "' while trying to set reference for parameter " << formalModuleParameter->name << " of " << (callStmt != nullptr ? "called" : "uncalled") << " module " << targetModule->name << "\n";
                return false;
            }

            offsetToFirstQubitPerFormalParameterOfTargetModule[formalModuleParameter->name] = *offsetToFirstQubitOfParameterValue;
        }

        if (!offsetToFirstQubitPerFormalParameterOfTargetModule.empty()) {
            firstVariableQubitOffsetLookup->openNewVariableQubitOffsetScope();
            for (const auto& [formalParameterIdentifier, offsetToFirstQubit]: offsetToFirstQubitPerFormalParameterOfTargetModule) {
                if (!firstVariableQubitOffsetLookup->registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(formalParameterIdentifier, offsetToFirstQubit)) {
                    std::cerr << "Failed to register offset to first qubit of module parameter '" << formalParameterIdentifier << "' of " << (callStmt != nullptr ? "called" : "uncalled") << " module " << targetModule->name << "\n";
                    return false;
                }
            }
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
            synthesisOfModuleBodyOk = std::ranges::all_of(statements, [&](const Statement::ptr& stmt) { return processStatement(stmt); });
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

        if (!offsetToFirstQubitPerFormalParameterOfTargetModule.empty() && !firstVariableQubitOffsetLookup->closeVariableQubitOffsetScope()) {
            std::cerr << "Failed to close qubit offset scope for parameters and local variables during cleanup after synthesis of " << (callStmt != nullptr ? "called" : "uncalled") << " module " << targetModule->name << "\n";
            return false;
        }
        modules.pop();
        return synthesisOfModuleBodyOk;
    }

    [[nodiscard]] std::optional<SyrecSynthesis::EvaluatedBitrangeAccess> SyrecSynthesis::evaluateAndValidateBitrangeAccess(const VariableAccess& userDefinedVariableAccess, const Number::LoopVariableMapping& loopVariableValueLookup) {
        assert(userDefinedVariableAccess.var != nullptr);
        const unsigned          accessedVariableBitwidth   = userDefinedVariableAccess.var->bitwidth;
        const std::string_view& accessedVariableIdentifier = userDefinedVariableAccess.var->name;

        unsigned evaluatedBitrangeStartValue = 0U;
        unsigned evaluatedBitrangeEndValue   = accessedVariableBitwidth - 1;
        if (!userDefinedVariableAccess.range.has_value()) {
            return EvaluatedBitrangeAccess({.bitrangeStart = evaluatedBitrangeStartValue, .bitrangeEnd = evaluatedBitrangeEndValue});
        }

        if (const std::optional<unsigned> evaluationResultOfBitrangeStart = userDefinedVariableAccess.range->first->tryEvaluate(loopVariableValueLookup); evaluationResultOfBitrangeStart.has_value()) {
            evaluatedBitrangeStartValue = *evaluationResultOfBitrangeStart;
        } else {
            std::cerr << "Failed to determine value of bitrange start in access on variable " << accessedVariableIdentifier << "\n";
            return std::nullopt;
        }

        if (evaluatedBitrangeStartValue >= accessedVariableBitwidth) {
            std::cerr << "User defined bitrange start value '" << std::to_string(evaluatedBitrangeStartValue) << "' was not within the valid range [0, " << std::to_string(accessedVariableBitwidth) << "] in bitrange access on variable " << accessedVariableIdentifier << "\n";
            return std::nullopt;
        }

        if (const std::optional<unsigned> evaluationResultOfBitrangeEnd = userDefinedVariableAccess.range->second->tryEvaluate(loopVariableValueLookup); evaluationResultOfBitrangeEnd.has_value()) {
            evaluatedBitrangeEndValue = *evaluationResultOfBitrangeEnd;
        } else {
            std::cerr << "Failed to determine value of bitrange start in access on variable " << accessedVariableIdentifier << "\n";
            return std::nullopt;
        }

        if (evaluatedBitrangeEndValue >= accessedVariableBitwidth) {
            std::cerr << "User defined bitrange end value '" << std::to_string(evaluatedBitrangeEndValue) << "' was not within the valid range [0, " << std::to_string(accessedVariableBitwidth) << "] in bitrange access on variable " << accessedVariableIdentifier << "\n";
            return std::nullopt;
        }
        return EvaluatedBitrangeAccess({.bitrangeStart = evaluatedBitrangeStartValue, .bitrangeEnd = evaluatedBitrangeEndValue});
    }

    [[nodiscard]] std::optional<SyrecSynthesis::EvaluatedDimensionAccess> SyrecSynthesis::evaluateAndValidateDimensionAccess(const VariableAccess& userDefinedVariableAccess, const Number::LoopVariableMapping& loopVariableValueLookup) {
        assert(userDefinedVariableAccess.var != nullptr);
        const std::string_view& accessedVariableIdentifier = userDefinedVariableAccess.var->name;
        if (userDefinedVariableAccess.indexes.size() != userDefinedVariableAccess.var->dimensions.size()) {
            std::cerr << "The number of indices (" << std::to_string(userDefinedVariableAccess.indexes.size()) << ") defined in a variable access must match the number of dimensions (" << std::to_string(userDefinedVariableAccess.var->dimensions.size()) << ") of the accessed variable " << accessedVariableIdentifier << "\n";
            return std::nullopt;
        }

        std::size_t              dimensionIdx             = 0;
        EvaluatedDimensionAccess evaluatedDimensionAccess = {.containedOnlyNumericExpressions = true, .accessedValuePerDimension = std::vector<std::optional<unsigned>>(userDefinedVariableAccess.var->dimensions.size(), std::nullopt)};

        for (const auto& dimensionExpr: userDefinedVariableAccess.indexes) {
            if (dimensionExpr == nullptr) {
                std::cerr << "Expression defining index for dimension " << std::to_string(dimensionIdx) << " in variable access on " << accessedVariableIdentifier << " cannot be NULL\n";
                return std::nullopt;
            }
            if (const auto& dimensionExprAsNumericExpr = std::dynamic_pointer_cast<NumericExpression>(dimensionExpr); dimensionExprAsNumericExpr != nullptr) {
                if (const std::optional<unsigned> evaluatedDimensionExpr = dimensionExprAsNumericExpr->value != nullptr ? dimensionExprAsNumericExpr->value->tryEvaluate(loopVariableValueLookup) : std::nullopt; evaluatedDimensionExpr.has_value()) {
                    if (*evaluatedDimensionExpr >= userDefinedVariableAccess.var->dimensions.at(dimensionIdx)) {
                        std::cerr << "Access on value " << std::to_string(*evaluatedDimensionExpr) << " of dimension " << std::to_string(dimensionIdx) << " was not within the valid range [0, " << std::to_string(userDefinedVariableAccess.var->dimensions.at(dimensionIdx)) << " in access on variable " << accessedVariableIdentifier << "\n";
                        return std::nullopt;
                    }
                    evaluatedDimensionAccess.accessedValuePerDimension[dimensionIdx] = evaluatedDimensionExpr;
                } else {
                    std::cerr << "Failed to evaluate defined value for numeric expression defined in dimension " << std::to_string(dimensionIdx) << " in variable access on " << accessedVariableIdentifier << "\n";
                    return std::nullopt;
                }
            } else {
                evaluatedDimensionAccess.containedOnlyNumericExpressions = false;
            }
            ++dimensionIdx;
        }
        return evaluatedDimensionAccess;
    }

    std::optional<SyrecSynthesis::EvaluatedVariableAccess> SyrecSynthesis::evaluateAndValidateVariableAccess(const VariableAccess::ptr& userDefinedVariableAccess, const Number::LoopVariableMapping& loopVariableValueLookup, const std::unique_ptr<FirstVariableQubitOffsetLookup>& firstVariableQubitOffsetLookup) {
        if (userDefinedVariableAccess == nullptr) {
            std::cerr << "Cannot synthesis variable access that is null\n";
            return std::nullopt;
        }
        if (userDefinedVariableAccess->var == nullptr) {
            std::cerr << "Cannot synthesis variable access in which the accessed variable is null\n";
            return std::nullopt;
        }

        qc::Qubit offsetToFirstQubitOfVariable = 0;
        if (const std::optional<qc::Qubit> determinedOffsetToFirstQubitOfVariableFromLookup = firstVariableQubitOffsetLookup != nullptr ? firstVariableQubitOffsetLookup->getOffsetToFirstQubitOfVariableInCurrentScope(userDefinedVariableAccess->var->name) : std::nullopt; determinedOffsetToFirstQubitOfVariableFromLookup.has_value()) {
            offsetToFirstQubitOfVariable = *determinedOffsetToFirstQubitOfVariableFromLookup;
        } else {
            std::cerr << "Failed to determine first qubit for variable with identifier " << userDefinedVariableAccess->var->name << "\n";
            return std::nullopt;
        }

        const std::optional<EvaluatedDimensionAccess> evaluatedDimensionAccess = evaluateAndValidateDimensionAccess(*userDefinedVariableAccess, loopVariableValueLookup);
        const std::optional<EvaluatedBitrangeAccess>  evaluatedBitrangeAccess  = evaluateAndValidateBitrangeAccess(*userDefinedVariableAccess, loopVariableValueLookup);
        if (evaluatedBitrangeAccess.has_value() && evaluatedDimensionAccess.has_value()) {
            return EvaluatedVariableAccess({.offsetToFirstQubitOfVariable = offsetToFirstQubitOfVariable, .accessedVariable = *userDefinedVariableAccess->var, .evaluatedBitrangeAccess = *evaluatedBitrangeAccess, .evaluatedDimensionAccess = *evaluatedDimensionAccess, .userDefinedDimensionAccess = userDefinedVariableAccess->indexes});
        }
        return std::nullopt;
    }

    [[nodiscard]] bool SyrecSynthesis::getQubitsForVariableAccessContainingOnlyIndicesEvaluableAtCompileTime(const EvaluatedVariableAccess& evaluatedVariableAccess, std::vector<qc::Qubit>& containerForAccessedQubits) {
        if (!evaluatedVariableAccess.evaluatedDimensionAccess.containedOnlyNumericExpressions) {
            std::cerr << "Synthesis of variable access containing only indices evaluable at compile time could not be performed due to evaluated variable access indicating that not all indices could be evaluated at compile time\n";
            return false;
        }

        const Variable&                accessedVariable        = evaluatedVariableAccess.accessedVariable;
        const EvaluatedBitrangeAccess& evaluatedBitrangeAccess = evaluatedVariableAccess.evaluatedBitrangeAccess;
        const auto& [_, accessedValuePerDimension]             = evaluatedVariableAccess.evaluatedDimensionAccess;

        // Add dimension access offset to first qubit of variable
        auto containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements   = accessedVariable.dimensions;
        containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.back() = 1U;

        for (std::size_t offset = 2U; offset <= containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.size(); ++offset) {
            const std::size_t idxToCurrentElemInOffsetContainer                                                      = accessedVariable.dimensions.size() - offset;
            const std::size_t idxToPrevElemInOffsetContainer                                                         = idxToCurrentElemInOffsetContainer + 1U;
            containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.at(idxToCurrentElemInOffsetContainer) = containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.at(idxToPrevElemInOffsetContainer) * accessedVariable.dimensions.at(idxToPrevElemInOffsetContainer);
        }

        unsigned offsetToAccessedValue = 0U;
        for (std::size_t i = 0; i < accessedValuePerDimension.size(); ++i) {
            if (!accessedValuePerDimension.at(i).has_value()) {
                std::cerr << "Failed to fetch accessed value of dimension " << std::to_string(i) << " in evaluated variable access that only contained compile time constant indices, this should not happen\n";
                return false;
            }
            offsetToAccessedValue += *accessedValuePerDimension.at(i) * containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.at(i);
        }
        // Determine final offset to first accessed qubit by adding offset from bitrange start
        const qc::Qubit currQubitIdx = evaluatedVariableAccess.offsetToFirstQubitOfVariable + (offsetToAccessedValue * accessedVariable.bitwidth);
        containerForAccessedQubits   = evaluatedBitrangeAccess.getIndicesOfAccessedBits();
        std::ranges::for_each(containerForAccessedQubits, [currQubitIdx](qc::Qubit& qubitIdx) { return qubitIdx += currQubitIdx; });
        return true;
    }

    [[nodiscard]] bool SyrecSynthesis::getQubitsForVariableAccessContainingIndicesNotEvaluableAtCompileTime(const EvaluatedVariableAccess& evaluatedVariableAccess, std::vector<qc::Qubit>& containerForAccessedQubits) {
        const std::size_t numQubitsAccessedByBitrangeAccess = evaluatedVariableAccess.evaluatedBitrangeAccess.getIndicesOfAccessedBits().size();
        bool              synthesisOk                       = getConstantLines(static_cast<unsigned>(numQubitsAccessedByBitrangeAccess), 0U, containerForAccessedQubits);

        // Generate ancillary qubits storing unrolled index
        std::vector<qc::Qubit> ancillaryQubitsStoringUnrolledIndex;
        synthesisOk &= calculateSymbolicUnrolledIndexForElementInVariable(evaluatedVariableAccess, ancillaryQubitsStoringUnrolledIndex);

        synthesisOk &= transferQubitsOfElementAtIndexInVariableToOtherQubits(evaluatedVariableAccess, ancillaryQubitsStoringUnrolledIndex, containerForAccessedQubits, QubitTransferOperation::CopyValue);
        return synthesisOk;
    }

    bool SyrecSynthesis::calculateSymbolicUnrolledIndexForElementInVariable(const EvaluatedVariableAccess& evaluatedVariableAccess, std::vector<qc::Qubit>& containerToStoreUnrolledIndex) {
        assert(containerToStoreUnrolledIndex.empty());

        const Variable&        accessedVariable          = evaluatedVariableAccess.accessedVariable;
        const Expression::vec& accessedIndexPerDimension = evaluatedVariableAccess.userDefinedDimensionAccess;
        assert(accessedIndexPerDimension.size() == accessedVariable.dimensions.size());

        const std::size_t numDimensionsOfAccessedVariable                                    = accessedVariable.dimensions.size();
        auto              containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements = accessedVariable.dimensions;
        containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.back()            = 1U;

        for (std::size_t offset = 2U; offset <= containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.size(); ++offset) {
            const std::size_t idxToCurrentElemInOffsetContainer                                                      = numDimensionsOfAccessedVariable - offset;
            const std::size_t idxToPrevElemInOffsetContainer                                                         = idxToCurrentElemInOffsetContainer + 1U;
            containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.at(idxToCurrentElemInOffsetContainer) = containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.at(idxToPrevElemInOffsetContainer) * accessedVariable.dimensions.at(idxToPrevElemInOffsetContainer);
        }

        // Determine how many qubits are necessary to store the unrolled index to any element in the accessed variable
        const unsigned numElementsInAccessedVariable                               = determineNumberOfElementsInVariable(accessedVariable);
        const unsigned numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable = determineNumberOfBitsRequiredToStoreValue(numElementsInAccessedVariable);

        // Generate ancillary qubits storing unrolled index
        bool synthesisOk = getConstantLines(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, 0U, containerToStoreUnrolledIndex);

        std::optional<unsigned> compileTimeValueOfUnrolledIndex = 0U;
        // Calculate unrolled index
        for (std::size_t i = 0; i < numDimensionsOfAccessedVariable && synthesisOk; ++i) {
            const unsigned offsetToNextElementOfDimensionInNumberOfArrayElements = containerForOffsetsToNextElementOfDimensionInNumberOfArrayElements.at(i);

            // Integer constants (compile time constant expressions defined in the dimension access are assumed to have been evaluated during the validation of the dimension access) are assumed to have a default bitwidth of 32
            // if no bitwidth restriction exists (i.e. defined by the bitwidth of the assigned to variable of an assignment). However, to calculate the unrolled index one or more addition/multiplication operations need to be synthesized
            // with the addition operation requiring that both summands have the same bitwidth thus we need to truncate the bitwidth and value of the integer constant to the required bitwidth which is equal to the bitwidth required to
            // store the index to any value of the accessed variable (i.e. for a variable a[2][3](<BITWIDTH>) one would need 3 bits to store the maximum possible index value 5 [assuming zero-based indexing]).
            if (const auto* userDefinedIndexExprAsNumericOne = dynamic_cast<NumericExpression*>(accessedIndexPerDimension.at(i).get()); userDefinedIndexExprAsNumericOne != nullptr && userDefinedIndexExprAsNumericOne->bitwidth() > numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable) {
                const std::optional<unsigned> constantValueOfExprEvaluatedToCompileTime = evaluatedVariableAccess.evaluatedDimensionAccess.accessedValuePerDimension.at(i);
                assert(constantValueOfExprEvaluatedToCompileTime.has_value());

                // TODO: Integer truncation operation is currently hard coded but option from synthesis settings should be use if available in the future
                unsigned evaluatedCompileTimeValueOfExpr = utils::truncateConstantValueToExpectedBitwidth(*constantValueOfExprEvaluatedToCompileTime, numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, utils::IntegerConstantTruncationOperation::Modulo);
                evaluatedCompileTimeValueOfExpr          = utils::truncateConstantValueToExpectedBitwidth(evaluatedCompileTimeValueOfExpr * offsetToNextElementOfDimensionInNumberOfArrayElements, numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, utils::IntegerConstantTruncationOperation::Modulo);
                // We might be able to compute parts of the unrolled index at compile time.
                if (compileTimeValueOfUnrolledIndex.has_value()) {
                    compileTimeValueOfUnrolledIndex = *compileTimeValueOfUnrolledIndex + evaluatedCompileTimeValueOfExpr;
                } else if (evaluatedCompileTimeValueOfExpr != 0) {
                    std::vector<qc::Qubit> qubitsStoringUnrolledIndexSummandForDimension;
                    synthesisOk &= getConstantLines(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, 0U, qubitsStoringUnrolledIndexSummandForDimension) && moveIntegerValueToAncillaryQubits(annotatableQuantumComputation, qubitsStoringUnrolledIndexSummandForDimension, evaluatedCompileTimeValueOfExpr) && assignAdd(containerToStoreUnrolledIndex, qubitsStoringUnrolledIndexSummandForDimension, AssignStatement::AssignOperation::Add) && clearIntegerValueFromAncillaryQubits(annotatableQuantumComputation, qubitsStoringUnrolledIndexSummandForDimension, evaluatedCompileTimeValueOfExpr);
                }
            } else {
                compileTimeValueOfUnrolledIndex.reset();

                const std::size_t      numOperationsPriorToSynthesisOfExpr = annotatableQuantumComputation.getNops();
                std::vector<qc::Qubit> qubitsStoringSynthesizedExprOfDimension;
                // We do not need to manually generate ancillary qubits here since they are generated during the synthesis of the expression (or qubits of a variable simply copied to our container in case of a variable access with only compile time constant expressions)
                if (!onExpression(accessedIndexPerDimension.at(i), qubitsStoringSynthesizedExprOfDimension, {}, BinaryExpression::BinaryOperation::Add)) {
                    std::cerr << "Failed to synthesis index expression for dimension " << std::to_string(i) << " of dimension access for variable access on variable " << accessedVariable.name << "\n";
                    return false;
                }

                const std::size_t numOperationsAfterSynthesisOfExpr = annotatableQuantumComputation.getNops();
                // The bitwidth of synthesized expression could be smaller/larger than the one storing the unrolled index with the former needing to be truncated/enlarged so that the subsequent addition operation can be synthesized
                // with the addition operation requiring the same operand bitwidth. Due to this condition, we think that bitwidth of the index expression should not be larger than the bitwidth required to store the unrolled index.
                // A smaller bitwidth should be allowed but needs to be padded to the required bitwidth.
                if (qubitsStoringSynthesizedExprOfDimension.size() > numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable) { // An index out of range value should have been already detected during the evaluation and validation of the dimension access that is assumed to have been performed prior to this call.
                    std::cerr << "Bitwidth of expression (" << std::to_string(qubitsStoringSynthesizedExprOfDimension.size()) << ") can be at most be as large as the number of qubits (" << std::to_string(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable) << ") required to store the maximum possible unrolled index in the accessed variable " << accessedVariable.name << "\n";
                    return false;
                }

                if (qubitsStoringSynthesizedExprOfDimension.size() < numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable) {
                    const unsigned         qubitContainersSizeDifference = numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable - static_cast<unsigned>(qubitsStoringSynthesizedExprOfDimension.size());
                    std::vector<qc::Qubit> paddingQubits;
                    synthesisOk &= getConstantLines(qubitContainersSizeDifference, 0U, paddingQubits);
                    qubitsStoringSynthesizedExprOfDimension.insert(qubitsStoringSynthesizedExprOfDimension.end(), paddingQubits.cbegin(), paddingQubits.cend());
                }

                std::optional<std::size_t> numOperationsPriorToSynthesisOfSummandInUnrolledIndexSum;
                std::optional<std::size_t> numOperationsAfterSynthesisOfSummandInUnrolledIndexSum;

                std::vector<qc::Qubit> qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex;
                if (offsetToNextElementOfDimensionInNumberOfArrayElements == 1) {
                    qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex = qubitsStoringSynthesizedExprOfDimension;
                } else if (std::has_single_bit(offsetToNextElementOfDimensionInNumberOfArrayElements)) {
                    numOperationsPriorToSynthesisOfSummandInUnrolledIndexSum = annotatableQuantumComputation.getNops();
                    synthesisOk &= getConstantLines(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, 0U, qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex) && leftShift(annotatableQuantumComputation, qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex, qubitsStoringSynthesizedExprOfDimension, offsetToNextElementOfDimensionInNumberOfArrayElements / 2);
                    numOperationsAfterSynthesisOfSummandInUnrolledIndexSum = annotatableQuantumComputation.getNops();
                } else {
                    numOperationsPriorToSynthesisOfSummandInUnrolledIndexSum = annotatableQuantumComputation.getNops();
                    std::vector<qc::Qubit> qubitsStoringOffsetToNextElementOfDimensionInNumberOfArrayElements;
                    synthesisOk &= getConstantLines(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, 0U, qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex) && getConstantLines(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, 0U, qubitsStoringOffsetToNextElementOfDimensionInNumberOfArrayElements) && moveIntegerValueToAncillaryQubits(annotatableQuantumComputation, qubitsStoringOffsetToNextElementOfDimensionInNumberOfArrayElements, offsetToNextElementOfDimensionInNumberOfArrayElements) && multiplication(annotatableQuantumComputation, qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex, qubitsStoringSynthesizedExprOfDimension, qubitsStoringOffsetToNextElementOfDimensionInNumberOfArrayElements);
                    numOperationsAfterSynthesisOfSummandInUnrolledIndexSum = annotatableQuantumComputation.getNops();
                }
                synthesisOk &= assignAdd(containerToStoreUnrolledIndex, qubitsStoringSymbolicValueOfSummandOfDimensionForUnrolledIndex, AssignStatement::AssignOperation::Add);

                // We can reset the state of the ancillary qubits used to calculate the summand S = <offset_to_next_element> * <index_of_dimension> back to their initial state since they are no longer needed after the summand was added
                // to the unrolled index by simply replaying the used operations in reverse order. This reset would allow for the ancillary qubits to be reused in future operation. We need to use the syrec::AnnotatableQuantumComputation::replayOperationsAtGivenIndexRange(...) to replay the
                // operations instead of manually adding the qc::Operation via the qc::QuantumComputation base class since the former will add the required gate annotations to the replayed operations which the latter will not.
                if (numOperationsPriorToSynthesisOfSummandInUnrolledIndexSum.has_value() && numOperationsPriorToSynthesisOfSummandInUnrolledIndexSum > 0) {
                    if (!numOperationsAfterSynthesisOfSummandInUnrolledIndexSum.has_value()) {
                        std::cerr << "Failed to undo quantum operations required to calculate summand of dimension " << std::to_string(i) << "for unrolled index sum\n";
                        return false;
                    }

                    const std::size_t idxOfFirstRelevantOperation = *numOperationsAfterSynthesisOfSummandInUnrolledIndexSum - 1;
                    const std::size_t idxOfLastRelevantOperation  = *numOperationsPriorToSynthesisOfSummandInUnrolledIndexSum;
                    synthesisOk &= annotatableQuantumComputation.replayOperationsAtGivenIndexRange(idxOfFirstRelevantOperation, idxOfLastRelevantOperation);
                }

                if (numOperationsPriorToSynthesisOfExpr > 0 && numOperationsPriorToSynthesisOfExpr != numOperationsAfterSynthesisOfExpr) {
                    // After the summand generated for the current dimension is added to the unrolled index (and the operations for the summand assumed to be reset at this point) one can also undo the operations required to synthesize the user-defined expression
                    // for the current dimension to reset the used ancillary qubits back to their initial state using the same procedure as for the summand.
                    const std::size_t idxOfFirstRelevantOperation = numOperationsAfterSynthesisOfExpr - 1;
                    const std::size_t idxOfLastRelevantOperation  = numOperationsPriorToSynthesisOfExpr;
                    synthesisOk &= annotatableQuantumComputation.replayOperationsAtGivenIndexRange(idxOfFirstRelevantOperation, idxOfLastRelevantOperation);
                }
            }
        }
        return synthesisOk;
    }

    bool SyrecSynthesis::transferQubitsOfElementAtIndexInVariableToOtherQubits(const EvaluatedVariableAccess& evaluatedVariableAccess, const std::vector<qc::Qubit>& qubitsStoringUnrolledIndexOfElementToSelect, const std::vector<qc::Qubit>& qubitsStoringResultOfTransferOperation, const QubitTransferOperation qubitTransferOperation) {
        if (qubitTransferOperation != QubitTransferOperation::SwapQubits && qubitTransferOperation != QubitTransferOperation::CopyValue) {
            std::cerr << "Invalid qubit transfer operation defined\n";
            return false;
        }

        const Variable&                accessedVariable                     = evaluatedVariableAccess.accessedVariable;
        const EvaluatedBitrangeAccess& evaluatedBitrangeAccess              = evaluatedVariableAccess.evaluatedBitrangeAccess;
        const unsigned                 offsetToFirstQubitOfAccessedVariable = evaluatedVariableAccess.offsetToFirstQubitOfVariable;

        if (const std::size_t numQubitsAccessedByBitrange = evaluatedBitrangeAccess.getIndicesOfAccessedBits().size(); numQubitsAccessedByBitrange != qubitsStoringResultOfTransferOperation.size()) {
            std::cerr << "Tried to perform a conditional swap of the " << std::to_string(numQubitsAccessedByBitrange) << " qubits of the accessed bitrange with the provided " << std::to_string(qubitsStoringResultOfTransferOperation.size()) << " qubits\n";
            return false;
        }

        bool              synthesisOk                                                 = true;
        const std::size_t numElementsInAccessedVariable                               = determineNumberOfElementsInVariable(accessedVariable);
        const unsigned    numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable = determineNumberOfBitsRequiredToStoreValue(static_cast<unsigned>(numElementsInAccessedVariable));

        std::vector<qc::Qubit> ancillaryQubitsStoringCurrentIndex;
        synthesisOk &= getConstantLines(numQubitsRequiredToStoreIndexToAnyElementInAccessedVariable, 0U, ancillaryQubitsStoringCurrentIndex);

        qc::Qubit qubitOffsetToCurrentElementInAccessedVariable = offsetToFirstQubitOfAccessedVariable;
        // Since the start qubit is allowed to be larger than the end qubit in a bitrange access of a variable access, we determine the offsets to the qubits accessed with the defined bitrange in the bitwidht of the variable
        // e.g. assuming a variable declaration 'in a[2][3](4)' then the offsets generated for a variable access 'a[0][1].2:1' are (2, 1) while the offsets for 'a[0][1].1:2' are (1, 2).
        const std::vector<qc::Qubit> relativeQubitOffsetForAccessedQubitsInElement = evaluatedBitrangeAccess.getIndicesOfAccessedBits();

        const qc::Controls controlQubitsFromCompareOperation(ancillaryQubitsStoringCurrentIndex.cbegin(), ancillaryQubitsStoringCurrentIndex.cend());
        for (std::size_t i = 0; i < numElementsInAccessedVariable && synthesisOk; ++i) {
            // Move current index to ancillary qubits and compare with unrolled index with the result of the operation being stored in the qubits storing the current index.
            // The latter qubits are then used as control qubits to perform the qubit-wise transfer of the qubits of the currently accessed element to the result qubits.
            synthesisOk &= checkIfQubitsMatchAndStoreResultInRhsOperandQubits(annotatableQuantumComputation, qubitsStoringUnrolledIndexOfElementToSelect, ancillaryQubitsStoringCurrentIndex, false);

            annotatableQuantumComputation.activateControlQubitPropagationScope();
            for (const qc::Control controlQubit: controlQubitsFromCompareOperation) {
                synthesisOk &= annotatableQuantumComputation.registerControlQubitForPropagationInCurrentAndNestedScopes(controlQubit.qubit);
            }

            for (std::size_t j = 0; j < relativeQubitOffsetForAccessedQubitsInElement.size() && synthesisOk; ++j) {
                const qc::Qubit currAccessedQubitOfVariable = qubitOffsetToCurrentElementInAccessedVariable + relativeQubitOffsetForAccessedQubitsInElement.at(j);

                if (qubitTransferOperation == QubitTransferOperation::SwapQubits) {
                    synthesisOk &= annotatableQuantumComputation.addOperationsImplementingFredkinGate(currAccessedQubitOfVariable, qubitsStoringResultOfTransferOperation.at(j));
                } else {
                    synthesisOk &= annotatableQuantumComputation.addOperationsImplementingCnotGate(currAccessedQubitOfVariable, qubitsStoringResultOfTransferOperation.at(j));
                }
            }
            qubitOffsetToCurrentElementInAccessedVariable += accessedVariable.bitwidth;
            annotatableQuantumComputation.deactivateControlQubitPropagationScope();

            // We reset the qubits originally storing the value of the accessed element in the variable by first reverting the operations used to compare the unrolled index to the index of the accessed element and then incrementing it
            // to advance the index to the next element.
            synthesisOk &= checkIfQubitsMatchAndStoreResultInRhsOperandQubits(annotatableQuantumComputation, qubitsStoringUnrolledIndexOfElementToSelect, ancillaryQubitsStoringCurrentIndex, true) && increment(annotatableQuantumComputation, ancillaryQubitsStoringCurrentIndex);
        }
        // Clear the ancillary qubits storing the current index of the accessed element in the variable back to their initial state (i.e. zero them).
        synthesisOk &= clearIntegerValueFromAncillaryQubits(annotatableQuantumComputation, ancillaryQubitsStoringCurrentIndex, static_cast<unsigned>(numElementsInAccessedVariable));
        return synthesisOk;
    }

    std::vector<unsigned> SyrecSynthesis::EvaluatedBitrangeAccess::getIndicesOfAccessedBits() const {
        std::size_t bitrangeStartAndEndIdxDifference = 0;
        bool        bitrangeStartIdxLargerThanEnd    = false;
        if (bitrangeStart > bitrangeEnd) {
            bitrangeStartAndEndIdxDifference = bitrangeStart - bitrangeEnd;
            bitrangeStartIdxLargerThanEnd    = true;
        } else {
            bitrangeStartAndEndIdxDifference = bitrangeEnd - bitrangeStart;
        }

        unsigned    currBitIdx = bitrangeStart;
        std::vector containerForAccessedBits(bitrangeStartAndEndIdxDifference + 1U, 0U);
        for (qc::Qubit& bitIdx: containerForAccessedBits) {
            bitIdx     = currBitIdx;
            currBitIdx = bitrangeStartIdxLargerThanEnd ? currBitIdx - 1U : currBitIdx + 1U;
        }
        return containerForAccessedBits;
    }
} // namespace syrec
