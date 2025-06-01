/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/components/custom_statement_visitor.hpp"

#include "TSyrecParser.h"
#include "Token.h"
#include "core/syrec/number.hpp"
#include "core/syrec/parser/components/custom_expression_visitor.hpp"
#include "core/syrec/parser/utils/custom_error_messages.hpp"
#include "core/syrec/parser/utils/if_statement_expression_components_recorder.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace syrec_parser;

std::optional<syrec::Statement::vec> CustomStatementVisitor::visitStatementListTyped(const TSyrecParser::StatementListContext* context) {
    if (context == nullptr) {
        return std::nullopt;
    }

    syrec::Statement::vec statements;
    statements.reserve(context->stmts.size());

    for (const auto& antlrStatementContext: context->stmts) {
        if (const std::optional<syrec::Statement::ptr> generatedStatement = visitStatementTyped(antlrStatementContext); generatedStatement.has_value() && generatedStatement.value()) {
            statements.emplace_back(*generatedStatement);
        }
    }
    return statements;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitStatementTyped(const TSyrecParser::StatementContext* context) {
    std::optional<syrec::Statement::ptr> generatedStatement;
    if (context->callStatement() != nullptr) {
        generatedStatement = visitCallStatementTyped(context->callStatement());
    } else if (context->forStatement() != nullptr) {
        generatedStatement = visitForStatementTyped(context->forStatement());
    } else if (context->ifStatement() != nullptr) {
        generatedStatement = visitIfStatementTyped(context->ifStatement());
    } else if (context->unaryStatement() != nullptr) {
        generatedStatement = visitUnaryStatementTyped(context->unaryStatement());
    } else if (context->assignStatement() != nullptr) {
        generatedStatement = visitAssignStatementTyped(context->assignStatement());
    } else if (context->swapStatement() != nullptr) {
        generatedStatement = visitSwapStatementTyped(context->swapStatement());
    } else if (context->skipStatement() != nullptr) {
        generatedStatement = visitSkipStatementTyped(context->skipStatement());
    } else {
        // We should not have to report an error at this position since the tokenizer should already report an error if the currently processed token is
        // not in the union of the FIRST sets of the potential alternatives.
        return std::nullopt;
    }

    if (generatedStatement.has_value() && *generatedStatement != nullptr && context != nullptr && context->getStart() != nullptr) {
        generatedStatement->get()->lineNumber = static_cast<unsigned>(context->getStart()->getLine());
    }
    return generatedStatement;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitAssignStatementTyped(const TSyrecParser::AssignStatementContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    std::optional<CustomExpressionVisitor::DeterminedExpressionOperandBitwidthInformation> expectedBitwidthOfAssignmentLhsOperand;
    const std::optional<syrec::VariableAccess::ptr>                                        assignmentLhsOperand = expressionVisitorInstance->visitSignalTyped(context->signal(), &expectedBitwidthOfAssignmentLhsOperand);
    if (assignmentLhsOperand.has_value()) {
        recordErrorIfAssignmentToReadonlyVariableIsPerformed(*assignmentLhsOperand->get()->var, *context->signal()->start);
        expressionVisitorInstance->setRestrictionOnVariableAccesses(*assignmentLhsOperand);
    }
    const std::optional<syrec::AssignStatement::AssignOperation> assignmentOperation = mapAntlrTokenToAssignmentOperation(*context);

    std::optional<CustomExpressionVisitor::DeterminedExpressionOperandBitwidthInformation> expectedBitwidthOfAssignmentRhsOperand;
    std::optional<syrec::Expression::ptr>                                                  assignmentRhsOperand = expressionVisitorInstance->visitExpressionTyped(context->expression(), expectedBitwidthOfAssignmentRhsOperand);

    bool detectedSemanticErrorAfterOperandsWhereProcessed = false;
    if (expectedBitwidthOfAssignmentLhsOperand.has_value() && assignmentRhsOperand.has_value()) {
        CustomExpressionVisitor::truncateConstantValuesInExpression(*assignmentRhsOperand, expectedBitwidthOfAssignmentLhsOperand->operandBitwidth, parserConfiguration.integerConstantTruncationOperation, &detectedSemanticErrorAfterOperandsWhereProcessed);
        if (detectedSemanticErrorAfterOperandsWhereProcessed) {
            recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->expression()->getStart()));
        } else if (expectedBitwidthOfAssignmentRhsOperand.has_value() && expectedBitwidthOfAssignmentLhsOperand->operandBitwidth != expectedBitwidthOfAssignmentRhsOperand->operandBitwidth) {
            Message::Position assignmentOperandBitwidthMismatchErrorPosition = mapTokenPositionToMessagePosition(*context->signal()->getStart());
            if (expectedBitwidthOfAssignmentRhsOperand.has_value() && expectedBitwidthOfAssignmentRhsOperand->positionOfOperandWithKnownBitwidth.has_value()) {
                assignmentOperandBitwidthMismatchErrorPosition = *expectedBitwidthOfAssignmentRhsOperand->positionOfOperandWithKnownBitwidth;
            } else if (expectedBitwidthOfAssignmentLhsOperand.has_value() && expectedBitwidthOfAssignmentLhsOperand->positionOfOperandWithKnownBitwidth.has_value()) {
                assignmentOperandBitwidthMismatchErrorPosition = *expectedBitwidthOfAssignmentLhsOperand->positionOfOperandWithKnownBitwidth;
            }
            recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(assignmentOperandBitwidthMismatchErrorPosition, expectedBitwidthOfAssignmentLhsOperand->operandBitwidth, expectedBitwidthOfAssignmentRhsOperand->operandBitwidth);
            detectedSemanticErrorAfterOperandsWhereProcessed = true;
        }
    }
    expressionVisitorInstance->clearRestrictionOnVariableAccesses();
    return !detectedSemanticErrorAfterOperandsWhereProcessed && assignmentLhsOperand.has_value() && assignmentOperation.has_value() && assignmentRhsOperand.has_value() ? std::optional(std::make_shared<syrec::AssignStatement>(*assignmentLhsOperand, *assignmentOperation, *assignmentRhsOperand)) : std::nullopt;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitUnaryStatementTyped(const TSyrecParser::UnaryStatementContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    const std::optional<syrec::VariableAccess::ptr> assignedToVariable = expressionVisitorInstance->visitSignalTyped(context->signal(), nullptr);
    if (assignedToVariable.has_value()) {
        recordErrorIfAssignmentToReadonlyVariableIsPerformed(*assignedToVariable->get()->var, *context->signal()->start);
    }

    expressionVisitorInstance->clearRestrictionOnVariableAccesses();

    const std::optional<syrec::UnaryStatement::UnaryOperation> assignmentOperation = mapAntlrTokenToUnaryAssignmentOperation(*context);
    return assignedToVariable.has_value() && assignmentOperation.has_value() ? std::make_optional(std::make_shared<syrec::UnaryStatement>(*assignmentOperation, *assignedToVariable)) : std::nullopt;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitSwapStatementTyped(const TSyrecParser::SwapStatementContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    std::optional<CustomExpressionVisitor::DeterminedExpressionOperandBitwidthInformation> expectedBitwidthOfAssignmentLhsOperand;
    const std::optional<syrec::VariableAccess::ptr>                                        swapLhsOperand = expressionVisitorInstance->visitSignalTyped(context->lhsOperand, &expectedBitwidthOfAssignmentLhsOperand);
    if (swapLhsOperand.has_value()) {
        recordErrorIfAssignmentToReadonlyVariableIsPerformed(*swapLhsOperand->get()->var, *context->lhsOperand->getStart());
        expressionVisitorInstance->setRestrictionOnVariableAccesses(*swapLhsOperand);
    }

    std::optional<CustomExpressionVisitor::DeterminedExpressionOperandBitwidthInformation> expectedBitwidthOfAssignmentRhsOperand;
    const std::optional<syrec::VariableAccess::ptr>                                        swapRhsOperand = expressionVisitorInstance->visitSignalTyped(context->rhsOperand, &expectedBitwidthOfAssignmentRhsOperand);
    if (swapRhsOperand.has_value()) {
        recordErrorIfAssignmentToReadonlyVariableIsPerformed(*swapRhsOperand->get()->var, *context->rhsOperand->getStart());
        if (!parserConfiguration.allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess) {
            // A semantic error generated during the processing of the lhs or rhs operand of the swap statement should not prevent the check for the usage of overlapping variable accesses in the dimension access of the operand
            // on the lhs. To prevent the duplicate generation of the already found semantic errors on the lhs, a filter for the semantic error of interest will be temporarily set. An identical check for the usage of the variable
            // of the lhs in any dimension access on the rhs is already performed during the processing of the latter.
            sharedGeneratedMessageContainerInstance->setFilterForToBeRecordedMessages(std::string(getIdentifierForSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>()));
            expressionVisitorInstance->setRestrictionOnVariableAccesses(*swapRhsOperand);
            // Future versions of the parser might implement a more efficient version of this check instead of simply reusing the visitor function as the same logic (i.e. duplicate allocations, etc.) is executed twice.
            expressionVisitorInstance->visitSignalTyped(context->lhsOperand, nullptr);
            sharedGeneratedMessageContainerInstance->clearFilterForToBeRecordedMessages();
        }
    }
    expressionVisitorInstance->clearRestrictionOnVariableAccesses();
    if (expectedBitwidthOfAssignmentLhsOperand.has_value() && expectedBitwidthOfAssignmentRhsOperand.has_value() && expectedBitwidthOfAssignmentLhsOperand->operandBitwidth != expectedBitwidthOfAssignmentRhsOperand->operandBitwidth) {
        // Since the right-hand side is processed after the left-hand one, the former will also serve as the position in which any potential operand mismatch error is reported.
        recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(mapTokenPositionToMessagePosition(*context->rhsOperand->literalIdent()->getSymbol()), expectedBitwidthOfAssignmentLhsOperand->operandBitwidth, expectedBitwidthOfAssignmentRhsOperand->operandBitwidth);
        return std::nullopt;
    }
    return swapLhsOperand.has_value() && swapRhsOperand.has_value() ? std::make_optional(std::make_shared<syrec::SwapStatement>(*swapLhsOperand, *swapRhsOperand)) : std::nullopt;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitSkipStatementTyped([[maybe_unused]] const TSyrecParser::SkipStatementContext* context) {
    return std::make_shared<syrec::SkipStatement>();
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitCallStatementTyped(const TSyrecParser::CallStatementContext* context) {
    if (context == nullptr) {
        return std::nullopt;
    }

    const std::optional<std::string>                        calledModuleIdentifier = context->moduleIdent != nullptr ? std::optional(context->moduleIdent->getText()) : std::nullopt;
    const std::optional<utils::TemporaryVariableScope::ptr> activeSymbolTableScope = symbolTable->getActiveTemporaryScope();
    // Should rename callee arguments to caller arguments in grammar
    syrec::Variable::vec symbolTableEntryPerCallerArgument;
    symbolTableEntryPerCallerArgument.reserve(context->callerArguments.size());

    std::vector<std::string> callerArgumentVariableIdentifiers;
    callerArgumentVariableIdentifiers.reserve(context->callerArguments.size());

    for (const auto& antlrCallerArgumentToken: context->callerArguments) {
        if (antlrCallerArgumentToken == nullptr || !activeSymbolTableScope.has_value()) {
            continue;
        }

        // In case that a user defines a call/uncall statement with a loop variable as a caller argument, the token text will not include the loop variable prefix and thus the identifier might match
        // with the one of an existing variable. Since we cannot recover the 'dropped' loop variable prefix symbol, the reported semantic errors could differ between the now described cases with
        // the loop variable identifier not matching an variable causes more semantic errors. Due to the generation of a syntax error, the false positive of the loop variable matching an existing variable
        // will not cause the SyReC program to be detected as well formed.
        callerArgumentVariableIdentifiers.emplace_back(antlrCallerArgumentToken->getText());
        if (const std::optional<utils::TemporaryVariableScope::ScopeEntry::readOnlyPtr> matchingSymbolTableEntryForCallerArgument = activeSymbolTableScope->get()->getVariableByName(antlrCallerArgumentToken->getText()); matchingSymbolTableEntryForCallerArgument.has_value()) {
            if (matchingSymbolTableEntryForCallerArgument->get()->getVariableData().has_value()) {
                symbolTableEntryPerCallerArgument.emplace_back(std::make_shared<syrec::Variable>(**matchingSymbolTableEntryForCallerArgument->get()->getVariableData()));
            }
        } else {
            recordSemanticError<SemanticError::NoVariableMatchingIdentifier>(mapTokenPositionToMessagePosition(*antlrCallerArgumentToken), antlrCallerArgumentToken->getText());
        }
    }

    if (!calledModuleIdentifier.has_value()) {
        return std::nullopt;
    }

    NotOverloadResolutedCallStatementScope*                                             activeModuleScopeRecordingCallStatements = getActiveModuleScopeRecordingCallStatements();
    std::optional<NotOverloadResolutedCallStatementScope::CallStatementInstanceVariant> callStatementInstanceVariant;
    if (context->literalOpCall() != nullptr) {
        callStatementInstanceVariant = std::make_shared<syrec::CallStatement>(nullptr, callerArgumentVariableIdentifiers);
    } else if (context->literalOpUncall() != nullptr) {
        callStatementInstanceVariant = std::make_shared<syrec::UncallStatement>(nullptr, callerArgumentVariableIdentifiers);
    }

    if (callStatementInstanceVariant.has_value()) {
        if (activeModuleScopeRecordingCallStatements != nullptr) {
            activeModuleScopeRecordingCallStatements->callStatementsToPerformOverloadResolutionOn.emplace_back(*callStatementInstanceVariant, *calledModuleIdentifier, symbolTableEntryPerCallerArgument, context->moduleIdent->getLine(), context->moduleIdent->getCharPositionInLine());
        } else {
            recordCustomError(Message::Position(context->moduleIdent->getLine(), context->moduleIdent->getCharPositionInLine()), "Cannot record call statement variant due to no scope to record such statements is open! This is an internal error that should not happen");
        }

        // While we do check for the reversibility of assignments/swaps by not allowing the usage of overlapping variable access parts between the two sides of such a statement, variable aliases
        // that are generated by a call/uncall are not considered for these checks and will not be detected by the parser (the SyReC specification does not forbid the usage of a variable as a caller argument multiple times in a module call/uncall).
        // A simple SyReC program showcasing this is the following:
        //      module x(inout a(4), out b(4)) a <=> b module main() wire t(4) call x(t, t)
        // The use of the local variable 't' as an argument twice in the module call, the overlapping access in the operands of the swap statement will not detected by the parser
        // since the formal parameters are used for the overlap check instead. Future versions of the parser might perform the overlap checks using the variable aliases.
        // For now we delegate the responsibility for such overlap checks for call/uncall to subsequent components that will process the IR generated by the parser.
        if (context->literalOpCall() != nullptr) {
            return std::get<std::shared_ptr<syrec::CallStatement>>(*callStatementInstanceVariant);
        }
        if (context->literalOpUncall() != nullptr) {
            return std::get<std::shared_ptr<syrec::UncallStatement>>(*callStatementInstanceVariant);
        }
    }
    return std::nullopt;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitIfStatementTyped(const TSyrecParser::IfStatementContext* context) {
    if (context == nullptr) {
        return std::nullopt;
    }
    auto generatedIfStatement = std::make_shared<syrec::IfStatement>();

    // A note regarding the reporting of semantic errors, if the guard condition evaluates to a constant value at compile time, semantic errors in the statements of the not taken branch will still be reported
    // (we will follow the behaviour found in other compilers [see https://godbolt.org/z/nM419obo4]).
    const auto& backupOfCurrentIfStatementExpressionComponentsComparer = expressionVisitorInstance->getIfStatementExpressionComponentsRecorder();
    expressionVisitorInstance->clearIfStatementExpressionComponentsRecorder();

    auto ifStatementExpressionComponentsComparer = std::make_shared<utils::IfStatementExpressionComponentsRecorder>();
    expressionVisitorInstance->setIfStatementExpressionComponentsRecorder(ifStatementExpressionComponentsComparer);

    bool detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed = false;
    // The operands in the guard/closing-guard condition expression must be equal to one since the value of said condition must evaluate to a boolean value.
    std::optional<CustomExpressionVisitor::DeterminedExpressionOperandBitwidthInformation> determinedOperandBitwidthOfGuardConditionExpression;
    generatedIfStatement->setCondition(expressionVisitorInstance->visitExpressionTyped(context->guardCondition, determinedOperandBitwidthOfGuardConditionExpression).value_or(nullptr));

    if (generatedIfStatement->condition != nullptr) {
        CustomExpressionVisitor::truncateConstantValuesInExpression(generatedIfStatement->condition, 1, parserConfiguration.integerConstantTruncationOperation, &detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed);
        if (detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed) {
            recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->guardCondition->getStart()));
        } else if (determinedOperandBitwidthOfGuardConditionExpression.has_value() && determinedOperandBitwidthOfGuardConditionExpression->operandBitwidth != 1) {
            recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(mapTokenPositionToMessagePosition(*context->guardCondition->getStart()), 1, determinedOperandBitwidthOfGuardConditionExpression->operandBitwidth);
            detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed = true;
        }
    }

    // Similarly to the handling of the statement list production in the ForStatement, an empty statement list should already report a syntax error and thus an explicit handling
    // of an empty statement at this point is not necessary. Additionally, currently the parser does not implement the dead code elimination optimization technique which would allow
    // the parser to remove the statements of the not executed branch, if one can determine the value of the guard condition at compile time, thus semantic errors are reported regardless
    // of the value of the guard condition.
    generatedIfStatement->thenStatements = visitStatementListTyped(context->trueBranchStmts).value_or(syrec::Statement::vec());
    generatedIfStatement->elseStatements = visitStatementListTyped(context->falseBranchStmts).value_or(syrec::Statement::vec());

    ifStatementExpressionComponentsComparer->switchMode(utils::IfStatementExpressionComponentsRecorder::OperationMode::Comparing);
    std::optional<CustomExpressionVisitor::DeterminedExpressionOperandBitwidthInformation> determinedOperandBitwidthOfClosingGuardConditionExpression;
    generatedIfStatement->setFiCondition(expressionVisitorInstance->visitExpressionTyped(context->matchingGuardExpression, determinedOperandBitwidthOfClosingGuardConditionExpression).value_or(nullptr));

    if (generatedIfStatement->fiCondition != nullptr) {
        bool detectedDivisionByZeroDuringTruncationOfConstantValues = false;
        CustomExpressionVisitor::truncateConstantValuesInExpression(generatedIfStatement->fiCondition, 1, parserConfiguration.integerConstantTruncationOperation, &detectedDivisionByZeroDuringTruncationOfConstantValues);
        if (detectedDivisionByZeroDuringTruncationOfConstantValues) {
            recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->matchingGuardExpression->getStart()));
            detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed = true;
        } else if (determinedOperandBitwidthOfClosingGuardConditionExpression.has_value() && determinedOperandBitwidthOfClosingGuardConditionExpression->operandBitwidth != 1) {
            recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(mapTokenPositionToMessagePosition(*context->matchingGuardExpression->getStart()), 1, determinedOperandBitwidthOfClosingGuardConditionExpression->operandBitwidth);
            detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed = true;
        }
    }

    if (generatedIfStatement->condition != nullptr && generatedIfStatement->fiCondition != nullptr && !ifStatementExpressionComponentsComparer->recordedMatchingExpressionComponents().value_or(true)) {
        recordSemanticError<SemanticError::IfGuardExpressionMismatch>(mapTokenPositionToMessagePosition(*context->guardCondition->getStart()));
        detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed = true;
    }

    if (backupOfCurrentIfStatementExpressionComponentsComparer.has_value()) {
        expressionVisitorInstance->setIfStatementExpressionComponentsRecorder(*backupOfCurrentIfStatementExpressionComponentsComparer);
    } else {
        expressionVisitorInstance->clearIfStatementExpressionComponentsRecorder();
    }
    return !detectedSemanticErrorAfterOperandsOfGuardAndClosingGuardConditionWhereProcessed ? std::make_optional(generatedIfStatement) : std::nullopt;
}

std::optional<syrec::Statement::ptr> CustomStatementVisitor::visitForStatementTyped(const TSyrecParser::ForStatementContext* context) {
    if (context == nullptr) {
        return std::nullopt;
    }

    const std::optional<utils::TemporaryVariableScope::ptr> activeSymbolTableScope = symbolTable->getActiveTemporaryScope();
    const std::optional<std::string>                        loopVariableIdentifier = visitLoopVariableDefinitionTyped(context->loopVariableDefinition());
    if (loopVariableIdentifier.has_value()) {
        if (activeSymbolTableScope.has_value()) {
            activeSymbolTableScope->get()->recordLoopVariable(std::make_shared<syrec::Number>(*loopVariableIdentifier));
        }
        expressionVisitorInstance->setRestrictionOnLoopVariablesUsableInFutureLoopVariableValueInitializations(*loopVariableIdentifier);
    }

    const std::optional<syrec::Number::ptr> iterationRangeStartValue   = expressionVisitorInstance->visitNumberTyped(context->startValue);
    std::optional<unsigned int>             valueOfIterationRangeStart = iterationRangeStartValue.has_value() && *iterationRangeStartValue != nullptr ? tryGetConstantValueOf(**iterationRangeStartValue) : std::nullopt;
    expressionVisitorInstance->clearRestrictionOnLoopVariablesUsableInFutureLoopVariableValueInitializations();

    if (loopVariableIdentifier.has_value() && activeSymbolTableScope.has_value()) {
        activeSymbolTableScope->get()->updateValueOfLoopVariable(*loopVariableIdentifier, valueOfIterationRangeStart);
    }

    const std::optional<syrec::Number::ptr> iterationRangeEndValue   = expressionVisitorInstance->visitNumberTyped(context->endValue);
    const std::optional<unsigned int>       valueOfIterationRangeEnd = iterationRangeEndValue.has_value() && *iterationRangeEndValue != nullptr ? tryGetConstantValueOf(**iterationRangeEndValue) : std::nullopt;

    const syrec::Number::ptr iterationRangeStepSizeValue = visitLoopStepsizeDefinitionTyped(context->loopStepsizeDefinition()).value_or(std::make_shared<syrec::Number>(1));
    auto                     generatedForStatement       = std::make_shared<syrec::ForStatement>();
    generatedForStatement->loopVariable                  = loopVariableIdentifier.value_or("");

    if (iterationRangeStartValue.has_value()) {
        generatedForStatement->range = context->endValue != nullptr ? std::make_pair(*iterationRangeStartValue, iterationRangeEndValue.value_or(nullptr)) : std::make_pair(*iterationRangeStartValue, *iterationRangeStartValue);
    } else if (iterationRangeEndValue.has_value()) {
        generatedForStatement->range = std::make_pair(std::make_shared<syrec::Number>(0), *iterationRangeEndValue);
        valueOfIterationRangeStart   = 0;
    }

    generatedForStatement->step                                                                   = iterationRangeStepSizeValue;
    const std::optional<unsigned int> valueOfIterationRangeStepSize                               = iterationRangeStepSizeValue != nullptr ? tryGetConstantValueOf(*iterationRangeStepSizeValue) : std::nullopt;
    bool                              shouldValueOfLoopVariableBeResetPriorToProcessingOfLoopBody = true;
    if (valueOfIterationRangeStepSize.has_value() && valueOfIterationRangeStart.has_value() && valueOfIterationRangeEnd.has_value()) {
        if (*valueOfIterationRangeStepSize == 0) {
            recordSemanticError<SemanticError::InfiniteLoopDetected>(
                    mapTokenPositionToMessagePosition(*context->literalKeywordFor()->getSymbol()),
                    *valueOfIterationRangeStart, *valueOfIterationRangeEnd, *valueOfIterationRangeStepSize);
        }

        // The declared initial value of a loop variable should only be propagated to the statements in the body of the loop if the number of iterations performed by the loop is equal to one.
        // It is the responsibility of the user to keep the used overflow semantics for unsigned integers, causing a wrap-around at the borders of the value range, in mind when defining the iteration range of a loop.
        // The iteration range is equivalent to the python range(<START>, <END>, <STEP>) function, meaning that we assume that the <END> value is not included in the iteration range (i.e. equal to for $i = START; $i < END; $i += STEP)
        if (*valueOfIterationRangeStart < *valueOfIterationRangeEnd) {
            shouldValueOfLoopVariableBeResetPriorToProcessingOfLoopBody = *valueOfIterationRangeStart + *valueOfIterationRangeStepSize <= *valueOfIterationRangeEnd;
        } else if (*valueOfIterationRangeStart > *valueOfIterationRangeEnd) {
            shouldValueOfLoopVariableBeResetPriorToProcessingOfLoopBody = *valueOfIterationRangeStart - *valueOfIterationRangeStepSize >= *valueOfIterationRangeEnd;
        }
    }

    if (activeSymbolTableScope.has_value() && loopVariableIdentifier.has_value() && shouldValueOfLoopVariableBeResetPriorToProcessingOfLoopBody) {
        activeSymbolTableScope->get()->updateValueOfLoopVariable(*loopVariableIdentifier, std::nullopt);
    }

    // An empty loop body statement list definition results in a syntax error and thus does not need to be checked again here. Additionally, due to the
    // parser not implementing the dead code elimination optimization technique, semantic errors will also be created for the statements of the body of a loop
    // that does not perform any iterations.
    generatedForStatement->statements = visitStatementListTyped(context->statementList()).value_or(syrec::Statement::vec());

    if (loopVariableIdentifier.has_value() && activeSymbolTableScope.has_value()) {
        activeSymbolTableScope->get()->removeVariable(*loopVariableIdentifier);
    }
    return generatedForStatement->step != nullptr && generatedForStatement->range.first != nullptr && generatedForStatement->range.second != nullptr ? std::make_optional(generatedForStatement) : std::nullopt;
}

std::vector<CustomStatementVisitor::NotOverloadResolutedCallStatementScope> CustomStatementVisitor::getCallStatementsWithNotPerformedOverloadResolution() const {
    return callStatementsWithNotPerformedOverloadResolutionScopes;
}

void CustomStatementVisitor::openNewScopeToRecordCallStatementsInModule(const NotOverloadResolutedCallStatementScope::DeclaredModuleSignature& enclosingModuleSignature) {
    callStatementsWithNotPerformedOverloadResolutionScopes.emplace_back(enclosingModuleSignature);
}

// START OF NON-PUBLIC FUNCTIONALITY
std::optional<std::string> CustomStatementVisitor::visitLoopVariableDefinitionTyped(const TSyrecParser::LoopVariableDefinitionContext* context) const {
    if (context == nullptr || context->literalIdent() == nullptr) {
        return std::nullopt;
    }

    std::string loopVariableIdentifier = "$" + context->literalIdent()->getText();
    if (const std::optional<utils::TemporaryVariableScope::ptr> activeSymbolTableScope = symbolTable->getActiveTemporaryScope(); activeSymbolTableScope.has_value() && activeSymbolTableScope->get()->getVariableByName(loopVariableIdentifier)) {
        recordSemanticError<SemanticError::DuplicateVariableDeclaration>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), loopVariableIdentifier);
    }
    return loopVariableIdentifier;
}

std::optional<syrec::Number::ptr> CustomStatementVisitor::visitLoopStepsizeDefinitionTyped(const TSyrecParser::LoopStepsizeDefinitionContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    std::optional<syrec::Number::ptr> userDefinedStepsizeValue = expressionVisitorInstance->visitNumberTyped(context->number());
    if (!userDefinedStepsizeValue.has_value() || !userDefinedStepsizeValue.value()) {
        return std::nullopt;
    }

    if (context->literalOpMinus() != nullptr) {
        if (const std::optional<unsigned int> evaluatedValueForStepsize = userDefinedStepsizeValue.value()->tryEvaluate({}); evaluatedValueForStepsize.has_value()) {
            return std::make_shared<syrec::Number>(-*evaluatedValueForStepsize);
        }

        // Since we cannot store an 'expression' of the form -(<Number>) in the IR representation, a constant expression (0 - <Number>) is used instead.
        return std::make_shared<syrec::Number>(syrec::Number::ConstantExpression(
                std::make_shared<syrec::Number>(0),
                syrec::Number::ConstantExpression::Operation::Subtraction,
                *userDefinedStepsizeValue));
    }
    return userDefinedStepsizeValue;
}

void CustomStatementVisitor::recordErrorIfAssignmentToReadonlyVariableIsPerformed(const syrec::Variable& accessedVariable, const antlr4::Token& reportedErrorPosition) const {
    if (!doesVariableTypeAllowAssignment(accessedVariable.type)) {
        recordSemanticError<SemanticError::AssignmentToReadonlyVariable>(mapTokenPositionToMessagePosition(reportedErrorPosition), accessedVariable.name);
    }
}

CustomStatementVisitor::NotOverloadResolutedCallStatementScope* CustomStatementVisitor::getActiveModuleScopeRecordingCallStatements() {
    if (callStatementsWithNotPerformedOverloadResolutionScopes.empty()) {
        return nullptr;
    }
    return &callStatementsWithNotPerformedOverloadResolutionScopes.back();
}

std::optional<syrec::AssignStatement::AssignOperation> CustomStatementVisitor::mapAntlrTokenToAssignmentOperation(const TSyrecParser::AssignStatementContext& assignmentStatementContext) {
    if (assignmentStatementContext.literalOpAddAssign() != nullptr) {
        return syrec::AssignStatement::AssignOperation::Add;
    }
    if (assignmentStatementContext.literalOpSubAssign() != nullptr) {
        return syrec::AssignStatement::AssignOperation::Subtract;
    }
    if (assignmentStatementContext.literalOpXorAssign() != nullptr) {
        return syrec::AssignStatement::AssignOperation::Exor;
    }
    return std::nullopt;
}

std::optional<syrec::UnaryStatement::UnaryOperation> CustomStatementVisitor::mapAntlrTokenToUnaryAssignmentOperation(const TSyrecParser::UnaryStatementContext& unaryStatementContext) {
    if (unaryStatementContext.literalOpIncrementAssign() != nullptr) {
        return syrec::UnaryStatement::UnaryOperation::Increment;
    }
    if (unaryStatementContext.literalOpDecrementAssign() != nullptr) {
        return syrec::UnaryStatement::UnaryOperation::Decrement;
    }
    if (unaryStatementContext.literalOpInvertAssign() != nullptr) {
        return syrec::UnaryStatement::UnaryOperation::Invert;
    }
    return std::nullopt;
}
