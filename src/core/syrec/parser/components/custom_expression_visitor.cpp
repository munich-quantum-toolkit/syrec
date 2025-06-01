/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/components/custom_expression_visitor.hpp"

#include "TSyrecParser.h"
#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/parser/utils/custom_error_messages.hpp"
#include "core/syrec/parser/utils/if_statement_expression_components_recorder.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
#include "core/syrec/parser/utils/variable_access_index_check.hpp"
#include "core/syrec/parser/utils/variable_overlap_check.hpp"
#include "core/syrec/variable.hpp"
#include "tree/ParseTreeType.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace syrec_parser;

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::visitExpressionTyped(const TSyrecParser::ExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth) {
    if (context == nullptr) {
        return std::nullopt;
    }

    if (const auto* const binaryExpressionContext = dynamic_cast<const TSyrecParser::ExpressionFromBinaryExpressionContext*>(context); binaryExpressionContext != nullptr) {
        return visitBinaryExpressionTyped(binaryExpressionContext->binaryExpression(), optionalDeterminedOperandBitwidth);
    }
    if (const auto* const shiftExpressionContext = dynamic_cast<const TSyrecParser::ExpressionFromShiftExpressionContext*>(context); shiftExpressionContext != nullptr) {
        return visitShiftExpressionTyped(shiftExpressionContext->shiftExpression(), optionalDeterminedOperandBitwidth);
    }
    if (const auto* const unaryExpressionContext = dynamic_cast<const TSyrecParser::ExpressionFromUnaryExpressionContext*>(context); unaryExpressionContext != nullptr) {
        return visitUnaryExpressionTyped(unaryExpressionContext->unaryExpression(), optionalDeterminedOperandBitwidth);
    }
    if (const auto* const expressionFromNumberContext = dynamic_cast<const TSyrecParser::ExpressionFromNumberContext*>(context); expressionFromNumberContext != nullptr) {
        return visitExpressionFromNumberTyped(expressionFromNumberContext);
    }
    if (const auto* const expressionFromSignalContext = dynamic_cast<const TSyrecParser::ExpressionFromSignalContext*>(context); expressionFromSignalContext != nullptr) {
        return visitExpressionFromSignalTyped(expressionFromSignalContext, optionalDeterminedOperandBitwidth);
    }

    // We should not have to report an error at this position since the tokenizer should already report an error if the currently processed token is
    // not in the union of the FIRST sets of the potential alternatives.
    //recordCustomError(Message::Position(0, 0), "Unhandled expression context variant. This should not happen");
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::visitBinaryExpressionTyped(const TSyrecParser::BinaryExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth) {
    if (context == nullptr) {
        return std::nullopt;
    }
    recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::ExpressionBracketKind::Opening);

    std::optional<DeterminedExpressionOperandBitwidthInformation> determinedLhsOperandBitwidth;
    std::optional<syrec::Expression::ptr>                         lhsOperand              = visitExpressionTyped(context->lhsOperand, determinedLhsOperandBitwidth);
    const std::optional<syrec::BinaryExpression::BinaryOperation> mappedToBinaryOperation = context->binaryOperation != nullptr ? mapTokenToBinaryOperation(*context) : std::nullopt;
    if (context->binaryOperation != nullptr && !mappedToBinaryOperation.has_value()) {
        recordSemanticError<SemanticError::UnhandledOperationFromGrammarInParser>(mapTokenPositionToMessagePosition(*context->binaryOperation), context->binaryOperation->getText());
    }

    if (mappedToBinaryOperation.has_value()) {
        recordExpressionComponent(*mappedToBinaryOperation);
    }

    std::optional<DeterminedExpressionOperandBitwidthInformation> determinedRhsOperandBitwidth;
    std::optional<syrec::Expression::ptr>                         rhsOperand = visitExpressionTyped(context->rhsOperand, determinedRhsOperandBitwidth);
    recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::ExpressionBracketKind::Closing);
    if (!mappedToBinaryOperation.has_value()) {
        // We cannot propagate the operand bitwidth of the left or right-hand side operand to the parent expression if we do not know which binary operation
        // was defined in the current expression since logical and relational operations will aggregate the result in a single bit while all other operations
        // propagate the bitwidth of its operands
        return std::nullopt;
    }

    const std::optional<unsigned int> constantValueOfRhsOperand = rhsOperand.has_value() && *rhsOperand != nullptr ? tryGetConstantValueOf(**rhsOperand) : std::nullopt;
    if ((*mappedToBinaryOperation == syrec::BinaryExpression::BinaryOperation::Divide || *mappedToBinaryOperation == syrec::BinaryExpression::BinaryOperation::Modulo || *mappedToBinaryOperation == syrec::BinaryExpression::BinaryOperation::FracDivide) && constantValueOfRhsOperand.has_value() && *constantValueOfRhsOperand == 0) {
        recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*(context->lhsOperand != nullptr ? context->lhsOperand->getStart() : context->rhsOperand->getStart())));
        return std::nullopt;
    }

    optionalDeterminedOperandBitwidth = determinedLhsOperandBitwidth.has_value() ? determinedLhsOperandBitwidth : determinedRhsOperandBitwidth;
    if (*mappedToBinaryOperation == syrec::BinaryExpression::BinaryOperation::LogicalOr || *mappedToBinaryOperation == syrec::BinaryExpression::BinaryOperation::LogicalAnd) {
        if (!optionalDeterminedOperandBitwidth.has_value()) {
            optionalDeterminedOperandBitwidth = DeterminedExpressionOperandBitwidthInformation();
        }
        optionalDeterminedOperandBitwidth->operandBitwidth = 1;
    }

    if (optionalDeterminedOperandBitwidth.has_value()) {
        if (!determinedLhsOperandBitwidth.has_value() && lhsOperand.has_value()) {
            // Truncate integer constants until first logical or relational operation is encountered
            bool detectedDivisionByZeroDuringTruncationOfIntegerConstants = false;
            truncateConstantValuesInExpression(*lhsOperand, optionalDeterminedOperandBitwidth->operandBitwidth, parserConfiguration.integerConstantTruncationOperation, &detectedDivisionByZeroDuringTruncationOfIntegerConstants);
            if (detectedDivisionByZeroDuringTruncationOfIntegerConstants) {
                recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->lhsOperand->getStart()));
                return std::nullopt;
            }
        } else if (!determinedRhsOperandBitwidth.has_value() && rhsOperand.has_value()) {
            // Truncate integer constants until first logical or relational operation is encountered
            bool detectedDivisionByZeroDuringTruncationOfIntegerConstants = false;
            truncateConstantValuesInExpression(*rhsOperand, optionalDeterminedOperandBitwidth->operandBitwidth, parserConfiguration.integerConstantTruncationOperation, &detectedDivisionByZeroDuringTruncationOfIntegerConstants);
            if (detectedDivisionByZeroDuringTruncationOfIntegerConstants) {
                recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->rhsOperand->getStart()));
                return std::nullopt;
            }
        }

        // We start by comparing the bitwidth of the operands of the expression with the expected bitwidth that was potentially set (either by one of the operands) or based on the
        // operation used in the current expression. In the latter case, the comparison between the operand bitwidths can be omitted since they are already compared to the expected one.
        // Additionally, if the expected bitwidth was set to the bitwidth of the other operand of the expression then the comparison will already be performed in the following two checks.
        bool shouldOperandBitwidthsBeCompared = true;
        if (determinedLhsOperandBitwidth.has_value() && determinedLhsOperandBitwidth->operandBitwidth != optionalDeterminedOperandBitwidth->operandBitwidth) {
            const Message::Position operandBitwidthLengthMismatchErrorPosition = determinedLhsOperandBitwidth->positionOfOperandWithKnownBitwidth.has_value() ? determinedLhsOperandBitwidth->positionOfOperandWithKnownBitwidth.value() : mapTokenPositionToMessagePosition(*context->lhsOperand->getStart());

            recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(operandBitwidthLengthMismatchErrorPosition, optionalDeterminedOperandBitwidth->operandBitwidth, determinedLhsOperandBitwidth->operandBitwidth);
            shouldOperandBitwidthsBeCompared = false;
        }
        if (determinedRhsOperandBitwidth.has_value() && determinedRhsOperandBitwidth->operandBitwidth != optionalDeterminedOperandBitwidth->operandBitwidth) {
            const Message::Position operandBitwidthLengthMismatchErrorPosition = determinedRhsOperandBitwidth->positionOfOperandWithKnownBitwidth.has_value() ? determinedRhsOperandBitwidth->positionOfOperandWithKnownBitwidth.value() : mapTokenPositionToMessagePosition(*context->rhsOperand->getStart());

            recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(operandBitwidthLengthMismatchErrorPosition, optionalDeterminedOperandBitwidth->operandBitwidth, determinedRhsOperandBitwidth->operandBitwidth);
            shouldOperandBitwidthsBeCompared = false;
        }

        if (shouldOperandBitwidthsBeCompared && determinedLhsOperandBitwidth.has_value() && determinedRhsOperandBitwidth.has_value()) {
            if (determinedLhsOperandBitwidth->operandBitwidth != determinedRhsOperandBitwidth->operandBitwidth) {
                // The position of operand that set the expected bitwidth is unknown only if said operand is itself an expression with an unknown bitwidth.
                // In all other cases, either:
                // I.   The bitwidth of the operands of the current expression matches (branch should not be entered then)
                // II.  The bitwidth of the operands did not match. In this case the position of one operand must be known (otherwise the bitwidth of both operands would be unknown
                //      or would match [if both operands are nested expressions whose bitwidth was set due to the usage of a logical or relational operand]).
                // The position of the right-hand side operand has precedence over the left-hand one since the former is processed after the latter.
                const Message::Position operandBitwidthLengthMismatchErrorPosition = determinedRhsOperandBitwidth->positionOfOperandWithKnownBitwidth.has_value() ? determinedRhsOperandBitwidth->positionOfOperandWithKnownBitwidth.value() : determinedLhsOperandBitwidth->positionOfOperandWithKnownBitwidth.value();
                recordSemanticError<SemanticError::ExpressionBitwidthMismatches>(operandBitwidthLengthMismatchErrorPosition, determinedLhsOperandBitwidth->operandBitwidth, determinedRhsOperandBitwidth->operandBitwidth);
                return std::nullopt;
            }
            optionalDeterminedOperandBitwidth->positionOfOperandWithKnownBitwidth = determinedLhsOperandBitwidth->positionOfOperandWithKnownBitwidth;
        }
    }

    const std::optional<unsigned int> expectedBitwidthOfOperandsInExpression = optionalDeterminedOperandBitwidth.has_value() ? std::make_optional(optionalDeterminedOperandBitwidth->operandBitwidth) : std::nullopt;
    if (isBinaryOperationARelationalOrLogicalOne(*mappedToBinaryOperation)) {
        if (!optionalDeterminedOperandBitwidth.has_value()) {
            optionalDeterminedOperandBitwidth = DeterminedExpressionOperandBitwidthInformation();
        }
        optionalDeterminedOperandBitwidth->operandBitwidth = 1;
    }

    // Instead of propagating the position of the operand that led to the expression having the currently set operand bitwidth, we instead propagate the start position
    // of the current expression to aid the user in determining which expressions need to be modified when resolving operand bitwidth errors.
    if (optionalDeterminedOperandBitwidth.has_value()) {
        if (context->getStart() != nullptr) {
            optionalDeterminedOperandBitwidth->positionOfOperandWithKnownBitwidth = mapTokenPositionToMessagePosition(*context->getStart());
        } else {
            optionalDeterminedOperandBitwidth->positionOfOperandWithKnownBitwidth.reset();
        }
    }

    if (lhsOperand.has_value() && rhsOperand.has_value()) {
        // We delegate the truncation of constant values to the caller of this function since the expected bitwidth of the operands could have been set in the currently processed
        // expression and needs to be propagate to any parent expression
        if (const std::optional<syrec::Expression::ptr> simplifiedBinaryExpr = trySimplifyBinaryExpression(syrec::BinaryExpression(*lhsOperand, *mappedToBinaryOperation, *rhsOperand), expectedBitwidthOfOperandsInExpression, nullptr); simplifiedBinaryExpr.has_value()) {
            // If the binary expression evaluated to a numeric expression then the expected operand bitwidth needs to be reset since we cannot assume the expected bitwidth for such an expression.
            if (const auto& simplifiedBinaryExprAsNumericOne = optionalDeterminedOperandBitwidth.has_value() ? std::dynamic_pointer_cast<syrec::NumericExpression>(*simplifiedBinaryExpr) : nullptr; simplifiedBinaryExprAsNumericOne != nullptr) {
                optionalDeterminedOperandBitwidth.reset();
            }
            return simplifiedBinaryExpr;
        }
        return std::make_shared<syrec::BinaryExpression>(*lhsOperand, *mappedToBinaryOperation, *rhsOperand);
    }
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::visitShiftExpressionTyped(const TSyrecParser::ShiftExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth) {
    if (context == nullptr) {
        return std::nullopt;
    }

    recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::ExpressionBracketKind::Opening);
    std::optional<syrec::Expression::ptr>                       toBeShiftedOperand     = visitExpressionTyped(context->expression(), optionalDeterminedOperandBitwidth);
    const std::optional<syrec::ShiftExpression::ShiftOperation> mappedToShiftOperation = context->shiftOperation != nullptr ? mapTokenToShiftOperation(*context) : std::nullopt;
    if (context->shiftOperation != nullptr && !mappedToShiftOperation.has_value()) {
        recordSemanticError<SemanticError::UnhandledOperationFromGrammarInParser>(mapTokenPositionToMessagePosition(*context->shiftOperation), context->shiftOperation->getText());
    }

    if (mappedToShiftOperation.has_value()) {
        recordExpressionComponent(*mappedToShiftOperation);
    }

    const std::optional<syrec::Number::ptr> shiftAmount = visitNumberTyped(context->number());
    recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::ExpressionBracketKind::Closing);
    if (!mappedToShiftOperation.has_value()) {
        return std::nullopt;
    }

    if (context->getStart() != nullptr && optionalDeterminedOperandBitwidth.has_value()) {
        optionalDeterminedOperandBitwidth->positionOfOperandWithKnownBitwidth = mapTokenPositionToMessagePosition(*context->getStart());
    }

    if (toBeShiftedOperand.has_value() && mappedToShiftOperation.has_value() && shiftAmount.has_value()) {
        const std::optional<unsigned int> expectedBitwidthOfOperandsInLhsOperand = optionalDeterminedOperandBitwidth.has_value() ? std::make_optional(optionalDeterminedOperandBitwidth->operandBitwidth) : std::nullopt;
        if (const std::optional<syrec::Expression::ptr> optionalSimplifiedShiftExpr = trySimplifyShiftExpression(syrec::ShiftExpression(*toBeShiftedOperand, *mappedToShiftOperation, *shiftAmount), expectedBitwidthOfOperandsInLhsOperand); optionalSimplifiedShiftExpr.has_value()) {
            // If the binary expression evaluated to a numeric expression then the expected operand bitwidth needs to be reset since we cannot assume the expected bitwidth for such an expression.
            if (const auto& simplifiedShiftExprAsNumericOne = optionalDeterminedOperandBitwidth.has_value() ? std::dynamic_pointer_cast<syrec::NumericExpression>(*optionalSimplifiedShiftExpr) : nullptr; simplifiedShiftExprAsNumericOne != nullptr) {
                optionalDeterminedOperandBitwidth.reset();
            }
            return optionalSimplifiedShiftExpr;
        }
        return std::make_shared<syrec::ShiftExpression>(*toBeShiftedOperand, *mappedToShiftOperation, *shiftAmount);
    }
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::visitUnaryExpressionTyped(const TSyrecParser::UnaryExpressionContext* context, [[maybe_unused]] std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth) const {
    if (context != nullptr && context->start != nullptr) {
        recordCustomError(mapTokenPositionToMessagePosition(*context->start), "Unary expressions are currently not supported");
    }
    // As a future note, identical to the behaviour in the shift expression, the unary operation applied by this type of expression does not the change the bitwidth of the latter which
    // in turn means that no truncation is required.
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::visitExpressionFromNumberTyped(const TSyrecParser::ExpressionFromNumberContext* context) const {
    if (const auto& generatedNumberContainer = context != nullptr ? visitNumberTyped(context->number()) : std::nullopt; generatedNumberContainer.has_value()) {
        return std::make_shared<syrec::NumericExpression>(*generatedNumberContainer, DEFAULT_EXPRESSION_BITWIDTH);
    }
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::visitExpressionFromSignalTyped(const TSyrecParser::ExpressionFromSignalContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth) {
    if (const auto& generatedSignalContainer = context != nullptr ? visitSignalTyped(context->signal(), &optionalDeterminedOperandBitwidth) : std::nullopt; generatedSignalContainer.has_value()) {
        return std::make_shared<syrec::VariableExpression>(*generatedSignalContainer);
    }
    return std::nullopt;
}

std::optional<syrec::Number::ptr> CustomExpressionVisitor::visitNumberTyped(const TSyrecParser::NumberContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    if (const auto* const numberFromConstantContext = dynamic_cast<const TSyrecParser::NumberFromConstantContext*>(context); numberFromConstantContext != nullptr) {
        return visitNumberFromConstantTyped(numberFromConstantContext);
    }
    if (const auto* const numberFromExpressionContext = dynamic_cast<const TSyrecParser::NumberFromExpressionContext*>(context); numberFromExpressionContext != nullptr) {
        return visitNumberFromExpressionTyped(numberFromExpressionContext);
    }
    if (const auto* const numberFromLoopVariableContext = dynamic_cast<const TSyrecParser::NumberFromLoopVariableContext*>(context); numberFromLoopVariableContext != nullptr) {
        return visitNumberFromLoopVariableTyped(numberFromLoopVariableContext);
    }
    if (const auto* const numberFromSignalWidthContext = dynamic_cast<const TSyrecParser::NumberFromSignalwidthContext*>(context); numberFromSignalWidthContext != nullptr) {
        return visitNumberFromSignalwidthTyped(numberFromSignalWidthContext);
    }
    // We should not have to report an error at this position since the tokenizer should already report an error if the currently processed token is
    // not in the union of the FIRST sets of the potential alternatives.
    //recordCustomError(Message::Position(0, 0), "Unhandled number context variant. This should not happen");
    return std::nullopt;
}

std::optional<syrec::Number::ptr> CustomExpressionVisitor::visitNumberFromConstantTyped(const TSyrecParser::NumberFromConstantContext* context) const {
    // Production should only be called if the token contains only numeric characters and thus deserialization should only fail if an overflow occurs.
    // Leading and trailing whitespace should also be trimmed from the token text by the parser. If the text of the token contains non-numeric characters,
    // the deserialization will not throw an exception.
    if (context == nullptr || context->literalInt() == nullptr) {
        return std::nullopt;
    }

    bool didSerializationOfIntegerFromStringFailDueToValueOverflow = false;
    if (const std::optional<unsigned int> constantValue = deserializeConstantFromString(context->literalInt()->getText(), &didSerializationOfIntegerFromStringFailDueToValueOverflow); constantValue.has_value() && !didSerializationOfIntegerFromStringFailDueToValueOverflow) {
        recordExpressionComponent(*constantValue);
        return std::make_shared<syrec::Number>(*constantValue);
    }

    if (didSerializationOfIntegerFromStringFailDueToValueOverflow) {
        recordSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(mapTokenPositionToMessagePosition(*context->literalInt()->getSymbol()), context->literalInt()->getText(), UINT_MAX);
    }
    return std::nullopt;
}

std::optional<syrec::Number::ptr> CustomExpressionVisitor::visitNumberFromExpressionTyped(const TSyrecParser::NumberFromExpressionContext* context) const {
    if (context == nullptr) {
        return std::nullopt;
    }

    recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::ExpressionBracketKind::Opening);
    std::optional<syrec::Number::ptr> lhsOperand = visitNumberTyped(context->lhsOperand);

    const std::optional<syrec::Number::ConstantExpression::Operation> operation = context->op != nullptr ? mapTokenToConstantExpressionOperation(*context) : std::nullopt;
    if (context->op != nullptr && !operation.has_value()) {
        recordSemanticError<SemanticError::UnhandledOperationFromGrammarInParser>(mapTokenPositionToMessagePosition(*context->op), context->op->getText());
    }

    if (operation.has_value()) {
        switch (*operation) {
            case syrec::Number::ConstantExpression::Operation::Addition:
                recordExpressionComponent(syrec::BinaryExpression::BinaryOperation::Add);
                break;
            case syrec::Number::ConstantExpression::Operation::Subtraction:
                recordExpressionComponent(syrec::BinaryExpression::BinaryOperation::Subtract);
                break;
            case syrec::Number::ConstantExpression::Operation::Multiplication:
                recordExpressionComponent(syrec::BinaryExpression::BinaryOperation::Multiply);
                break;
            case syrec::Number::ConstantExpression::Operation::Division:
                recordExpressionComponent(syrec::BinaryExpression::BinaryOperation::Divide);
                break;
        }
    }

    std::optional<syrec::Number::ptr> rhsOperand = visitNumberTyped(context->rhsOperand);
    recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::ExpressionBracketKind::Closing);

    const std::optional<unsigned int> evaluationResultOfLhsOperand = lhsOperand.has_value() && *lhsOperand ? lhsOperand->get()->tryEvaluate({}) : std::nullopt;
    const std::optional<unsigned int> evaluationResultOfRhsOperand = rhsOperand.has_value() && *rhsOperand ? rhsOperand->get()->tryEvaluate({}) : std::nullopt;

    if (operation.has_value()) {
        switch (*operation) {
            case syrec::Number::ConstantExpression::Operation::Addition: {
                if (evaluationResultOfLhsOperand.has_value() && *evaluationResultOfLhsOperand == 0U) {
                    return rhsOperand;
                }
                if (evaluationResultOfRhsOperand.has_value() && *evaluationResultOfRhsOperand == 0U) {
                    return lhsOperand;
                }
                break;
            }
            case syrec::Number::ConstantExpression::Operation::Subtraction: {
                if (evaluationResultOfRhsOperand.has_value() && *evaluationResultOfRhsOperand == 0) {
                    return lhsOperand;
                }
                break;
            }
            case syrec::Number::ConstantExpression::Operation::Multiplication: {
                if (evaluationResultOfLhsOperand.has_value() && *evaluationResultOfLhsOperand == 1) {
                    return rhsOperand;
                }

                if (evaluationResultOfRhsOperand.has_value() && *evaluationResultOfRhsOperand == 1) {
                    return lhsOperand;
                }
                break;
            }
            case syrec::Number::ConstantExpression::Operation::Division: {
                if (evaluationResultOfRhsOperand.has_value()) {
                    if (*evaluationResultOfRhsOperand == 0) {
                        recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->rhsOperand->getStart()));
                        return std::nullopt;
                    }
                    if (*evaluationResultOfRhsOperand == 1) {
                        return lhsOperand;
                    }
                }

                if (evaluationResultOfLhsOperand.has_value() && *evaluationResultOfLhsOperand == 0) {
                    return std::make_shared<syrec::Number>(0);
                }
            }
        }

        if (lhsOperand.has_value() && rhsOperand.has_value()) {
            const auto constantExpression = syrec::Number::ConstantExpression(*lhsOperand, *operation, *rhsOperand);
            if (const std::optional<unsigned int> evaluatedConstantExpressionValue = constantExpression.tryEvaluate({}); evaluatedConstantExpressionValue.has_value()) {
                return std::make_shared<syrec::Number>(*evaluatedConstantExpressionValue);
            }
            return std::make_shared<syrec::Number>(constantExpression);
        }
    }
    return std::nullopt;
}

std::optional<syrec::Number::ptr> CustomExpressionVisitor::visitNumberFromLoopVariableTyped(const TSyrecParser::NumberFromLoopVariableContext* context) const {
    if (context == nullptr || context->literalIdent() == nullptr) {
        return std::nullopt;
    }

    const std::optional<utils::TemporaryVariableScope::ptr> activeVariableScopeInSymbolTable = symbolTable->getActiveTemporaryScope();
    if (!activeVariableScopeInSymbolTable) {
        return std::nullopt;
    }

    const std::string loopVariableIdentifier = "$" + context->literalIdent()->getText();
    recordExpressionComponent(loopVariableIdentifier);
    if (const std::optional<utils::TemporaryVariableScope::ScopeEntry::readOnlyPtr> matchingLoopVariableForIdentifier = activeVariableScopeInSymbolTable->get()->getVariableByName(loopVariableIdentifier); matchingLoopVariableForIdentifier.has_value() && matchingLoopVariableForIdentifier->get()->isReferenceToLoopVariable()) {
        if (optionalRestrictionOnLoopVariableUsageInLoopVariableValueInitialization.has_value() && optionalRestrictionOnLoopVariableUsageInLoopVariableValueInitialization.value() == loopVariableIdentifier) {
            recordSemanticError<SemanticError::ValueOfLoopVariableNotUsableInItsInitialValueDeclaration>(mapTokenPositionToMessagePosition(*context->literalLoopVariablePrefix()->getSymbol()), loopVariableIdentifier);
        }
        if (const std::optional<unsigned int> valueOfLoopVariable = activeVariableScopeInSymbolTable->get()->getValueOfLoopVariable(loopVariableIdentifier); valueOfLoopVariable.has_value()) {
            return std::make_shared<syrec::Number>(*valueOfLoopVariable);
        }
        if (const std::optional<syrec::Number::ptr>& symbolTableEntryForLoopVariable = matchingLoopVariableForIdentifier->get()->getLoopVariableData(); symbolTableEntryForLoopVariable.has_value()) {
            return symbolTableEntryForLoopVariable;
        }
        if (context->literalLoopVariablePrefix() != nullptr) {
            recordCustomError(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), "Symbol table entry for loop variable with identifier " + loopVariableIdentifier + " did not return variable data. This should not happen");
        }
    }

    if (context->literalLoopVariablePrefix() != nullptr) {
        // It would not make sense to report at the position of the loop variable prefix '$' that the loop variable '$<LOOP_VAR>' was not declared the loop variable prefix did not exist.
        recordSemanticError<SemanticError::NoVariableMatchingIdentifier>(mapTokenPositionToMessagePosition(*context->literalLoopVariablePrefix()->getSymbol()), loopVariableIdentifier);
    }
    return std::nullopt;
}

std::optional<syrec::Number::ptr> CustomExpressionVisitor::visitNumberFromSignalwidthTyped(const TSyrecParser::NumberFromSignalwidthContext* context) const {
    if (context == nullptr || context->literalIdent() == nullptr) {
        return std::nullopt;
    }

    const std::optional<utils::TemporaryVariableScope::ptr> activeVariableScopeInSymbolTable = symbolTable->getActiveTemporaryScope();
    if (!activeVariableScopeInSymbolTable) {
        return std::nullopt;
    }

    const std::string& variableIdentifier = context->literalIdent()->getSymbol()->getText();
    recordExpressionComponent(context->literalSignalWidthPrefix()->getSymbol()->getText() + variableIdentifier);
    if (const std::optional<utils::TemporaryVariableScope::ScopeEntry::readOnlyPtr> matchingVariableForIdentifier = activeVariableScopeInSymbolTable->get()->getVariableByName(variableIdentifier); matchingVariableForIdentifier.has_value()) {
        if (matchingVariableForIdentifier->get()->getDeclaredVariableBitwidth().has_value()) {
            return std::make_shared<syrec::Number>(*matchingVariableForIdentifier->get()->getDeclaredVariableBitwidth());
        }
        return std::nullopt;
    }
    recordSemanticError<SemanticError::NoVariableMatchingIdentifier>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), variableIdentifier);
    return std::nullopt;
}

std::optional<syrec::VariableAccess::ptr> CustomExpressionVisitor::visitSignalTyped(const TSyrecParser::SignalContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>* optionalDeterminedOperandBitwidth) {
    if (context == nullptr || context->literalIdent() == nullptr) {
        return std::nullopt;
    }

    const std::optional<utils::TemporaryVariableScope::ptr> activeVariableScopeInSymbolTable = symbolTable->getActiveTemporaryScope();
    if (!activeVariableScopeInSymbolTable) {
        return std::nullopt;
    }

    // If we are omitting the variable identifier in the production, a default token for the variable identifier is generated (its text states that the IDENT token is missing) and its
    // 'error' message text used as the variables identifier and thus potentially reported in semantic errors. Since the lexer will already generate an identical syntax error, we assume that
    // the actual variable identifier is empty in this error case.
    const std::string& variableIdentifier = context->literalIdent()->getTreeType() != antlr4::tree::ParseTreeType::ERROR ? context->literalIdent()->getSymbol()->getText() : "";

    const std::optional<utils::TemporaryVariableScope::ScopeEntry::readOnlyPtr> matchingVariableForIdentifier = !variableIdentifier.empty() ? activeVariableScopeInSymbolTable->get()->getVariableByName(variableIdentifier) : std::nullopt;

    if (!variableIdentifier.empty()) {
        recordExpressionComponent(variableIdentifier);
        if (!matchingVariableForIdentifier.has_value() || !matchingVariableForIdentifier.value()->getVariableData().has_value()) {
            recordSemanticError<SemanticError::NoVariableMatchingIdentifier>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), variableIdentifier);
            return std::nullopt;
        }
    }

    const std::size_t          numUserAccessedDimensions = context->accessedDimensions.size();
    syrec::VariableAccess::ptr generatedVariableAccess   = std::make_shared<syrec::VariableAccess>();
    generatedVariableAccess->indexes                     = syrec::Expression::vec(numUserAccessedDimensions, nullptr);

    std::optional<bool> backupOfStatusWhetherDimensionAccessIsCurrentlyProcessed;
    if (!context->accessedDimensions.empty()) {
        backupOfStatusWhetherDimensionAccessIsCurrentlyProcessed = isCurrentlyProcessingDimensionAccessOfVariableAccess();
        markStartOfProcessingOfDimensionAccessOfVariableAccess();
    }

    for (std::size_t i = 0; i < context->accessedDimensions.size(); ++i) {
        // Due to the implemented optimization/simplifications of expression (i.e. an expression multiplied with 0 can be replaced with the latter), we need to record the components of the user defined
        // variable access in their unoptimized 'form' which are available during the processing of said components by the parser.
        recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::VariableAccessComponent::DimensionAccessExpressionStart);
        std::optional<DeterminedExpressionOperandBitwidthInformation> determinedBitwidthOfOperandsInDimensionAccessExpr;
        if (const std::optional<syrec::Expression::ptr> exprDefiningAccessedValueOfDimension = visitExpressionTyped(context->accessedDimensions.at(i), determinedBitwidthOfOperandsInDimensionAccessExpr); exprDefiningAccessedValueOfDimension.has_value()) {
            generatedVariableAccess->indexes[i] = *exprDefiningAccessedValueOfDimension;
        }

        recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::VariableAccessComponent::DimensionAccessExpressionEnd);
        // Any generated bitwidth restriction generated during the processing of the expression defining the accessed value of the dimension needs to be cleared if the latter was processed to prevent
        // the propagation of the restriction to the parsing process for the remaining components of the variable access
        if (determinedBitwidthOfOperandsInDimensionAccessExpr.has_value()) {
            // Regardless of whether variable accesses with an unknown length of the accessed bitrange were defined in the expression (referred to as E) specifying the index value for the currently processed dimension of the dimension access,
            // truncation of constant values in any binary expression that is part of E can use the set bitlength of any variable access (since we are assuming that the user defined a well formed expression [i.e. the accessed bitrange of any variable access operands
            // has the same length]).
            bool detectedDivisionByZeroDuringTruncationOfConstantValues = false;
            truncateConstantValuesInExpression(generatedVariableAccess->indexes[i], determinedBitwidthOfOperandsInDimensionAccessExpr->operandBitwidth, parserConfiguration.integerConstantTruncationOperation, &detectedDivisionByZeroDuringTruncationOfConstantValues);
            if (detectedDivisionByZeroDuringTruncationOfConstantValues) {
                recordSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(mapTokenPositionToMessagePosition(*context->accessedDimensions.at(i)->getStart()));
            }
        }
    }

    if (backupOfStatusWhetherDimensionAccessIsCurrentlyProcessed.has_value() && !*backupOfStatusWhetherDimensionAccessIsCurrentlyProcessed) {
        markEndOfProcessingOfDimensionAccessOfVariableAccess();
    }

    if (matchingVariableForIdentifier.has_value()) {
        const std::vector<unsigned int>& declaredValuesPerDimensionOfReferenceVariable = matchingVariableForIdentifier.value()->getDeclaredVariableDimensions();

        if (const std::optional<syrec::Variable::ptr>& variableDataFromSymbolTable = matchingVariableForIdentifier->get()->getVariableData(); variableDataFromSymbolTable.has_value()) {
            // Currently the shared_pointer instance returned by the symbol table for entry of the variable matching the current identifier cannot be assigned to the variable access
            // data field since it expects a shared_pointer instance to a modifiable variable, thus we are forced to create a copy of the variable data from the symbol table. Future
            // versions might change the variable field in the variable access to match the value returned by the symbol table (Reasoning: Why would the user modify the referenced variable data in the variable access
            // when he can simply reassign the smart_pointer).
            generatedVariableAccess->setVar(*variableDataFromSymbolTable);
        } else {
            recordCustomError(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), "Symbol table entry for variable with identifier " + variableIdentifier + " did not return variable data. This should not happen");
        }

        if (numUserAccessedDimensions == 0) {
            if (declaredValuesPerDimensionOfReferenceVariable.size() == 1 && declaredValuesPerDimensionOfReferenceVariable.front() == 1) {
                generatedVariableAccess->indexes.emplace_back(std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(0), 1));
            } else {
                recordSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()));
            }
        } else if (numUserAccessedDimensions != declaredValuesPerDimensionOfReferenceVariable.size()) {
            if (numUserAccessedDimensions > declaredValuesPerDimensionOfReferenceVariable.size()) {
                recordSemanticError<SemanticError::TooManyDimensionsAccessed>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), numUserAccessedDimensions, declaredValuesPerDimensionOfReferenceVariable.size());
            } else {
                // Checking whether the dimension access of the variable was fully defined by the user prevents the propagation of non-1D signal values
                recordSemanticError<SemanticError::TooFewDimensionsAccessed>(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), numUserAccessedDimensions, declaredValuesPerDimensionOfReferenceVariable.size());
            }
        }

        if (const std::optional<utils::VariableAccessIndicesValidity> indexValidityOfUserDefinedAccessedValuesPerDimension = utils::validateVariableAccessIndices(*generatedVariableAccess); indexValidityOfUserDefinedAccessedValuesPerDimension.has_value() && !indexValidityOfUserDefinedAccessedValuesPerDimension->isValid()) {
            const std::size_t numDimensionsToCheck = declaredValuesPerDimensionOfReferenceVariable.size();
            for (std::size_t dimensionIdx = 0; dimensionIdx < numDimensionsToCheck; ++dimensionIdx) {
                const utils::VariableAccessIndicesValidity::IndexValidationResult validityOfAccessedValueOfDimension = indexValidityOfUserDefinedAccessedValuesPerDimension->accessedValuePerDimensionValidity.at(dimensionIdx);
                // We should not have to check whether the index validation result for the given index contains a value when an out of range access is reported except for an error in the implementation of the overlap check.
                if (validityOfAccessedValueOfDimension.indexValidity == utils::VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange && validityOfAccessedValueOfDimension.indexValue.has_value()) {
                    recordSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(mapTokenPositionToMessagePosition(*context->accessedDimensions.at(dimensionIdx)->getStart()), validityOfAccessedValueOfDimension.indexValue.value(), dimensionIdx, declaredValuesPerDimensionOfReferenceVariable.at(dimensionIdx));
                }
            }
        }
    }

    if (context->bitStart != nullptr) {
        recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::VariableAccessComponent::BitrangeStart);
    }
    const std::optional<syrec::Number::ptr> bitRangeStart = visitNumberTyped(context->bitStart);

    if (context->bitRangeEnd != nullptr) {
        recordExpressionComponent(utils::IfStatementExpressionComponentsRecorder::VariableAccessComponent::BitrangeEnd);
    }
    const std::optional<syrec::Number::ptr> bitRangeEnd = visitNumberTyped(context->bitRangeEnd);

    // Premature exit since index checks for both the defined dimension as well as bit range access depend on the information from the referenced variable.
    if (generatedVariableAccess->var == nullptr) {
        return std::nullopt;
    }

    if (bitRangeStart.has_value() || bitRangeEnd.has_value()) {
        if (bitRangeStart.has_value() && bitRangeEnd.has_value()) {
            generatedVariableAccess->range = std::make_pair(bitRangeStart.value(), bitRangeEnd.value());
        } else if (bitRangeStart.has_value()) {
            generatedVariableAccess->range = std::make_pair(bitRangeStart.value(), bitRangeStart.value());
        } else {
            generatedVariableAccess->range = std::make_pair(bitRangeEnd.value(), bitRangeEnd.value());
        }

        syrec::VariableAccess temporaryVariableAccess = syrec::VariableAccess();
        temporaryVariableAccess.setVar(generatedVariableAccess->var);
        temporaryVariableAccess.range = generatedVariableAccess->range;

        if (const std::optional<utils::VariableAccessIndicesValidity> indexValidityOfUserDefinedAccessOnBitrange = utils::validateVariableAccessIndices(temporaryVariableAccess); indexValidityOfUserDefinedAccessOnBitrange.has_value() && !indexValidityOfUserDefinedAccessOnBitrange->isValid() && indexValidityOfUserDefinedAccessOnBitrange->bitRangeAccessValidity.has_value()) {
            if (bitRangeStart.has_value() && bitRangeEnd.has_value()) {
                if (const utils::VariableAccessIndicesValidity::IndexValidationResult accessedBitRangeStartIndexValidity = indexValidityOfUserDefinedAccessOnBitrange->bitRangeAccessValidity->bitRangeStartValidity;
                    accessedBitRangeStartIndexValidity.indexValidity == utils::VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange && accessedBitRangeStartIndexValidity.indexValue.has_value()) {
                    recordSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(mapTokenPositionToMessagePosition(*context->bitStart->getStart()), accessedBitRangeStartIndexValidity.indexValue.value(), generatedVariableAccess->var->bitwidth);
                }

                if (const utils::VariableAccessIndicesValidity::IndexValidationResult accessedBitRangeEndIndexValidity = indexValidityOfUserDefinedAccessOnBitrange->bitRangeAccessValidity->bitRangeEndValidity;
                    accessedBitRangeEndIndexValidity.indexValidity == utils::VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange && accessedBitRangeEndIndexValidity.indexValue.has_value()) {
                    recordSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(mapTokenPositionToMessagePosition(*context->bitRangeEnd->getStart()), accessedBitRangeEndIndexValidity.indexValue.value(), generatedVariableAccess->var->bitwidth);
                }
            } else if (const std::optional<utils::VariableAccessIndicesValidity::IndexValidationResult> accessedBitIndexValidity = bitRangeStart.has_value() ? indexValidityOfUserDefinedAccessOnBitrange->bitRangeAccessValidity->bitRangeStartValidity : indexValidityOfUserDefinedAccessOnBitrange->bitRangeAccessValidity->bitRangeEndValidity;
                       accessedBitIndexValidity.has_value() && accessedBitIndexValidity->indexValidity == utils::VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange && accessedBitIndexValidity->indexValue.has_value()) {
                recordSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(
                        mapTokenPositionToMessagePosition(bitRangeStart.has_value() ? *context->bitStart->getStart() : *context->bitRangeEnd->getStart()),
                        accessedBitIndexValidity->indexValue.value(),
                        generatedVariableAccess->var->bitwidth);
            }
        }
    }

    // Since the error reported when defining an overlapping variable access is reported at the position of the variable identifier, this check needs to be performed prior
    // to the check for matching operand bitwidths (if no internal ordering of the reported errors is performed [which is currently the case])
    if (const std::optional<utils::VariableAccessOverlapCheckResult>& overlapCheckResultWithRestrictedVariableParts = optionalRestrictionOnVariableAccesses.has_value() && *optionalRestrictionOnVariableAccesses && (!isCurrentlyProcessingDimensionAccessOfVariableAccess() || !parserConfiguration.allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess) ? utils::checkOverlapBetweenVariableAccesses(**optionalRestrictionOnVariableAccesses, *generatedVariableAccess) : std::nullopt;
        overlapCheckResultWithRestrictedVariableParts.has_value() && overlapCheckResultWithRestrictedVariableParts->overlapState == utils::VariableAccessOverlapCheckResult::OverlapState::Overlapping) {
        if (!overlapCheckResultWithRestrictedVariableParts->overlappingIndicesInformation.has_value()) {
            recordCustomError(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()), "Overlap with restricted variable parts detected but no further information about overlap available. This should not happen");
        } else {
            if (isCurrentlyProcessingDimensionAccessOfVariableAccess()) {
                recordSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
                        mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()),
                        overlapCheckResultWithRestrictedVariableParts->stringifyOverlappingIndicesInformation());
            } else {
                recordSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
                        mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol()),
                        overlapCheckResultWithRestrictedVariableParts->stringifyOverlappingIndicesInformation());
            }
        }
    }

    std::optional<unsigned int> accessedBitRangeStart;
    if (context->bitStart != nullptr) {
        accessedBitRangeStart = bitRangeStart.has_value() ? bitRangeStart->get()->tryEvaluate({}) : std::nullopt;
    } else {
        accessedBitRangeStart = 0;
    }

    std::optional<unsigned int> accessedBitRangeEnd;
    if (context->bitRangeEnd != nullptr) {
        accessedBitRangeEnd = bitRangeEnd.has_value() ? bitRangeEnd->get()->tryEvaluate({}) : std::nullopt;
    } else {
        accessedBitRangeEnd = context->bitStart != nullptr ? accessedBitRangeStart : generatedVariableAccess->getVar()->bitwidth - 1;
    }

    std::optional<unsigned int> userAccessedBitrangeLength;
    if (accessedBitRangeStart.has_value() && accessedBitRangeEnd.has_value()) {
        userAccessedBitrangeLength = (*accessedBitRangeStart > *accessedBitRangeEnd ? *accessedBitRangeStart - *accessedBitRangeEnd : *accessedBitRangeEnd - *accessedBitRangeStart) + 1;
    } else if (context->bitStart != nullptr && context->bitRangeEnd == nullptr) {
        userAccessedBitrangeLength = 1;
    }

    if (userAccessedBitrangeLength.has_value() && optionalDeterminedOperandBitwidth != nullptr) {
        *optionalDeterminedOperandBitwidth = DeterminedExpressionOperandBitwidthInformation({*userAccessedBitrangeLength,
                                                                                             context->literalIdent()->getSymbol() != nullptr ? std::make_optional(mapTokenPositionToMessagePosition(*context->literalIdent()->getSymbol())) : std::nullopt});
    }
    return generatedVariableAccess;
}

void CustomExpressionVisitor::clearRestrictionOnVariableAccesses() {
    optionalRestrictionOnVariableAccesses.reset();
}

void CustomExpressionVisitor::setRestrictionOnLoopVariablesUsableInFutureLoopVariableValueInitializations(const std::string_view& loopVariableIdentifier) {
    optionalRestrictionOnLoopVariableUsageInLoopVariableValueInitialization = loopVariableIdentifier;
}

void CustomExpressionVisitor::clearRestrictionOnLoopVariablesUsableInFutureLoopVariableValueInitializations() {
    optionalRestrictionOnLoopVariableUsageInLoopVariableValueInitialization.reset();
}

void CustomExpressionVisitor::setIfStatementExpressionComponentsRecorder(const utils::IfStatementExpressionComponentsRecorder::ptr& ifStatementExpressionComponentsRecorder) {
    if (ifStatementExpressionComponentsRecorder == nullptr) {
        return;
    }
    optionalIfStatementExpressionComponentsRecorder = ifStatementExpressionComponentsRecorder;
}

void CustomExpressionVisitor::clearIfStatementExpressionComponentsRecorder() {
    optionalIfStatementExpressionComponentsRecorder.reset();
}

void CustomExpressionVisitor::markStartOfProcessingOfDimensionAccessOfVariableAccess() {
    isCurrentlyProcessingDimensionAccessOfVariableAccessFlag = true;
}

void CustomExpressionVisitor::markEndOfProcessingOfDimensionAccessOfVariableAccess() {
    isCurrentlyProcessingDimensionAccessOfVariableAccessFlag = false;
}

std::optional<utils::IfStatementExpressionComponentsRecorder::ptr> CustomExpressionVisitor::getIfStatementExpressionComponentsRecorder() const {
    return optionalIfStatementExpressionComponentsRecorder;
}

bool CustomExpressionVisitor::setRestrictionOnVariableAccesses(const syrec::VariableAccess::ptr& notAccessiblePartsForFutureVariableAccesses) {
    clearRestrictionOnVariableAccesses();
    if (notAccessiblePartsForFutureVariableAccesses == nullptr || notAccessiblePartsForFutureVariableAccesses->var == nullptr || notAccessiblePartsForFutureVariableAccesses->var->name.empty() || std::any_of(notAccessiblePartsForFutureVariableAccesses->indexes.cbegin(), notAccessiblePartsForFutureVariableAccesses->indexes.cbegin(), [](const syrec::Expression::ptr& exprDefiningAccessedValueOfDimension) {
            return !tryGetConstantValueOf(*exprDefiningAccessedValueOfDimension).has_value();
        })) {
        return false;
    }
    optionalRestrictionOnVariableAccesses = notAccessiblePartsForFutureVariableAccesses;
    return true;
}

bool CustomExpressionVisitor::isCurrentlyProcessingDimensionAccessOfVariableAccess() const {
    return isCurrentlyProcessingDimensionAccessOfVariableAccessFlag;
}

bool CustomExpressionVisitor::truncateConstantValuesInExpression(syrec::Expression::ptr& expression, unsigned int expectedBitwidthOfOperandsInExpression, const utils::IntegerConstantTruncationOperation truncationOperationToUseForIntegerConstants, bool* detectedDivisionByZero) {
    if (expression == nullptr) {
        return false;
    }

    bool wasOriginalExprModified = false;
    if (auto* const exprAsBinaryExpr = dynamic_cast<syrec::BinaryExpression*>(&*expression); exprAsBinaryExpr != nullptr) {
        if (isBinaryOperationARelationalOrLogicalOne(exprAsBinaryExpr->binaryOperation)) {
            return false;
        }
        const bool wasLhsExprModified = exprAsBinaryExpr->lhs != nullptr ? truncateConstantValuesInExpression(exprAsBinaryExpr->lhs, expectedBitwidthOfOperandsInExpression, truncationOperationToUseForIntegerConstants, detectedDivisionByZero) : false;
        const bool wasRhsExprModified = exprAsBinaryExpr->rhs != nullptr ? truncateConstantValuesInExpression(exprAsBinaryExpr->rhs, expectedBitwidthOfOperandsInExpression, truncationOperationToUseForIntegerConstants, detectedDivisionByZero) : false;

        if (wasLhsExprModified || wasRhsExprModified) {
            if (const std::optional<syrec::Expression::ptr> simplifiedBinaryExpr = trySimplifyBinaryExpression(*exprAsBinaryExpr, expectedBitwidthOfOperandsInExpression, detectedDivisionByZero); simplifiedBinaryExpr.has_value()) {
                expression = *simplifiedBinaryExpr;
                return true;
            }
        }
    } else if (auto* const exprAsShiftExpr = dynamic_cast<syrec::ShiftExpression*>(&*expression); exprAsShiftExpr != nullptr) {
        wasOriginalExprModified = truncateConstantValuesInExpression(exprAsShiftExpr->lhs, expectedBitwidthOfOperandsInExpression, truncationOperationToUseForIntegerConstants, detectedDivisionByZero);
        if (const std::optional<syrec::Expression::ptr> simplfifiedShiftExpr = trySimplifyShiftExpression(*exprAsShiftExpr, expectedBitwidthOfOperandsInExpression); simplfifiedShiftExpr.has_value()) {
            expression = *simplfifiedShiftExpr;
        }
    } else if (auto* const exprAsNumericExpr = dynamic_cast<syrec::NumericExpression*>(&*expression); exprAsNumericExpr != nullptr) {
        if (exprAsNumericExpr->value == nullptr) {
            return false;
        }

        if (!exprAsNumericExpr->value->isConstant()) {
            exprAsNumericExpr->bwidth = expectedBitwidthOfOperandsInExpression;
            wasOriginalExprModified   = true;
        }

        if (const std::optional<unsigned int> constantValueOfNumericExpr = exprAsNumericExpr->value->tryEvaluate({}); constantValueOfNumericExpr.has_value()) {
            exprAsNumericExpr->value  = std::make_shared<syrec::Number>(truncateConstantValueToExpectedBitwidth(*constantValueOfNumericExpr, expectedBitwidthOfOperandsInExpression, truncationOperationToUseForIntegerConstants));
            exprAsNumericExpr->bwidth = expectedBitwidthOfOperandsInExpression;
            wasOriginalExprModified   = true;
        }
    }
    return wasOriginalExprModified;
}

// START OF NON-PUBLIC FUNCTIONALITY
void CustomExpressionVisitor::recordExpressionComponent(const utils::IfStatementExpressionComponentsRecorder::ExpressionComponent& expressionComponent) const {
    if (!optionalIfStatementExpressionComponentsRecorder.has_value() || *optionalIfStatementExpressionComponentsRecorder == nullptr) {
        return;
    }
    optionalIfStatementExpressionComponentsRecorder->get()->recordExpressionComponent(expressionComponent);
}

std::optional<syrec::BinaryExpression::BinaryOperation> CustomExpressionVisitor::mapTokenToBinaryOperation(const TSyrecParser::BinaryExpressionContext& binaryExpressionContext) {
    if (binaryExpressionContext.literalOpPlus() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Add;
    }
    if (binaryExpressionContext.literalOpMinus() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Subtract;
    }
    if (binaryExpressionContext.literalOpBitwiseXor() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Exor;
    }
    if (binaryExpressionContext.literalOpMultiply() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Multiply;
    }
    if (binaryExpressionContext.literalOpDivision() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Divide;
    }
    if (binaryExpressionContext.literalOpModulo() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Modulo;
    }
    if (binaryExpressionContext.literalOpUpperBitMultiply() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::FracDivide;
    }
    if (binaryExpressionContext.literalOpLogicalAnd() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::LogicalAnd;
    }
    if (binaryExpressionContext.literalOpLogicalOr() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::LogicalOr;
    }
    if (binaryExpressionContext.literalOpBitwiseAnd() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::BitwiseAnd;
    }
    if (binaryExpressionContext.literalOpBitwiseOr() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::BitwiseOr;
    }
    if (binaryExpressionContext.literalOpLessThan() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::LessThan;
    }
    if (binaryExpressionContext.literalOpGreaterThan() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::GreaterThan;
    }
    if (binaryExpressionContext.literalOpEqual() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::Equals;
    }
    if (binaryExpressionContext.literalOpNotEqual() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::NotEquals;
    }
    if (binaryExpressionContext.literalOpLessOrEqual() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::LessEquals;
    }
    if (binaryExpressionContext.literalOpGreaterOrEqual() != nullptr) {
        return syrec::BinaryExpression::BinaryOperation::GreaterEquals;
    }
    return std::nullopt;
}

std::optional<syrec::ShiftExpression::ShiftOperation> CustomExpressionVisitor::mapTokenToShiftOperation(const TSyrecParser::ShiftExpressionContext& shiftExpressionContext) {
    if (shiftExpressionContext.literalOpLeftShift() != nullptr) {
        return syrec::ShiftExpression::ShiftOperation::Left;
    }
    if (shiftExpressionContext.literalOpRightShift() != nullptr) {
        return syrec::ShiftExpression::ShiftOperation::Right;
    }
    return std::nullopt;
}

std::optional<syrec::Number::ConstantExpression::Operation> CustomExpressionVisitor::mapTokenToConstantExpressionOperation(const TSyrecParser::NumberFromExpressionContext& constantExpressionContext) {
    if (constantExpressionContext.literalOpPlus() != nullptr) {
        return syrec::Number::ConstantExpression::Operation::Addition;
    }
    if (constantExpressionContext.literalOpMinus() != nullptr) {
        return syrec::Number::ConstantExpression::Operation::Subtraction;
    }
    if (constantExpressionContext.literalOpMultiply() != nullptr) {
        return syrec::Number::ConstantExpression::Operation::Multiplication;
    }
    if (constantExpressionContext.literalOpDivision() != nullptr) {
        return syrec::Number::ConstantExpression::Operation::Division;
    }
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::trySimplifyBinaryExpressionWithConstantValueOfOneOperandKnown(unsigned int knownOperandValue, syrec::BinaryExpression::BinaryOperation binaryOperation, const syrec::Expression::ptr& unknownOperandValue, bool isValueOfLhsOperandKnown) {
    if (knownOperandValue > 1) {
        return std::nullopt;
    }
    if (knownOperandValue == 1) {
        switch (binaryOperation) {
            case syrec::BinaryExpression::BinaryOperation::Multiply:
            case syrec::BinaryExpression::BinaryOperation::LogicalAnd:
            case syrec::BinaryExpression::BinaryOperation::FracDivide:
                return unknownOperandValue;
            case syrec::BinaryExpression::BinaryOperation::Modulo:
                return isValueOfLhsOperandKnown ? std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(1), 1) : std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(0), 1);
            case syrec::BinaryExpression::BinaryOperation::LogicalOr:
                return std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(1), 1);
            case syrec::BinaryExpression::BinaryOperation::Divide:
                return !isValueOfLhsOperandKnown ? std::make_optional(unknownOperandValue) : std::nullopt;
            default:
                return std::nullopt;
        }
    }

    // Known operand value is zero at this point
    switch (binaryOperation) {
        case syrec::BinaryExpression::BinaryOperation::LogicalAnd:
        case syrec::BinaryExpression::BinaryOperation::BitwiseAnd:
        case syrec::BinaryExpression::BinaryOperation::Multiply:
            return std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(0), 1);
        case syrec::BinaryExpression::BinaryOperation::LogicalOr:
        case syrec::BinaryExpression::BinaryOperation::BitwiseOr:
        case syrec::BinaryExpression::BinaryOperation::Add:
        case syrec::BinaryExpression::BinaryOperation::Exor:
            return unknownOperandValue;
        case syrec::BinaryExpression::BinaryOperation::Subtract:
            // Expression (0 - x) cannot be simplified
            return isValueOfLhsOperandKnown ? std::nullopt : std::make_optional(unknownOperandValue);
        case syrec::BinaryExpression::BinaryOperation::Divide:
        case syrec::BinaryExpression::BinaryOperation::FracDivide:
        case syrec::BinaryExpression::BinaryOperation::Modulo:
            return isValueOfLhsOperandKnown ? std::make_optional(std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(0), 1)) : std::nullopt;
        default:
            break;
    }
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::trySimplifyShiftExpression(const syrec::ShiftExpression& shiftExpr, const std::optional<unsigned int>& optionalBitwidthOfOperandsInExpression) {
    syrec::Expression::ptr   toBeShiftedOperand = shiftExpr.lhs;
    const syrec::Number::ptr shiftAmount        = shiftExpr.rhs;

    const std::optional<unsigned int> constantValueOfToBeShiftedOperand = toBeShiftedOperand != nullptr ? tryGetConstantValueOf(*toBeShiftedOperand) : std::nullopt;
    const std::optional<unsigned int> constantValueOfShiftAmount        = shiftAmount != nullptr ? shiftAmount->tryEvaluate({}) : std::nullopt;
    if (!constantValueOfShiftAmount.has_value()) {
        return std::nullopt;
    }

    // A note regarding the truncation of operands in a shift expression.
    // In case that both operands evaluate to a constant value, evaluation of the expression is performed at compile time and the constant value propagated without truncation (since we do not know anything about any potential outer expression)
    // Otherwise, when the lhs operand evaluate to a variable access or expression, the performed shift will not change the bitwidth of the result and thus neither of the two operands need to be truncated.
    if (*constantValueOfShiftAmount == 0) {
        return toBeShiftedOperand;
    }
    if (constantValueOfToBeShiftedOperand.has_value()) {
        if (const std::optional<unsigned int> evaluationResultOfShiftOperation = utils::tryEvaluate(constantValueOfToBeShiftedOperand, shiftExpr.shiftOperation, constantValueOfShiftAmount); evaluationResultOfShiftOperation.has_value()) {
            return std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(*evaluationResultOfShiftOperation), optionalBitwidthOfOperandsInExpression.value_or(DEFAULT_EXPRESSION_BITWIDTH));
        }
    }
    if (*constantValueOfShiftAmount >= optionalBitwidthOfOperandsInExpression.value_or(MAX_SUPPORTED_SIGNAL_BITWIDTH)) {
        return std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(0), optionalBitwidthOfOperandsInExpression.value_or(1));
    }
    return std::nullopt;
}

std::optional<syrec::Expression::ptr> CustomExpressionVisitor::trySimplifyBinaryExpression(const syrec::BinaryExpression& binaryExpr, const std::optional<unsigned int>& optionalBitwidthOfOperandsInExpression, bool* detectedDivisionByZero) {
    const std::optional<unsigned int> constantValueOfLhsOperand = binaryExpr.lhs != nullptr ? tryGetConstantValueOf(*binaryExpr.lhs) : std::nullopt;
    const std::optional<unsigned int> constantValueOfRhsOperand = binaryExpr.rhs != nullptr ? tryGetConstantValueOf(*binaryExpr.rhs) : std::nullopt;
    if (detectedDivisionByZero != nullptr) {
        *detectedDivisionByZero = (binaryExpr.binaryOperation == syrec::BinaryExpression::BinaryOperation::Divide || binaryExpr.binaryOperation == syrec::BinaryExpression::BinaryOperation::FracDivide) && constantValueOfRhsOperand.has_value() && *constantValueOfRhsOperand == 0;
        if (*detectedDivisionByZero) {
            return std::nullopt;
        }
    }

    if (constantValueOfLhsOperand.has_value() && constantValueOfRhsOperand.has_value()) {
        if (const std::optional<unsigned int> evaluationResultOfExpr = utils::tryEvaluate(constantValueOfLhsOperand, binaryExpr.binaryOperation, constantValueOfRhsOperand); evaluationResultOfExpr.has_value()) {
            return std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(*evaluationResultOfExpr), optionalBitwidthOfOperandsInExpression.value_or(DEFAULT_EXPRESSION_BITWIDTH));
        }
    }

    // We cannot propagate a given expected operand bitwidth to a binary expression that uses a logical or relational operation since the result of the expression has an expected bitwidth equal to 1 while its operands are allowed to use
    // a different operand bitwidth
    if (isBinaryOperationARelationalOrLogicalOne(binaryExpr.binaryOperation)) {
        return std::nullopt;
    }

    if (const std::optional<syrec::Expression::ptr>& simplifiedLhsOperand = constantValueOfLhsOperand.has_value() ? trySimplifyBinaryExpressionWithConstantValueOfOneOperandKnown(*constantValueOfLhsOperand, binaryExpr.binaryOperation, binaryExpr.rhs, true) : std::nullopt; simplifiedLhsOperand.has_value() && *simplifiedLhsOperand) {
        return simplifiedLhsOperand;
    }

    if (const std::optional<syrec::Expression::ptr>& simplifiedRhsOperand = constantValueOfRhsOperand.has_value() ? trySimplifyBinaryExpressionWithConstantValueOfOneOperandKnown(*constantValueOfRhsOperand, binaryExpr.binaryOperation, binaryExpr.lhs, false) : std::nullopt; simplifiedRhsOperand.has_value() && *simplifiedRhsOperand) {
        return simplifiedRhsOperand;
    }
    return std::nullopt;
}
