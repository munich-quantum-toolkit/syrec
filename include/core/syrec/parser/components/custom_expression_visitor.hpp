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

#include "TSyrecParser.h"
#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/parser/components/custom_base_visitor.hpp"
#include "core/syrec/parser/utils/if_statement_expression_components_recorder.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/parser/utils/symbolTable/base_symbol_table.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/variable.hpp"

#include <memory>
#include <optional>
#include <string_view>

namespace syrec_parser {
    class CustomExpressionVisitor: protected CustomBaseVisitor {
    public:
        CustomExpressionVisitor(const std::shared_ptr<ParserMessagesContainer>& sharedMessagesContainerInstance, const std::shared_ptr<utils::BaseSymbolTable>& sharedSymbolTableInstance, const syrec::ReadProgramSettings& parserConfiguration):
            CustomBaseVisitor(sharedMessagesContainerInstance, sharedSymbolTableInstance, parserConfiguration) {}

        struct DeterminedExpressionOperandBitwidthInformation {
            unsigned int                     operandBitwidth = 0;
            std::optional<Message::Position> positionOfOperandWithKnownBitwidth;
        };

        [[nodiscard]] std::optional<syrec::Expression::ptr>        visitExpressionTyped(const TSyrecParser::ExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth);
        [[nodiscard]] std::optional<syrec::Expression::ptr>        visitExpressionFromNumberTyped(const TSyrecParser::ExpressionFromNumberContext* context) const;
        [[nodiscard]] std::optional<syrec::Expression::ptr>        visitExpressionFromSignalTyped(const TSyrecParser::ExpressionFromSignalContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth);
        [[nodiscard]] std::optional<syrec::Expression::ptr>        visitBinaryExpressionTyped(const TSyrecParser::BinaryExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth);
        [[nodiscard]] std::optional<syrec::Expression::ptr>        visitUnaryExpressionTyped(const TSyrecParser::UnaryExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth) const;
        [[nodiscard]] std::optional<syrec::Expression::ptr>        visitShiftExpressionTyped(const TSyrecParser::ShiftExpressionContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>& optionalDeterminedOperandBitwidth);
        [[maybe_unused]] std::optional<syrec::VariableAccess::ptr> visitSignalTyped(const TSyrecParser::SignalContext* context, std::optional<DeterminedExpressionOperandBitwidthInformation>* optionalDeterminedOperandBitwidth);
        [[nodiscard]] std::optional<syrec::Number::ptr>            visitNumberTyped(const TSyrecParser::NumberContext* context) const;
        [[nodiscard]] std::optional<syrec::Number::ptr>            visitNumberFromConstantTyped(const TSyrecParser::NumberFromConstantContext* context) const;
        [[nodiscard]] std::optional<syrec::Number::ptr>            visitNumberFromSignalwidthTyped(const TSyrecParser::NumberFromSignalwidthContext* context) const;
        [[nodiscard]] std::optional<syrec::Number::ptr>            visitNumberFromExpressionTyped(const TSyrecParser::NumberFromExpressionContext* context) const;
        [[nodiscard]] std::optional<syrec::Number::ptr>            visitNumberFromLoopVariableTyped(const TSyrecParser::NumberFromLoopVariableContext* context) const;

        void                                                                             clearRestrictionOnVariableAccesses();
        void                                                                             setRestrictionOnLoopVariablesUsableInFutureLoopVariableValueInitializations(const std::string_view& loopVariableIdentifier);
        void                                                                             clearRestrictionOnLoopVariablesUsableInFutureLoopVariableValueInitializations();
        void                                                                             setIfStatementExpressionComponentsRecorder(const utils::IfStatementExpressionComponentsRecorder::ptr& ifStatementExpressionComponentsRecorder);
        void                                                                             clearIfStatementExpressionComponentsRecorder();
        void                                                                             markStartOfProcessingOfDimensionAccessOfVariableAccess();
        void                                                                             markEndOfProcessingOfDimensionAccessOfVariableAccess();
        [[nodiscard]] std::optional<utils::IfStatementExpressionComponentsRecorder::ptr> getIfStatementExpressionComponentsRecorder() const;
        [[maybe_unused]] bool                                                            setRestrictionOnVariableAccesses(const syrec::VariableAccess::ptr& notAccessiblePartsForFutureVariableAccesses);
        [[nodiscard]] bool                                                               isCurrentlyProcessingDimensionAccessOfVariableAccess() const;
        [[maybe_unused]] static bool                                                     truncateConstantValuesInExpression(syrec::Expression::ptr& expression, unsigned int expectedBitwidthOfOperandsInExpression, utils::IntegerConstantTruncationOperation truncationOperationToUseForIntegerConstants, bool* detectedDivisionByZero);

    protected:
        std::optional<syrec::VariableAccess::ptr>                          optionalRestrictionOnVariableAccesses;
        std::optional<std::string_view>                                    optionalRestrictionOnLoopVariableUsageInLoopVariableValueInitialization;
        std::optional<utils::IfStatementExpressionComponentsRecorder::ptr> optionalIfStatementExpressionComponentsRecorder;
        bool                                                               isCurrentlyProcessingDimensionAccessOfVariableAccessFlag = false;

        void recordExpressionComponent(const utils::IfStatementExpressionComponentsRecorder::ExpressionComponent& expressionComponent) const;

        [[nodiscard]] static std::optional<syrec::BinaryExpression::BinaryOperation>     mapTokenToBinaryOperation(const TSyrecParser::BinaryExpressionContext& binaryExpressionContext);
        [[nodiscard]] static std::optional<syrec::ShiftExpression::ShiftOperation>       mapTokenToShiftOperation(const TSyrecParser::ShiftExpressionContext& shiftExpressionContext);
        [[nodiscard]] static std::optional<syrec::Number::ConstantExpression::Operation> mapTokenToConstantExpressionOperation(const TSyrecParser::NumberFromExpressionContext& constantExpressionContext);
        [[nodiscard]] static std::optional<syrec::Expression::ptr>                       trySimplifyBinaryExpressionWithConstantValueOfOneOperandKnown(unsigned int knownOperandValue, syrec::BinaryExpression::BinaryOperation binaryOperation, const syrec::Expression::ptr& unknownOperandValue, bool isValueOfLhsOperandKnown);
        [[nodiscard]] static std::optional<syrec::Expression::ptr>                       trySimplifyShiftExpression(const syrec::ShiftExpression& shiftExpr, const std::optional<unsigned int>& optionalBitwidthOfOperandsInExpression);
        [[nodiscard]] static std::optional<syrec::Expression::ptr>                       trySimplifyBinaryExpression(const syrec::BinaryExpression& binaryExpr, const std::optional<unsigned int>& optionalBitwidthOfOperandsInExpression, bool* detectedDivisionByZero);
        [[nodiscard]] static constexpr bool                                              isBinaryOperationARelationalOrLogicalOne(syrec::BinaryExpression::BinaryOperation binaryOperation) {
            switch (binaryOperation) {
                case syrec::BinaryExpression::BinaryOperation::Equals:
                case syrec::BinaryExpression::BinaryOperation::GreaterThan:
                case syrec::BinaryExpression::BinaryOperation::GreaterEquals:
                case syrec::BinaryExpression::BinaryOperation::LessThan:
                case syrec::BinaryExpression::BinaryOperation::LessEquals:
                case syrec::BinaryExpression::BinaryOperation::NotEquals:
                case syrec::BinaryExpression::BinaryOperation::LogicalAnd:
                case syrec::BinaryExpression::BinaryOperation::LogicalOr:
                    return true;
                default:
                    return false;
            }
        }
    };
} // namespace syrec_parser
