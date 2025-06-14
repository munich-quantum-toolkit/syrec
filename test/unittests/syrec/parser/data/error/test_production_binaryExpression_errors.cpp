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

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableInBinaryExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 28), "a");
    performTestExecution("module main(out b(4)) b += (a - 2)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUnknownBinaryOperationInBinaryExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 39), "no viable alternative at input '(a <=>'");
    performTestExecution("module main(out b(4), in a(4)) b += (a <=> 2)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingOpeningBracketOfBinaryExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 38), "extraneous input '+' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out b(4), in a(4)) b += a + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidOpeningBracketOfBinaryExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 36), "extraneous input '[' expecting {'!', '~', '$', '#', '(', IDENT, INT}");
    recordSyntaxError(Message::Position(1, 39), "extraneous input '+' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out b(4), in a(4)) b += [a + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingClosingBracketOfBinaryExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 42), "missing ')' at '<EOF>'");
    performTestExecution("module main(out b(4), in a(4)) b += (a + 2");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidClosingBracketOfBinaryExpressionCausesError) {
    recordSyntaxError(Message::Position(1, 42), "mismatched input ']' expecting ')'");
    performTestExecution("module main(out b(4), in a(4)) b += (a + 2]");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNon1DVariableInEvaluatedVariableAccessInLhsOperandOfBinaryExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 41));
    performTestExecution("module main(out a(4), out b[2](4)) a += (b + 2)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNon1DVariableInEvaluatedVariableAccessInRhsOperandOfBinaryExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 45));
    performTestExecution("module main(out a(4), out b[2](4)) a += (2 - b)");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInModuloOperationWithConstantValueDivisorCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 41));
    performTestExecution("module main(out a(4), out b[2](4)) a += (2 % 0)");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInModuloOperationWithConstantExpressionDivisorCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 41));
    performTestExecution("module main(out a(4), out b[2](4)) a += (2 % (#b - 4))");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInUpperBitMultiplicationOperationWithConstantValueDivisorCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 41));
    performTestExecution("module main(out a(4), out b[2](4)) a += (2 *> 0)");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInUpperBitMultiplicationOperationWithConstantExpressionDivisorCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 41));
    performTestExecution("module main(out a(4), out b[2](4)) a += (2 *> (#b - 4))");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingModuloOperationInLhsOfBinaryExpression) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::Modulo);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 56));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 105));
    performTestExecution("module main(inout a(4), in b(2)) for $i = 0 to 1 do if ((b.$i:1 / 3) + a.0) then ++= a.0:1 else skip fi ((b.$i:1 / 3) + a.0) rof", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingModuloOperationInRhsOfBinaryExpression) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::Modulo);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 62));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 111));
    performTestExecution("module main(inout a(4), in b(2)) for $i = 0 to 1 do if (a.0 + (b.$i:1 / 3)) then ++= a.0:1 else skip fi (a.0 + (b.$i:1 / 3)) rof", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingBitwiseAndOperationInLhsOfBinaryExpression) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::BitwiseAnd);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 56));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 105));
    performTestExecution("module main(inout a(4), in b(2)) for $i = 0 to 1 do if ((b.$i:1 / 4) + a.0) then ++= a.0:1 else skip fi ((b.$i:1 / 4) + a.0) rof", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingBitwiseAndOperationInRhsOfBinaryExpression) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::BitwiseAnd);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 62));
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 111));
    performTestExecution("module main(inout a(4), in b(2)) for $i = 0 to 1 do if (a.0 + (b.$i:1 / 4)) then ++= a.0:1 else skip fi (a.0 + (b.$i:1 / 4)) rof", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, SemanticErrorInLhsOperandOfBinaryExpressionReportedEvenIfExpressionCouldBeSimplified) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 33), "b");
    performTestExecution("module main(inout a(2)) a.0 += ((b.1 + 1) * (#a - 2))");
}

TEST_F(SyrecParserErrorTestsFixture, SemanticErrorInRhsOperandOfBinaryExpressionReportedEvenIfExpressionCouldBeSimplified) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 44), "b");
    performTestExecution("module main(inout a(2)) a.0 += ((#a - 2) * (b.1 + 1))");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingFullSignalBitwidthAndRhsAccessingFullSignalBitwidthCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 52), 4, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0] += (a[1] + b)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMissmatchForOperandsOfBinaryExpressionWithLhsOperandAccessingFullSignalBitwidthAndRhsAccessingBitUsingConstantIndexCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 52), 4, 1);
    performTestExecution("module main(inout a[2](4), in b(1)) a[0] += (a[1] + b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingFullSignalBitwidthAndRhsAccessingUnknownBitCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 71), 4, 1);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 1 do a[0] += (a[1] + b.$i) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingFullSignalBitwidthAndRhsAccessingBitrangeWithConstantIndicesCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 52), 4, 2);
    performTestExecution("module main(inout a[2](4), in b(4)) a[0] += (a[1] + b.2:1)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMissmatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitUsingConstantIndexAndRhsAccessingFullSignalBitwidthCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 56), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += (a[1].0 + b)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitUsingConstantIndexAndRhsAccessingBitrangeWithConstantIndicesCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 56), 1, 3);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += (a[1].0 + a[1].3:1)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingUnknownBitAndRhsAccessingFullSignalBitwidthCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 76), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 1 do a[0].1 += (a[1].$i + b) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingUnknownBitAndRhsAccessingBitrangeWithConstantIndicesCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 76), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 1 do a[1].1 += (a[0].$i + a[1].3:2) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitrangeUsingConstantIndicesAndRhsAccessingFullSignalBitwidthWithBitwidthLargerThanAccessedBitrangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 60), 2, 3);
    performTestExecution("module main(inout a[2](4), in b(3)) a[0].1:2 += (a[1].1:2 + b)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitrangeUsingConstantIndicesAndRhsAccessingFullSignalBitwidthWithBitwidthSmallerThanAccessedBitrangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 60), 2, 1);
    performTestExecution("module main(inout a[2](4), in b(1)) a[0].1:2 += (a[1].1:2 + b)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitrangeUsingConstantIndicesAndRhsAccessingBitExplicitlyCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 60), 2, 1);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1:2 += (a[1].1:2 + b.1)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitrangeUsingConstantIndicesAndRhsAccessingUnknownBitCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 79), 2, 1);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 1 do a[0].1:2 += (a[1].1:2 + b.$i) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMissmatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitrangeUsingConstantIndicesAndRhsAccessingBitrangeWithConstantIndicesAndLargerThanLhsBitrangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 60), 2, 3);
    performTestExecution("module main(inout a[2](4), in b(4)) a[0].1:2 += (a[1].0:1 + b.3:1)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwithMismatchForOperandsOfBinaryExpressionWithLhsOperandAccessingBitrangeUsingConstantIndicesAndRhsAccessingBitrangeWithConstantIndicesAndSmallerThanLhsBitrangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 60), 3, 2);
    performTestExecution("module main(inout a[2](4), in b(4)) a[0].1:3 += (a[1].0:2 + b.3:2)");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchForOperandsOfBinaryExpressionWithBitwidthInheritedFromNestedExpressionOfLhsOperandCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 66), 2, 4);
    performTestExecution("module main(inout a[2](4), in b(2)) a[1].1:2 += ((b + a[0].1:2) + a[0])");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchForOperandsOfBinaryExpressionWithBitwidthInheritedFromNestedExpressionOfRhsOperandCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 99), 2, 4);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 3 do a[1].1:2 += ((a[0].$i:1 + (b + a[0].1:2)) + a[0]) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchForOperandsOfBinaryExpressionWithBitwidthInheritedFromNestedExpressionOfLhsOperandUsingLogicalOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 65), 1, 4);
    performTestExecution("module main(inout a[2](4), in b(2)) a[1].0 += ((b.1 || a[0].1) + a[0])");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchForOperandsOfBinaryExpressionWithBitwidthInheritedFromNestedExpressionOfRhsOperandUsingLogicalOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 98), 1, 4);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 3 do a[1].0 += ((a[0].$i:1 + (b.0 || a[0].1)) + a[0]) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchForOperandsOfBinaryExpressionWithBitwidthInheritedFromNestedExpressionOfLhsOperandUsingRelationalOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 64), 1, 4);
    performTestExecution("module main(inout a[2](4), in b(2)) a[1].0 += ((b = a[0].1:2) + a[0])");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchForOperandsOfBinaryExpressionWithBitwidthInheritedFromNestedExpressionOfRhsOperandUsingRelationalOperationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 97), 1, 4);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 3 do a[1].0 += ((a[0].$i:1 + (b = a[0].1:2)) + a[0]) rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthMismatchBetweenOperandsOfBinaryExpressionConsistingOfNestedBinaryExpressionsCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 58), 4, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0] += ((a[1] + 2) + (a[1].1:2 + b))");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndLhsOperandViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 48), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((a[1].1:2 || b.1) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndRhsOperandViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 55), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((b.1 || a[1].1:2) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndBothOperandsViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 48), 1, 2);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 53), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((b || a[1].1:2) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalAndOperationRequiresOperandsWithBitwidthOfOneAndLhsOperandViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 48), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((a[1].1:2 && b.1) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalAndOperationRequiresOperandsWithBitwidthOfOneAndRhsOperandViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 55), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((b.1 && a[1].1:2) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndLhsOperandBeingNestedBinaryExpressionViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 47), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((a[1].1:2 + b) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndLhsOperandBeingNestedShiftExpressionViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 47), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += ((a[1].1:2 << 1) || b.0)");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndRhsOperandBeingNestedBinaryExpressionViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 54), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) a[0].1 += (b.0 || (a[1].1:2 + b))");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndRhsOperandBeingNestedShiftExpressionViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 73), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 1 do a[0].1 += (b.0 || (a[1].1:2 << $i)) rof");
}

TEST_F(SyrecParserErrorTestsFixture, LogicalOperationRequiringOperandsWithBitwidthOfOneAndBothOperandBeingNestedExpressionsViolatingConstraintCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 66), 1, 2);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 84), 1, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 0 to 1 do a[0].1 += ((a[1].0:1 + b) || (a[1].1:2 << $i)) rof");
}

TEST_F(SyrecParserErrorTestsFixture, BinaryExpressionUsingLogicalOperationWithConstantOperandsCausesPropagationOfExpectedBitwidthOfNonConstantOperand) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 42), 3, 2);
    performTestExecution("module main(inout a(4), in b(2)) a.1:3 += ((#a || 2) + b)");
}

TEST_F(SyrecParserErrorTestsFixture, BinaryExpressionUsingRelationalOperationWithConstantOperandsCausesPropagationOfExpectedBitwidthOfNonConstantOperand) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 42), 3, 2);
    performTestExecution("module main(inout a(4), in b(2)) a.1:3 += (b + (#a < 2))");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthRestrictionOfBinaryExpressionIsPropagatedToShiftExpr) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 90), 1, 2);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 149), 1, 2);
    performTestExecution("module main(inout a(4), in b(2), in c[3](4)) for $i = 0 to 3 do if c[(((b.1 + 2) << $i) + a[0].0:1)].0 then skip else skip fi c[(((b.1 + 2) << $i) + a[0].0:1)].0 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthRestrictionOfBinaryExpressionIsPropagatedToBinaryExpr) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 92), 2, 1);
    performTestExecution("module main(inout a(4), in b(2), in c(2)) for $i = 0 to 3 step 2 do ++= a[(((b + 2) / $i) + c[0].0)] rof");
}
