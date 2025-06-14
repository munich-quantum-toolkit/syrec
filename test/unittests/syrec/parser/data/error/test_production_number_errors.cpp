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

#include <climits>
#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableInNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 43), "c");
    performTestExecution("module main(out a(4), out b[2](4)) ++= a[(#c - 2)]");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfInvalidOperationInNumericExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 45), "mismatched input '<<' expecting {'+', '-', '*', '/'}");
    buildAndRecordExpectedSemanticError<SemanticError::UnhandledOperationFromGrammarInParser>(Message::Position(1, 45), "<<");
    performTestExecution("module main(out a(8), out b[2](2)) ++= a.(#b << 2)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingOpeningBracketOfNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 28), 4, 0, 1);
    recordSyntaxError(Message::Position(1, 31), "mismatched input '-' expecting ']'");
    performTestExecution("module main(out a(4)) ++= a[#a - 4)]");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfInvalidOpeningBracketInNumericExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 31), "token recognition error at: '{'");
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 32), 4, 0, 4);
    recordSyntaxError(Message::Position(1, 35), "mismatched input '-' expecting ']'");
    performTestExecution("module main(out a[4](4)) ++= a[{#a - 2)]");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingClosingBracketOfNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 41), 6, 4);
    recordSyntaxError(Message::Position(1, 48), "missing ')' at '<EOF>'");
    performTestExecution("module main(out a(4), out b[2](4)) ++= a.(#a + 2");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfInvalidClosingBracketInNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 41), 6, 4);
    recordSyntaxError(Message::Position(1, 48), "mismatched input ']' expecting ')'");
    performTestExecution("module main(out a(4), out b[2](4)) ++= a.(#a + 2]");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 46));
    performTestExecution("module main(out a(4), out b[2](4)) ++= a.(2 / 0)");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInEvaluatedNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 46));
    performTestExecution("module main(out a(4), out b[2](4)) ++= a.(2 / (#a - 4))");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredLoopVariableInNumericExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 69), "$j");
    performTestExecution("module main(out a(4), out b[2](4)) for $i = 0 to 3 step 1 do ++= a.(($j + 2) / 2) rof");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingBitwidthOfUnknownVariableCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 31), "b");
    performTestExecution("module main(out a[1](4)) a += #b");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingBitwidthOfLoopVariableCausesError) {
    recordSyntaxError(Message::Position(1, 50), "mismatched input '(' expecting IDENT");
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 52), "i");
    performTestExecution("module main(out a[1](4)) for $i = 0 to 3 do a += #($i) rof");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingBitwidthOfConstantCausesError) {
    recordSyntaxError(Message::Position(1, 31), "mismatched input '5' expecting IDENT");
    performTestExecution("module main(out a[1](4)) a += #5");
}

TEST_F(SyrecParserErrorTestsFixture, UserDefinedIntegerConstantDefinedInProgramTooLargeToBeConvertedToUnsignedIntegerCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(Message::Position(1, 27), "34359738368", UINT_MAX);
    performTestExecution("module main(out a(4)) a += 34359738368");
}
