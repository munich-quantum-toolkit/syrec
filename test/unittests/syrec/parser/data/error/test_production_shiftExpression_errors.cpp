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
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
#include "core/syrec/program.hpp"
#include "test_syrec_parser_errors_base.hpp"

#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableInLhsOperandOfShiftExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 42), "c");
    performTestExecution("module main(out a(4), out b[2](4)) a += ((c << 2) + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableInRhsOperandOfShiftExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 51), "c");
    performTestExecution("module main(out a(4), out b[2](4)) a += ((b[0] << #c) + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfInvalidShiftOperationCausesError) {
    recordSyntaxError(Message::Position(1, 47), "no viable alternative at input '((b[0] <=>'");
    performTestExecution("module main(out a(4), out b[2](4)) a += ((b[0] <=> 2) + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingLhsOperandOfShiftExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 42), "no viable alternative at input '((<<'");
    performTestExecution("module main(out a(4), out b[2](4)) a += ((<< #b) + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingRhsOperandOfShiftExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 56), "mismatched input ')' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(out a(4), out b[2](4)) a += (b[1] + (b[0] >>))");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericExpressionInRhsOperandOfShiftExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 51), "no viable alternative at input '((b[0] << (b'");
    performTestExecution("module main(out a(4), out b[2](4)) a += ((b[0] << (b[1] - 2) + 2))");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNon1DVariableUsedAsLhsOperandOfShiftOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 42));
    performTestExecution("module main(out a(4), out b[2](4)) a += ((b >> #a) + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingModuloOperationInLhsOfShiftExpression) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::Modulo);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 71));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 132));
    performTestExecution("module main(inout a(4), in b(2), in c[4](1)) for $i = 0 to 1 do if c[(((b.$i:1 / 7) + a.0:2) << 2)] then ++= a.0:1 else skip fi c[(((b.$i:1 / 7) + a.0:2) << 2)] rof", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingBitwiseAndOperationInLhsOfShiftExpression) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::BitwiseAnd);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 71));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 132));
    performTestExecution("module main(inout a(4), in b(2), in c[4](1)) for $i = 0 to 1 do if c[(((b.$i:1 / 8) + a.1:3) << 2)] then ++= a.0:1 else skip fi c[(((b.$i:1 / 8) + a.1:3) << 2)] rof", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, SemanticErrorInToBeShiftedExpressionReportedEvenIfShiftAmountWouldAllowSimplificationOfExpression) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 32), "b");
    performTestExecution("module main(inout a(2)) a.0 += (b.1 << 2)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthRestrictionOfShiftExpressionIsPropagatedToBinaryExpr) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 86), 2, 1);
    performTestExecution("module main(inout a(4), in b(2), in c(2)) for $i = 0 to 3 do ++= a[(((b << 1) / $i) + c[0].0)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthRestrictionOfShiftExpressionIsPropagatedToShiftExpr) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 86), 3, 2);
    performTestExecution("module main(inout a(4), in b(3), in c(2)) for $i = 0 to 3 do ++= a[(((b << 2) / $i) + (c[0].0:1 >> 1))] rof");
}
