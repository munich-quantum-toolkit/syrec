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

#include "Parser.h"
#include "ParserRuleContext.h"
#include "Token.h"
#include "TokenStream.h"
#include "Vocabulary.h"
#include "atn/ATN.h"
#include "atn/ParserATNSimulatorOptions.h"
#include "atn/SerializedATNView.h"
#include "tree/TerminalNode.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace syrec_parser {
    /**
     * @brief A non-thread-safe ANTLR-generated (v4.13.2) SyReC parser
     */
    class TSyrecParser: public antlr4::Parser {
    public:
        enum : std::uint8_t {
            OpIncrementAssign   = 1,
            OpDecrementAssign   = 2,
            OpInvertAssign      = 3,
            OpAddAssign         = 4,
            OpSubAssign         = 5,
            OpXorAssign         = 6,
            OpPlus              = 7,
            OpMinus             = 8,
            OpMultiply          = 9,
            OpUpperBitMultiply  = 10,
            OpDivision          = 11,
            OpModulo            = 12,
            OpLeftShift         = 13,
            OpRightShift        = 14,
            OpSwap              = 15,
            OpGreaterOrEqual    = 16,
            OpLessOrEqual       = 17,
            OpGreaterThan       = 18,
            OpLessThan          = 19,
            OpEqual             = 20,
            OpNotEqual          = 21,
            OpLogicalAnd        = 22,
            OpLogicalOr         = 23,
            OpLogicalNegation   = 24,
            OpBitwiseAnd        = 25,
            OpBitwiseNegation   = 26,
            OpBitwiseOr         = 27,
            OpBitwiseXor        = 28,
            OpCall              = 29,
            OpUncall            = 30,
            VarTypeIn           = 31,
            VarTypeOut          = 32,
            VarTypeInout        = 33,
            VarTypeWire         = 34,
            VarTypeState        = 35,
            LoopVariablePrefix  = 36,
            SignalWidthPrefix   = 37,
            StatementDelimiter  = 38,
            ParameterDelimiter  = 39,
            OpenRBracket        = 40,
            CloseRBracket       = 41,
            OpenSBracket        = 42,
            CloseSBracket       = 43,
            KeywordModule       = 44,
            KeywordFor          = 45,
            KeywordDo           = 46,
            KeywordTo           = 47,
            KeywordStep         = 48,
            KeywordRof          = 49,
            KeywordIf           = 50,
            KeywordThen         = 51,
            KeywordElse         = 52,
            KeywordFi           = 53,
            KeywordSkip         = 54,
            BitrangeStartPrefix = 55,
            BitrangEndPrefix    = 56,
            SkipableWhitespaces = 57,
            LineComment         = 58,
            Multilinecomment    = 59,
            Ident               = 60,
            Int                 = 61
        };

        enum : std::uint8_t {
            RuleNumber                 = 0,
            RuleProgram                = 1,
            RuleModule                 = 2,
            RuleParameterList          = 3,
            RuleParameter              = 4,
            RuleSignalList             = 5,
            RuleSignalDeclaration      = 6,
            RuleStatementList          = 7,
            RuleStatement              = 8,
            RuleCallStatement          = 9,
            RuleLoopVariableDefinition = 10,
            RuleLoopStepsizeDefinition = 11,
            RuleForStatement           = 12,
            RuleIfStatement            = 13,
            RuleUnaryStatement         = 14,
            RuleAssignStatement        = 15,
            RuleSwapStatement          = 16,
            RuleSkipStatement          = 17,
            RuleSignal                 = 18,
            RuleExpression             = 19,
            RuleBinaryExpression       = 20,
            RuleUnaryExpression        = 21,
            RuleShiftExpression        = 22
        };

        explicit TSyrecParser(antlr4::TokenStream* input);

        TSyrecParser(antlr4::TokenStream* input, const antlr4::atn::ParserATNSimulatorOptions& options);
        ~TSyrecParser() override;

        [[nodiscard]] std::string                     getGrammarFileName() const override;
        [[nodiscard]] const antlr4::atn::ATN&         getATN() const override;
        [[nodiscard]] const std::vector<std::string>& getRuleNames() const override;
        [[nodiscard]] const antlr4::dfa::Vocabulary&  getVocabulary() const override;
        [[nodiscard]] antlr4::atn::SerializedATNView  getSerializedATN() const override;

        class NumberContext;
        class ProgramContext;
        class ModuleContext;
        class ParameterListContext;
        class ParameterContext;
        class SignalListContext;
        class SignalDeclarationContext;
        class StatementListContext;
        class StatementContext;
        class CallStatementContext;
        class LoopVariableDefinitionContext;
        class LoopStepsizeDefinitionContext;
        class ForStatementContext;
        class IfStatementContext;
        class UnaryStatementContext;
        class AssignStatementContext;
        class SwapStatementContext;
        class SkipStatementContext;
        class SignalContext;
        class ExpressionContext;
        class BinaryExpressionContext;
        class UnaryExpressionContext;
        class ShiftExpressionContext;

        class NumberContext: public antlr4::ParserRuleContext {
        public:
            NumberContext(ParserRuleContext* parent, size_t invokingState);

            NumberContext() = default;
            void copyFrom(NumberContext* ctx);
            using ParserRuleContext::copyFrom;

            [[nodiscard]] size_t getRuleIndex() const override;
        };

        class NumberFromSignalwidthContext: public NumberContext {
        public:
            explicit NumberFromSignalwidthContext(NumberContext* ctx);

            [[nodiscard]] antlr4::tree::TerminalNode* literalSignalWidthPrefix() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalIdent() const;
        };

        class NumberFromLoopVariableContext: public NumberContext {
        public:
            explicit NumberFromLoopVariableContext(NumberContext* ctx);

            [[nodiscard]] antlr4::tree::TerminalNode* literalLoopVariablePrefix() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalIdent() const;
        };

        class NumberFromConstantContext: public NumberContext {
        public:
            explicit NumberFromConstantContext(NumberContext* ctx);

            [[nodiscard]] antlr4::tree::TerminalNode* literalInt() const;
        };

        class NumberFromExpressionContext: public NumberContext {
        public:
            explicit NumberFromExpressionContext(NumberContext* ctx);

            NumberContext*                            lhsOperand = nullptr;
            antlr4::Token*                            op         = nullptr;
            NumberContext*                            rhsOperand = nullptr;
            [[nodiscard]] std::vector<NumberContext*> number() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpPlus() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpMinus() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpMultiply() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpDivision() const;
        };

        NumberContext* number();

        class ProgramContext: public antlr4::ParserRuleContext {
        public:
            ProgramContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] std::vector<ModuleContext*> module() const;
        };

        ProgramContext* program();

        class ModuleContext: public antlr4::ParserRuleContext {
        public:
            ModuleContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                          getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode*     literalIdent() const;
            [[nodiscard]] StatementListContext*           statementList() const;
            [[nodiscard]] ParameterListContext*           parameterList() const;
            [[nodiscard]] std::vector<SignalListContext*> signalList() const;
        };

        ModuleContext* module();

        class ParameterListContext: public antlr4::ParserRuleContext {
        public:
            ParameterListContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                         getRuleIndex() const override;
            [[nodiscard]] std::vector<ParameterContext*> parameter() const;
        };

        ParameterListContext* parameterList();

        class ParameterContext: public antlr4::ParserRuleContext {
        public:
            ParameterContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] SignalDeclarationContext*   signalDeclaration() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalVarTypeIn() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalVarTypeOut() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalVarTypeInout() const;
        };

        ParameterContext* parameter();

        class SignalListContext: public antlr4::ParserRuleContext {
        public:
            SignalListContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                                 getRuleIndex() const override;
            [[nodiscard]] std::vector<SignalDeclarationContext*> signalDeclaration() const;
            [[nodiscard]] antlr4::tree::TerminalNode*            literalVarTypeWire() const;
            [[nodiscard]] antlr4::tree::TerminalNode*            literalVarTypeState() const;
        };

        SignalListContext* signalList();

        class SignalDeclarationContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token*              intToken = nullptr;
            std::vector<antlr4::Token*> dimensionTokens;
            antlr4::Token*              signalWidthToken = nullptr;
            SignalDeclarationContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode* literalIdent() const;
        };

        SignalDeclarationContext* signalDeclaration();

        class StatementListContext: public antlr4::ParserRuleContext {
        public:
            StatementContext*              statementContext = nullptr;
            std::vector<StatementContext*> stmts;
            StatementListContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t getRuleIndex() const override;
        };

        StatementListContext* statementList();

        class StatementContext: public antlr4::ParserRuleContext {
        public:
            StatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                  getRuleIndex() const override;
            [[nodiscard]] CallStatementContext*   callStatement() const;
            [[nodiscard]] ForStatementContext*    forStatement() const;
            [[nodiscard]] IfStatementContext*     ifStatement() const;
            [[nodiscard]] UnaryStatementContext*  unaryStatement() const;
            [[nodiscard]] AssignStatementContext* assignStatement() const;
            [[nodiscard]] SwapStatementContext*   swapStatement() const;
            [[nodiscard]] SkipStatementContext*   skipStatement() const;
        };

        StatementContext* statement();

        class CallStatementContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token*              moduleIdent = nullptr;
            antlr4::Token*              identToken  = nullptr;
            std::vector<antlr4::Token*> callerArguments;
            CallStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpCall() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpUncall() const;
        };

        CallStatementContext* callStatement();

        class LoopVariableDefinitionContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token* variableIdent = nullptr;
            LoopVariableDefinitionContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode* literalIdent() const;
        };

        LoopVariableDefinitionContext* loopVariableDefinition();

        class LoopStepsizeDefinitionContext: public antlr4::ParserRuleContext {
        public:
            LoopStepsizeDefinitionContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] NumberContext*              number() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpMinus() const;
        };

        LoopStepsizeDefinitionContext* loopStepsizeDefinition();

        class ForStatementContext: public antlr4::ParserRuleContext {
        public:
            NumberContext* startValue = nullptr;
            NumberContext* endValue   = nullptr;
            ForStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                         getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode*    literalKeywordFor() const;
            [[nodiscard]] StatementListContext*          statementList() const;
            [[nodiscard]] LoopStepsizeDefinitionContext* loopStepsizeDefinition() const;
            [[nodiscard]] LoopVariableDefinitionContext* loopVariableDefinition() const;
        };

        ForStatementContext* forStatement();

        class IfStatementContext: public antlr4::ParserRuleContext {
        public:
            ExpressionContext*    guardCondition          = nullptr;
            StatementListContext* trueBranchStmts         = nullptr;
            StatementListContext* falseBranchStmts        = nullptr;
            ExpressionContext*    matchingGuardExpression = nullptr;
            IfStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t getRuleIndex() const override;
        };

        IfStatementContext* ifStatement();

        class UnaryStatementContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token* unaryOp = nullptr;
            UnaryStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] SignalContext*              signal() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpInvertAssign() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpIncrementAssign() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpDecrementAssign() const;
        };

        UnaryStatementContext* unaryStatement();

        class AssignStatementContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token* assignmentOp = nullptr;
            AssignStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] SignalContext*              signal() const;
            [[nodiscard]] ExpressionContext*          expression() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpAddAssign() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpSubAssign() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpXorAssign() const;
        };

        AssignStatementContext* assignStatement();

        class SwapStatementContext: public antlr4::ParserRuleContext {
        public:
            SignalContext* lhsOperand = nullptr;
            SignalContext* rhsOperand = nullptr;
            SwapStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t getRuleIndex() const override;
        };

        SwapStatementContext* swapStatement();

        class SkipStatementContext: public antlr4::ParserRuleContext {
        public:
            SkipStatementContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t getRuleIndex() const override;
        };

        SkipStatementContext* skipStatement();

        class SignalContext: public antlr4::ParserRuleContext {
        public:
            ExpressionContext*              expressionContext = nullptr;
            std::vector<ExpressionContext*> accessedDimensions;
            NumberContext*                  bitStart    = nullptr;
            NumberContext*                  bitRangeEnd = nullptr;
            SignalContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode* literalIdent() const;
        };

        SignalContext* signal();

        class ExpressionContext: public antlr4::ParserRuleContext {
        public:
            ExpressionContext(ParserRuleContext* parent, size_t invokingState);

            ExpressionContext() = default;
            void copyFrom(ExpressionContext* ctx);
            using ParserRuleContext::copyFrom;

            [[nodiscard]] size_t getRuleIndex() const override;
        };

        class ExpressionFromSignalContext: public ExpressionContext {
        public:
            explicit ExpressionFromSignalContext(ExpressionContext* ctx);

            [[nodiscard]] SignalContext* signal() const;
        };

        class ExpressionFromBinaryExpressionContext: public ExpressionContext {
        public:
            explicit ExpressionFromBinaryExpressionContext(ExpressionContext* ctx);

            [[nodiscard]] BinaryExpressionContext* binaryExpression() const;
        };

        class ExpressionFromNumberContext: public ExpressionContext {
        public:
            explicit ExpressionFromNumberContext(ExpressionContext* ctx);

            [[nodiscard]] NumberContext* number() const;
        };

        class ExpressionFromUnaryExpressionContext: public ExpressionContext {
        public:
            explicit ExpressionFromUnaryExpressionContext(ExpressionContext* ctx);

            [[nodiscard]] UnaryExpressionContext* unaryExpression() const;
        };

        class ExpressionFromShiftExpressionContext: public ExpressionContext {
        public:
            explicit ExpressionFromShiftExpressionContext(ExpressionContext* ctx);

            [[nodiscard]] ShiftExpressionContext* shiftExpression() const;
        };

        ExpressionContext* expression();

        class BinaryExpressionContext: public antlr4::ParserRuleContext {
        public:
            ExpressionContext* lhsOperand      = nullptr;
            antlr4::Token*     binaryOperation = nullptr;
            ExpressionContext* rhsOperand      = nullptr;
            BinaryExpressionContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpPlus() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpMinus() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpMultiply() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpDivision() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpModulo() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpUpperBitMultiply() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpLogicalAnd() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpLogicalOr() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpBitwiseAnd() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpBitwiseOr() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpBitwiseXor() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpLessThan() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpGreaterThan() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpEqual() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpNotEqual() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpLessOrEqual() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpGreaterOrEqual() const;
        };

        BinaryExpressionContext* binaryExpression();

        class UnaryExpressionContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token* unaryOperation = nullptr;
            UnaryExpressionContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] ExpressionContext*          expression() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpLogicalNegation() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpBitwiseNegation() const;
        };

        UnaryExpressionContext* unaryExpression();

        class ShiftExpressionContext: public antlr4::ParserRuleContext {
        public:
            antlr4::Token* shiftOperation = nullptr;
            ShiftExpressionContext(ParserRuleContext* parent, size_t invokingState);
            [[nodiscard]] size_t                      getRuleIndex() const override;
            [[nodiscard]] ExpressionContext*          expression() const;
            [[nodiscard]] NumberContext*              number() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpRightShift() const;
            [[nodiscard]] antlr4::tree::TerminalNode* literalOpLeftShift() const;
        };

        ShiftExpressionContext* shiftExpression();

        // By default the static state used to implement the parser is lazily initialized during the first
        // call to the constructor. You can call this function if you wish to initialize the static state
        // ahead of time.
        static void initialize();
    };
} // namespace syrec_parser
