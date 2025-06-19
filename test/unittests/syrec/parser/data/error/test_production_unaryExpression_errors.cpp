/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/custom_error_messages.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "test_syrec_parser_errors_base.hpp"

#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, UsageOfUnknownUnaryOperationInUnaryExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 36), "extraneous input '^' expecting {'!', '~', '$', '#', '(', IDENT, INT}");
    buildAndRecordExpectedSemanticError<SemanticError::IfGuardExpressionMismatch>(Message::Position(1, 36));
    performTestExecution("module main(inout a(1), in b(1)) if ^(a && b) then ++= a else --= a fi !(a && b)");
}

TEST_F(SyrecParserErrorTestsFixture, MissingOpeningBracketInUnaryExpressionWhenUsingLogicalNegationCausesError) {
    recordSyntaxError(Message::Position(1, 39), "mismatched input '&&' expecting 'then'");
    performTestExecution("module main(inout a(1), in b(1)) if !a && b) then ++= a else --= a fi !(a && b)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidOpeningBracketInUnaryExpressionWhenUsingLogicalNegationCausesError) {
    recordSyntaxError(Message::Position(1, 37), "extraneous input '[' expecting {'!', '~', '$', '#', '(', IDENT, INT}");
    recordSyntaxError(Message::Position(1, 40), "mismatched input '&&' expecting 'then'");
    performTestExecution("module main(inout a(1), in b(1)) if ![a && b)) then ++= a else --= a fi !(a && b)");
}

TEST_F(SyrecParserErrorTestsFixture, MissingClosingBracketInUnaryExpressionWhenUsingLogicalNegationCausesError) {
    recordSyntaxError(Message::Position(1, 45), "missing ')' at 'then'");
    performTestExecution("module main(inout a(1), in b(1)) if !(a && b then ++= a else --= a fi !(a && b)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidClosingBracketInUnaryExpressionWhenUsingLogicalNegationCausesError) {
    recordSyntaxError(Message::Position(1, 44), "mismatched input ']' expecting ')'");
    performTestExecution("module main(inout a(1), in b(1)) if !(a && b] then ++= a else --= a fi !(a && b)");
}

TEST_F(SyrecParserErrorTestsFixture, MissingOpeningBracketInUnaryExpressionWhenUsingBitwiseNegationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 36), 1U, 2U);
    recordSyntaxError(Message::Position(1, 45), "mismatched input '>' expecting 'then'");
    performTestExecution("module main(inout a(2), in b(2)) if (~a + b) > 1) then ++= a else --= a fi (~(a + b) > 1)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidOpeningBracketInUnaryExpressionWhenUsingBitwiseNegationCausesError) {
    recordSyntaxError(Message::Position(1, 38), "no viable alternative at input '(~['");
    performTestExecution("module main(inout a(2), in b(2)) if (~[a + b) > 1) then ++= a else --= a fi (~(a + b) > 1)");
}

TEST_F(SyrecParserErrorTestsFixture, MissingClosingBracketInUnaryExpressionWhenUsingBitwiseNegationCausesError) {
    recordSyntaxError(Message::Position(1, 45), "no viable alternative at input '(~(a + b >'");
    performTestExecution("module main(inout a(2), in b(2)) if (~(a + b > 1) then ++= a else --= a fi (~(a + b) > 1)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidClosingBracketInUnaryExpressionWhenUsingBitwiseNegationCausesError) {
    recordSyntaxError(Message::Position(1, 44), "no viable alternative at input '(~(a + b]'");
    performTestExecution("module main(inout a(2), in b(2)) if (~(a + b] > 1) then ++= a else --= a fi (~(a + b) > 1)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableInUnaryExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 30), "b");
    performTestExecution("module main(inout a(4)) a += ~b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNon1DVariableInUnaryExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 43));
    performTestExecution("module main(inout a(4), in b[2](4)) a += ~(b + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchBetweenLogicalAndBitwiseNegationInUnaryExpressionUsedAsGuardExpressionOfIfStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IfGuardExpressionMismatch>(Message::Position(1, 36));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 78), 1U, 2U);
    performTestExecution("module main(inout a(2), in b(2)) if (~(a + b) > 1) then ++= a else --= a fi (!(a + b) > 1)");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchBetweenBitwiseAndLogicalNegationInUnaryExpressionUsedAsGuardExpressionOfIfStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IfGuardExpressionMismatch>(Message::Position(1, 36));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 38), 1U, 2U);
    performTestExecution("module main(inout a(2), in b(2)) if (!(a + b) > 1) then ++= a else --= a fi (~(a + b) > 1)");
}

TEST_F(SyrecParserErrorTestsFixture, ExpressionWithBitwidthLargerThanOneNotAllowedAsOperandInUnaryExpressionUsingLogicalNegationOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 38), 4U, 1U);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 39), 1U, 4U);
    performTestExecution("module main(inout a(4), in b(4)) a += !b");
}

TEST_F(SyrecParserErrorTestsFixture, NestedExpressionWithBitwidthLargerThanOneNotAllowedAsOperandInUnaryExpressionUsingLogicalNegationOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 47), 4U, 1U);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 48), 1U, 2U);
    performTestExecution("module main(inout a(4), in b(4), in c(4)) a += !(b.0:1 + c.3:2)");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOnMoreThanOneBitInUnaryExpressionUsingLogicalNegationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 38), 2U, 1U);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 39), 1U, 2U);
    performTestExecution("module main(inout a(2), in b(4)) a += !b.0:1");
}
