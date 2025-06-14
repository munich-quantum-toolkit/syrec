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
#include "Token.h"
#include "core/syrec/number.hpp"
#include "core/syrec/parser/components/custom_base_visitor.hpp"
#include "core/syrec/parser/components/custom_expression_visitor.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/parser/utils/symbolTable/base_symbol_table.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace syrec_parser {
    class CustomStatementVisitor: protected CustomBaseVisitor {
    public:
        struct NotOverloadResolutedCallStatementScope {
            using CallStatementInstanceVariant = std::variant<std::shared_ptr<syrec::CallStatement>, std::shared_ptr<syrec::UncallStatement>>;

            struct DeclaredModuleSignature {
                std::string          moduleIdentifier;
                syrec::Variable::vec parameters;

                explicit DeclaredModuleSignature(std::string moduleIdentifier, syrec::Variable::vec parameters):
                    moduleIdentifier(std::move(moduleIdentifier)), parameters(std::move(parameters)) {}
            };

            struct CallStatementData {
                CallStatementInstanceVariant callStatementVariantInstance;
                std::string                  calledModuleIdentifier;
                syrec::Variable::vec         symbolTableEntriesForCallerArguments;
                std::size_t                  linePositionOfModuleIdentifier;
                std::size_t                  columnPositionOfModuleIdentifier;

                explicit CallStatementData(CallStatementInstanceVariant callStatementVariantInstance, std::string calledModuleIdentifier, syrec::Variable::vec symbolTableEntriesForCallerArguments, std::size_t linePositionOfModuleIdentifier, std::size_t columnPositionOfModuleIdentifier):
                    callStatementVariantInstance(std::move(callStatementVariantInstance)),
                    calledModuleIdentifier(std::move(calledModuleIdentifier)), symbolTableEntriesForCallerArguments(std::move(symbolTableEntriesForCallerArguments)),
                    linePositionOfModuleIdentifier(linePositionOfModuleIdentifier), columnPositionOfModuleIdentifier(columnPositionOfModuleIdentifier) {}
            };

            DeclaredModuleSignature        signatureOfModuleContainingCallStatement;
            std::vector<CallStatementData> callStatementsToPerformOverloadResolutionOn;

            explicit NotOverloadResolutedCallStatementScope(DeclaredModuleSignature signatureOfModuleContainingCallStatement):
                signatureOfModuleContainingCallStatement(std::move(signatureOfModuleContainingCallStatement)) {}
        };

        CustomStatementVisitor(const std::shared_ptr<ParserMessagesContainer>& sharedMessagesContainerInstance, const std::shared_ptr<utils::BaseSymbolTable>& sharedSymbolTableInstance, const syrec::ReadProgramSettings& parserConfiguration):
            CustomBaseVisitor(sharedMessagesContainerInstance, sharedSymbolTableInstance, parserConfiguration),
            expressionVisitorInstance(std::make_unique<CustomExpressionVisitor>(sharedMessagesContainerInstance, sharedSymbolTableInstance, parserConfiguration)) {}

        [[nodiscard]] std::optional<syrec::Statement::vec>        visitStatementListTyped(const TSyrecParser::StatementListContext* context);
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitStatementTyped(const TSyrecParser::StatementContext* context);
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitCallStatementTyped(const TSyrecParser::CallStatementContext* context);
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitForStatementTyped(const TSyrecParser::ForStatementContext* context);
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitIfStatementTyped(const TSyrecParser::IfStatementContext* context);
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitUnaryStatementTyped(const TSyrecParser::UnaryStatementContext* context) const;
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitAssignStatementTyped(const TSyrecParser::AssignStatementContext* context) const;
        [[nodiscard]] std::optional<syrec::Statement::ptr>        visitSwapStatementTyped(const TSyrecParser::SwapStatementContext* context) const;
        [[nodiscard]] static std::optional<syrec::Statement::ptr> visitSkipStatementTyped(const TSyrecParser::SkipStatementContext* context);

        [[nodiscard]] std::vector<NotOverloadResolutedCallStatementScope> getCallStatementsWithNotPerformedOverloadResolution() const;
        void                                                              openNewScopeToRecordCallStatementsInModule(const NotOverloadResolutedCallStatementScope::DeclaredModuleSignature& enclosingModuleSignature);

    protected:
        std::unique_ptr<CustomExpressionVisitor>            expressionVisitorInstance;
        std::vector<NotOverloadResolutedCallStatementScope> callStatementsWithNotPerformedOverloadResolutionScopes;

        [[nodiscard]] std::optional<std::string>        visitLoopVariableDefinitionTyped(const TSyrecParser::LoopVariableDefinitionContext* context) const;
        [[nodiscard]] std::optional<syrec::Number::ptr> visitLoopStepsizeDefinitionTyped(const TSyrecParser::LoopStepsizeDefinitionContext* context) const;

        void                                                  recordErrorIfAssignmentToReadonlyVariableIsPerformed(const syrec::Variable& accessedVariable, const antlr4::Token& reportedErrorPosition) const;
        [[nodiscard]] NotOverloadResolutedCallStatementScope* getActiveModuleScopeRecordingCallStatements();

        [[nodiscard]] static std::optional<syrec::AssignStatement::AssignOperation> mapAntlrTokenToAssignmentOperation(const TSyrecParser::AssignStatementContext& assignmentStatementContext);
        [[nodiscard]] static std::optional<syrec::UnaryStatement::UnaryOperation>   mapAntlrTokenToUnaryAssignmentOperation(const TSyrecParser::UnaryStatementContext& unaryStatementContext);
        [[nodiscard]] static bool                                                   doesVariableTypeAllowAssignment(const syrec::Variable::Type variableType) noexcept {
            return variableType == syrec::Variable::Type::Inout || variableType == syrec::Variable::Type::Out || variableType == syrec::Variable::Type::Wire;
        }
    };
} // namespace syrec_parser
