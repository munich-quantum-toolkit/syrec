/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/components/custom_module_visitor.hpp"

#include "TSyrecParser.h"
#include "Token.h"
#include "core/syrec/module.hpp"
#include "core/syrec/parser/components/custom_statement_visitor.hpp"
#include "core/syrec/parser/utils/custom_error_messages.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/parser/utils/symbolTable/base_symbol_table.hpp"
#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <variant>
#include <vector>

using namespace syrec_parser;

std::optional<std::shared_ptr<syrec::Program>> CustomModuleVisitor::parseProgram(const TSyrecParser::ProgramContext* context) const {
    return visitProgramTyped(context);
}

// START OF NON-PUBLIC FUNCTIONALITY
std::optional<std::shared_ptr<syrec::Program>> CustomModuleVisitor::visitProgramTyped(const TSyrecParser::ProgramContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    std::shared_ptr<const syrec::Module> lastProcessedUserDefinedModule = nullptr;
    auto                                 generatedProgram               = std::make_shared<syrec::Program>();
    for (const auto& antlrModuleContext: context->module()) {
        if (const std::optional<syrec::Module::ptr>& parsedModule = visitModuleTyped(antlrModuleContext); parsedModule.has_value()) {
            generatedProgram->addModule(*parsedModule);
            lastProcessedUserDefinedModule = *parsedModule;
        } else {
            lastProcessedUserDefinedModule.reset();
        }
    }

    std::string                          programEntryPointModuleIdentifier;
    std::shared_ptr<const syrec::Module> definedMainModule = nullptr;

    if (parserConfiguration.optionalProgramEntryPointModuleIdentifier.has_value()) {
        const auto userDefinedMainModuleIdentifierSemanticErrorPosition = Message::Position(0, 0);

        const std::string userDefinedProgramEntryPointModuleIdentifier = parserConfiguration.optionalProgramEntryPointModuleIdentifier.value();
        const std::regex  mainModuleIdentifierValidationRegex("^(_|[a-zA-Z])+\\w*");
        if (!std::regex_match(userDefinedProgramEntryPointModuleIdentifier, mainModuleIdentifierValidationRegex)) {
            recordSemanticError<SemanticError::InvalidUserDefinedProgramEntryPointModuleIdentifier>(userDefinedMainModuleIdentifierSemanticErrorPosition, userDefinedProgramEntryPointModuleIdentifier);
        } else {
            const syrec::Module::vec modulesMatchingIdentifier = symbolTable->getModulesByName(userDefinedProgramEntryPointModuleIdentifier);
            if (modulesMatchingIdentifier.empty()) {
                recordSemanticError<SemanticError::NoModuleMatchingUserDefinedProgramEntryPoint>(userDefinedMainModuleIdentifierSemanticErrorPosition, userDefinedProgramEntryPointModuleIdentifier);
            } else {
                definedMainModule = modulesMatchingIdentifier.back();
            }
        }
    } else {
        if (const syrec::Module::vec modulesMatchingIdentifier = symbolTable->getModulesByName("main"); !modulesMatchingIdentifier.empty()) {
            definedMainModule = modulesMatchingIdentifier.front();
        } else {
            definedMainModule = lastProcessedUserDefinedModule;
        }
    }

    // We are not requiring a C89 style def-before use for both call-/uncall statements, thus we need to perform overload resolution for any of these statements after the whole program was processed.
    const std::vector<CustomStatementVisitor::NotOverloadResolutedCallStatementScope> callStatementsScopeForWhichOverloadResolutionShouldBePerformed = statementVisitorInstance->getCallStatementsWithNotPerformedOverloadResolution();
    for (const auto& scope: callStatementsScopeForWhichOverloadResolutionShouldBePerformed) {
        for (const CustomStatementVisitor::NotOverloadResolutedCallStatementScope::CallStatementData& callStatementVariant: scope.callStatementsToPerformOverloadResolutionOn) {
            // The current module overload resolution algorithm uses the variable type to determine whether the user provided argument is assignable to the formal module parameter, thus a variable of type 'in' is not assignable
            // to a variable of type 'out'/'inout'. If one could prove that the called module will not perform an assignment to the variable that is declared as modifiable, the previously mentioned assignment could be valid but
            // is currently not used in the implementation of the symbol table.
            const utils::BaseSymbolTable::ModuleOverloadResolutionResult overloadResolutionResult = symbolTable->getModulesMatchingSignature(callStatementVariant.calledModuleIdentifier, callStatementVariant.symbolTableEntriesForCallerArguments);
            const auto                                                   semanticErrorPosition    = Message::Position(callStatementVariant.linePositionOfModuleIdentifier, callStatementVariant.columnPositionOfModuleIdentifier);

            if (callStatementVariant.calledModuleIdentifier == programEntryPointModuleIdentifier && definedMainModule != nullptr) {
                recordSemanticError<SemanticError::CannotCallMainModule>(semanticErrorPosition, programEntryPointModuleIdentifier);
                continue;
            }

            if (!symbolTable->existsModuleForName(callStatementVariant.calledModuleIdentifier)) {
                recordSemanticError<SemanticError::NoModuleMatchingIdentifier>(semanticErrorPosition, callStatementVariant.calledModuleIdentifier);
                continue;
            }

            // Recursive module calls are possible since we cannot determine whether this would produce an infinite recursion without performing a symbol execution of the call stack.
            // If no call/uncall statement is defined in the branch of an if-statement whose guard expression does not evaluate to a constant value, an infinite loop check could be performed
            // (but is not performed in the current parser implementation). It is the users responsibility to prevent such infinite recursions.
            if (overloadResolutionResult.resolutionResult != utils::BaseSymbolTable::ModuleOverloadResolutionResult::Result::SingleMatchFound) {
                recordSemanticError<SemanticError::NoModuleMatchingCallSignature>(semanticErrorPosition);
            } else if (overloadResolutionResult.resolutionResult == utils::BaseSymbolTable::ModuleOverloadResolutionResult::Result::SingleMatchFound && overloadResolutionResult.moduleMatchingSignature.has_value() && definedMainModule != nullptr && overloadResolutionResult.moduleMatchingSignature->get()->name == definedMainModule->name && doVariableCollectionsMatch(definedMainModule->parameters, overloadResolutionResult.moduleMatchingSignature->get()->parameters)) {
                // We need to check whether the module (assuming to have the identifier T) defining the entry-point of the SyReC program is not called by the user. In case that no module with the identifier 'main' was found and
                // the user did not explicitly define the expected identifier of the module serving as the entry-point then module overloading is possible even for modules with identifier T since we identify the 'main' module as the last
                // defined module in the SyReC program.
                recordSemanticError<SemanticError::CannotCallMainModule>(semanticErrorPosition, definedMainModule->name);
            } else if (overloadResolutionResult.moduleMatchingSignature.has_value()) {
                if (std::holds_alternative<std::shared_ptr<syrec::CallStatement>>(callStatementVariant.callStatementVariantInstance)) {
                    const auto& callStatementInstance = std::get<std::shared_ptr<syrec::CallStatement>>(callStatementVariant.callStatementVariantInstance);
                    callStatementInstance->target     = overloadResolutionResult.moduleMatchingSignature.value();
                } else if (std::holds_alternative<std::shared_ptr<syrec::UncallStatement>>(callStatementVariant.callStatementVariantInstance)) {
                    const auto& uncallStatementInstance = std::get<std::shared_ptr<syrec::UncallStatement>>(callStatementVariant.callStatementVariantInstance);
                    uncallStatementInstance->target     = overloadResolutionResult.moduleMatchingSignature.value();
                }
            } else {
                recordCustomError(semanticErrorPosition, "Failed to determine target module for call/uncall statement");
            }
        }
    }
    return generatedProgram;
}

std::optional<syrec::Module::ptr> CustomModuleVisitor::visitModuleTyped(const TSyrecParser::ModuleContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    const std::string&         mainModuleIdentifier = parserConfiguration.optionalProgramEntryPointModuleIdentifier.value_or("main");
    std::optional<std::string> moduleIdentifier;
    if (context->literalIdent() != nullptr) {
        moduleIdentifier = context->literalIdent()->getText();
        // According to the SyReC specification (https://revlib.org/doc/docu/revlib_2_0_1.pdf section 2.1): "The top-module of a program is defined by the special identifier main.". However, due to the user
        // being able to define the expected identifier of the program entry point via the syrec::ReadProgramSettings said config entry has precedence over the SyReC specification. In both cases however, only
        // one module can match the "main" modules identifier with violations of this invariant being reported as semantic errors.
        if (moduleIdentifier == mainModuleIdentifier && symbolTable->existsModuleForName(*moduleIdentifier)) {
            recordSemanticError<SemanticError::DuplicateMainModuleDefinition>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), *moduleIdentifier);
        }
        if (moduleIdentifier->starts_with(RESERVED_IDENTIFIER_PREFIX)) {
            recordSemanticError<SemanticError::ReservedIdentifierPrefixUsed>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), *moduleIdentifier, RESERVED_IDENTIFIER_PREFIX);
        }
    }

    auto generatedModule = std::make_shared<syrec::Module>(moduleIdentifier.value_or(""));
    symbolTable->openTemporaryScope();
    generatedModule->parameters = visitParameterListTyped(context->parameterList()).value_or(syrec::Variable::vec());

    for (const auto& antlrLocalVariableContexts: context->signalList()) {
        if (const std::optional<std::vector<syrec::Variable::ptr>> localVariableDefinitions = visitSignalListTyped(antlrLocalVariableContexts); localVariableDefinitions.has_value()) {
            generatedModule->variables.insert(generatedModule->variables.end(), localVariableDefinitions->cbegin(), localVariableDefinitions->cend());
        }
    }

    statementVisitorInstance->openNewScopeToRecordCallStatementsInModule(CustomStatementVisitor::NotOverloadResolutedCallStatementScope::DeclaredModuleSignature(generatedModule->name, generatedModule->parameters));
    generatedModule->statements = visitStatementListTyped(context->statementList()).value_or(syrec::Statement::vec());

    if (moduleIdentifier.has_value()) {
        if (*moduleIdentifier != mainModuleIdentifier) {
            auto moduleOverloadResolutionCall = utils::BaseSymbolTable::ModuleOverloadResolutionResult(utils::BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
            if (context->parameterList() != nullptr && !context->parameterList()->parameter().empty()) {
                moduleOverloadResolutionCall = symbolTable->getModulesMatchingSignature(*moduleIdentifier, generatedModule->parameters);
            } else {
                moduleOverloadResolutionCall = utils::BaseSymbolTable::ModuleOverloadResolutionResult(symbolTable->existsModuleForName(*moduleIdentifier) ? utils::BaseSymbolTable::ModuleOverloadResolutionResult::Result::SingleMatchFound : utils::BaseSymbolTable::ModuleOverloadResolutionResult::Result::NoMatchFound, std::nullopt);
            }
            if (moduleOverloadResolutionCall.resolutionResult != utils::BaseSymbolTable::ModuleOverloadResolutionResult::Result::CallerArgumentsInvalid && moduleOverloadResolutionCall.resolutionResult != utils::BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound) {
                recordSemanticError<SemanticError::DuplicateModuleDeclaration>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), *moduleIdentifier);
            }
        }
        if (generatedModule != nullptr) {
            symbolTable->insertModule(generatedModule);
        }
    }
    symbolTable->closeTemporaryScope();
    return generatedModule;
}

std::optional<std::vector<syrec::Variable::ptr>> CustomModuleVisitor::visitParameterListTyped(const TSyrecParser::ParameterListContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    syrec::Variable::vec processedParameterInstances;
    processedParameterInstances.reserve(context->parameter().size());

    const std::optional<utils::TemporaryVariableScope::ptr> activeSymbolTableScope = symbolTable->getActiveTemporaryScope();
    for (const auto& antlrParameterContext: context->parameter()) {
        // Not recording declared parameters will lead to overload resolution reporting 'false' errors (or fails to emit them when the semantically or syntactically incorrect module is not recorded in the symbol table)
        // which is an acceptable behaviour in case that the user failed to provide a valid module declaration
        if (const std::optional<syrec::Variable::ptr>& generatedParameterInstance = visitParameterTyped(antlrParameterContext); generatedParameterInstance.has_value()) {
            processedParameterInstances.emplace_back(*generatedParameterInstance);
            if (activeSymbolTableScope.has_value()) {
                activeSymbolTableScope->get()->recordVariable(*generatedParameterInstance);
            }
        }
    }
    return processedParameterInstances;
}

std::optional<syrec::Variable::ptr> CustomModuleVisitor::visitParameterTyped(const TSyrecParser::ParameterContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    std::optional<syrec::Variable::Type> parameterVariableType;
    if (context->literalVarTypeIn() != nullptr) {
        parameterVariableType = syrec::Variable::Type::In;
    } else if (context->literalVarTypeInout() != nullptr) {
        parameterVariableType = syrec::Variable::Type::Inout;
    } else if (context->literalVarTypeOut() != nullptr) {
        parameterVariableType = syrec::Variable::Type::Out;
    }

    if (const std::optional<syrec::Variable::ptr>& generatedParameterInstance = visitSignalDeclarationTyped(context->signalDeclaration()); generatedParameterInstance.has_value() && parameterVariableType.has_value()) {
        generatedParameterInstance->get()->type = *parameterVariableType;
        return generatedParameterInstance;
    }
    return std::nullopt;
}

std::optional<std::vector<syrec::Variable::ptr>> CustomModuleVisitor::visitSignalListTyped(const TSyrecParser::SignalListContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    std::optional<syrec::Variable::Type> localVariableType;
    if (context->literalVarTypeState() != nullptr) {
        localVariableType = syrec::Variable::Type::State;
    } else if (context->literalVarTypeWire() != nullptr) {
        localVariableType = syrec::Variable::Type::Wire;
    }

    syrec::Variable::vec processedLocalVariables;
    processedLocalVariables.reserve(context->signalDeclaration().size());

    const std::optional<utils::TemporaryVariableScope::ptr> activeSymbolTableScope = symbolTable->getActiveTemporaryScope();
    for (const auto& antlrVariableDeclarationToken: context->signalDeclaration()) {
        if (const std::optional<syrec::Variable::ptr>& generatedLocalVariableDeclaration = visitSignalDeclarationTyped(antlrVariableDeclarationToken); generatedLocalVariableDeclaration.has_value() && localVariableType.has_value()) {
            generatedLocalVariableDeclaration->get()->type = *localVariableType;
            processedLocalVariables.emplace_back(*generatedLocalVariableDeclaration);
            if (activeSymbolTableScope.has_value()) {
                activeSymbolTableScope->get()->recordVariable(*generatedLocalVariableDeclaration);
            }
        }
    }
    return processedLocalVariables;
}

std::optional<syrec::Variable::ptr> CustomModuleVisitor::visitSignalDeclarationTyped(const TSyrecParser::SignalDeclarationContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    const std::optional<utils::TemporaryVariableScope::ptr> activeSymbolTableScope = symbolTable->getActiveTemporaryScope();
    const std::optional<std::string>                        variableIdentifier     = context->literalIdent() != nullptr ? std::make_optional(context->literalIdent()->getText()) : std::nullopt;
    if (variableIdentifier.has_value()) {
        if (variableIdentifier->starts_with(RESERVED_IDENTIFIER_PREFIX)) {
            recordSemanticError<SemanticError::ReservedIdentifierPrefixUsed>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), *variableIdentifier, RESERVED_IDENTIFIER_PREFIX);
        }
        if (activeSymbolTableScope.has_value() && activeSymbolTableScope->get()->existsVariableForName(*variableIdentifier)) {
            recordSemanticError<SemanticError::DuplicateVariableDeclaration>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), *variableIdentifier);
        }
    }

    std::vector<unsigned int> declaredNumberOfValuesPerDimension;
    declaredNumberOfValuesPerDimension.reserve(context->dimensionTokens.size());

    for (std::size_t i = 0; i < context->dimensionTokens.size(); ++i) {
        const auto& antlrTokenForDeclaredNumberOfValuesOfDimension = context->dimensionTokens.at(i);
        if (antlrTokenForDeclaredNumberOfValuesOfDimension == nullptr) {
            continue;
        }

        bool                              didIntegerConstantDeserializationFailToDueOverflow = false;
        const std::optional<unsigned int> parsedIntegerConstantFromAntlrToken                = deserializeConstantFromString(antlrTokenForDeclaredNumberOfValuesOfDimension->getText(), &didIntegerConstantDeserializationFailToDueOverflow);

        if (parsedIntegerConstantFromAntlrToken.has_value()) {
            if (*parsedIntegerConstantFromAntlrToken == 0) {
                recordSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(mapTokenPositionToMessagePosition(*antlrTokenForDeclaredNumberOfValuesOfDimension), i);
                declaredNumberOfValuesPerDimension.emplace_back(0);
            } else {
                declaredNumberOfValuesPerDimension.emplace_back(*parsedIntegerConstantFromAntlrToken);
            }
        } else {
            if (didIntegerConstantDeserializationFailToDueOverflow) {
                recordSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(mapTokenPositionToMessagePosition(*antlrTokenForDeclaredNumberOfValuesOfDimension), antlrTokenForDeclaredNumberOfValuesOfDimension->getText(), UINT_MAX);
                declaredNumberOfValuesPerDimension.emplace_back(UINT_MAX);
            }
        }
    }

    unsigned int variableBitwidth = defaultVariableBitwidth;
    if (context->signalWidthToken != nullptr) {
        bool                              didIntegerConstantDeserializationFailToDueOverflow = false;
        const std::optional<unsigned int> parsedIntegerConstantFromAntlrToken                = deserializeConstantFromString(context->signalWidthToken->getText(), &didIntegerConstantDeserializationFailToDueOverflow);
        if (didIntegerConstantDeserializationFailToDueOverflow) {
            recordSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(mapTokenPositionToMessagePosition(*context->signalWidthToken), context->signalWidthToken->getText(), UINT_MAX);
        }

        if (parsedIntegerConstantFromAntlrToken.has_value()) {
            variableBitwidth = *parsedIntegerConstantFromAntlrToken;
        }
    }

    // If the defined variable bitwidth is larger than the maximum supported one, we report the error at the user defined bitwidth (if such a value was defined)
    // otherwise, if the value was taken from the configuration of the parser, the error is reported at the signal identifier. If none of the two tokens, the declared bitwidth or variable identifier,
    // are available (a special case that should occur where rarely) which should only be the case when an invalid signal declaration was defined by the user that failed to defined either of the two tokens, no error is reported.
    if (variableBitwidth > MAX_SUPPORTED_SIGNAL_BITWIDTH) {
        if (context->signalWidthToken != nullptr) {
            recordSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(mapTokenPositionToMessagePosition(*context->signalWidthToken), variableBitwidth, MAX_SUPPORTED_SIGNAL_BITWIDTH);
        } else if (context->literalIdent() != nullptr) {
            recordSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), variableBitwidth, MAX_SUPPORTED_SIGNAL_BITWIDTH);
        }
    } else if (variableBitwidth == 0) {
        if (context->signalWidthToken != nullptr) {
            recordSemanticError<SemanticError::VariableBitwidthEqualToZero>(mapTokenPositionToMessagePosition(*context->signalWidthToken));
        } else {
            recordSemanticError<SemanticError::VariableBitwidthEqualToZero>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()));
        }
    }

    // Add an implicit declaration if the user does not define the optional number of dimensions for a variable
    if (context->dimensionTokens.empty()) {
        declaredNumberOfValuesPerDimension.emplace_back(1);
    }

    if (variableIdentifier.has_value() && !declaredNumberOfValuesPerDimension.empty()) {
        return std::make_shared<syrec::Variable>(syrec::Variable::Type::In, *variableIdentifier, declaredNumberOfValuesPerDimension, variableBitwidth);
    }
    return std::nullopt;
}

std::optional<std::vector<syrec::Statement::ptr>> CustomModuleVisitor::visitStatementListTyped(const TSyrecParser::StatementListContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    syrec::Statement::vec processedStatementList;
    processedStatementList.reserve(context->stmts.size());

    for (const auto& antlrContextForStmt: context->stmts) {
        if (const std::optional<syrec::Statement::ptr> generatedStmtInstance = statementVisitorInstance->visitStatementTyped(antlrContextForStmt); generatedStmtInstance.has_value()) {
            processedStatementList.emplace_back(*generatedStmtInstance);
        }
    }
    return processedStatementList;
}

bool CustomModuleVisitor::doVariablesMatch(const syrec::Variable& lVariable, const syrec::Variable& rVariable) {
    return lVariable.name == rVariable.name && std::ranges::equal(lVariable.dimensions, rVariable.dimensions) && lVariable.bitwidth == rVariable.bitwidth;
}

bool CustomModuleVisitor::doVariableCollectionsMatch(const syrec::Variable::vec& lVariableCollection, const syrec::Variable::vec& rVariableCollection) {
    return std::ranges::equal(
            lVariableCollection,
            rVariableCollection,
            [](const syrec::Variable::ptr& lVariable, const syrec::Variable::ptr& rVariable) {
                return lVariable && rVariable && doVariablesMatch(*lVariable, *rVariable);
            });
}
