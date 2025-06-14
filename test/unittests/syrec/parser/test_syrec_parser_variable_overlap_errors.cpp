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
#include "utils/test_syrec_parser_errors_base.hpp"

#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAndNoBitrangeAccessAndRhsOperandWithoutDimensionAndNoBitRangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) b += b");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAndNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) b[0] += b");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAndNoBitrangeAccessAndRhsOperandWithDimensionButNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) b += b[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAndNoBitrangeAccessAndRhsOperandWithDimensionButNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) b[0] += b[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 30), 4, 1);
    performTestExecution("module main(out b[1](4)) b += b.0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 30), 4, 3);
    performTestExecution("module main(out b[1](4)) b += b.3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitrangeAccessWithStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b += b.$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitrangeAccessWithEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b += b.1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 30), 4, 1);
    performTestExecution("module main(out b[1](4)) b += b[0].0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 30), 4, 3);
    performTestExecution("module main(out b[1](4)) b += b[0].3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitrangeAccessWithStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b += b[0].$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithoutDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitrangeAccessWithEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b += b[0].1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 33), 4, 1);
    performTestExecution("module main(out b[1](4)) b[0] += b.0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 33), 4, 3);
    performTestExecution("module main(out b[1](4)) b[0] += b.3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitrangeAccessWithStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 59),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b[0] += b.$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithoutDimensionAccessAndBitrangeAccessWithEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 59),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b[0] += b.1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 33), 4, 1);
    performTestExecution("module main(out b[1](4)) b[0] += b[0].0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 33), 4, 3);
    performTestExecution("module main(out b[1](4)) b[0] += b[0].3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitrangeAccessWithStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 59),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b[0] += b[0].$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithLhsOperandWithDimensionAccessAndNoBitrangeAccessAndRhsOperandWithDimensionAccessAndBitrangeAccessWithEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 59),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[1](4)) for $i = 0 to 3 step 1 do b[0] += b[0].1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 30), 4, 1);
    performTestExecution("module main(out b(4)) b[0] += b.0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b.1 += b.1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndOverlappingBitrangeAccessFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 3);
    performTestExecution("module main(out b(4)) b.1 += b.0:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndOverlappingBitrangeAccessFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 3);
    performTestExecution("module main(out b(4)) b.2 += b.3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 2);
    performTestExecution("module main(out b(4)) b.2 += b.3:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 2);
    performTestExecution("module main(out b(4)) b.2 += b.2:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithStartEqualToBitAndEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 55),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2 += b.2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithEndEqualToBitAndStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 55),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2 += b.$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 4);
    performTestExecution("module main(out b(4)) b.2 += b[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b.2 += b[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndOverlappingBitrangeAccessFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 4);
    performTestExecution("module main(out b(4)) b.2 += b[0].0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndOverlappingBitrangeAccessFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 3);
    performTestExecution("module main(out b(4)) b.2 += b[0].3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 2);
    performTestExecution("module main(out b(4)) b.2 += b[0].2:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 29),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 29), 1, 2);
    performTestExecution("module main(out b(4)) b.2 += b[0].1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithStartEqualToBitAndEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 55),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2 += b[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithEndEqualToBitAndStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 55),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2 += b[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 4);
    performTestExecution("module main(out b(4)) b[0].1 += b");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b[0].1 += b.1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndOverlappingBitrangeAccessFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 3);
    performTestExecution("module main(out b(4)) b[0].1 += b.0:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndOverlappingBitrangeAccessFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 4);
    performTestExecution("module main(out b(4)) b[0].1 += b.3:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b[0].1 += b.1:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 3);
    performTestExecution("module main(out b(4)) b[0].3 += b.1:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithStartEqualToBitAndEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b[0].1 += b.1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithoutDimensionAndBitrangeAccessWithEndEqualToBitAndStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b[0].1 += b.$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 4);
    performTestExecution("module main(out b(4)) b[0].3 += b[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b[0].3 += b[0].3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndOverlappingBitrangeAccessFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 4);
    performTestExecution("module main(out b(4)) b[0].2 += b[0].0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndOverlappingBitrangeAccessFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 3);
    performTestExecution("module main(out b(4)) b[0].2 += b[0].3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 2);
    performTestExecution("module main(out b(4)) b[0].2 += b[0].2:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 32),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 32), 1, 2);
    performTestExecution("module main(out b(4)) b[0].2 += b[0].1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithStartEqualToBitAndEndIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b[0].2 += b[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithDimensionAndBitAccessAndRhsOperandWithDimensionAndBitrangeAccessWithEndEqualToBitAndStartIndexValueUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b[0].2 += b[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndNoBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 3, 4);
    performTestExecution("module main(out b(4)) b.0:2 += b");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitAccessWithBitrangeOverlappingBitFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 4, 1);
    performTestExecution("module main(out b(4)) b.0:3 += b.1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitAccessWithBitrangeOverlappingBitFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 3, 1);
    performTestExecution("module main(out b(4)) b.3:1 += b.2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessWithStartIndexValueUnknownAndRhsOperandWithoutDimensionAndBitAccessWithBitrangeEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.$i:2 += b.2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessWithEndIndexValueUnknownAndRhsOperandWithoutDimensionAndBitAccessWithBitrangeStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2:$i += b.2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithBitrangesAccessingSameIndices) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b.3:1 += b.1:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsBitrangeOverlappingFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 2, 4);
    performTestExecution("module main(out b(4)) b.2:1 += b.0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsBitrangeOverlappingFromTheLeftUpToRhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 2, 3);
    performTestExecution("module main(out b(4)) b.2:1 += b.0:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsBitrangeOverlappingFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 2, 4);
    performTestExecution("module main(out b(4)) b.2:1 += b.3:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsBitrangeOverlappingFromTheRightUpToRhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) b.2:1 += b.3:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsBitrangeEnclosingRhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 4, 2);
    performTestExecution("module main(out b(4)) b.3:0 += b.1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsBitrangeBeingEnclosedByRhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 31),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 31), 2, 4);
    performTestExecution("module main(out b(4)) b.1:2 += b.3:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsStartIndexUnknownAndEndIndexEqualToRhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.$i:2 += b.2:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsStartIndexUnknownAndEndIndexEqualToRhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.$i:2 += b.1:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsStartIndexUnknownAndEndIndexEnclosedByRhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.$i:2 += b.0:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsEndIndexUnknownAndStartIndexEqualToRhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:$i += b.1:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsEndIndexUnknownAndStartIndexEqualToRhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:$i += b.2:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsEndIndexUnknownAndStartIndexEnclosedByRhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:$i += b.2:0 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithRhsStartIndexUnknownAndEndIndexEqualToLhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:2 += b.$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithRhsStartIndexUnknownAndEndIndexEqualToLhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:2 += b.$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithRhsStartIndexUnknownAndEndIndexEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:3 += b.$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithRhsEndIndexUnknownAndStartIndexEqualToLhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2:3 += b.2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithRhsEndIndexUnknownAndStartIndexEqualToLhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:2 += b.2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithRhsEndIndexUnknownAndStartIndexEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.1:3 += b.2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsStartIndexUnknownAndRhsStartIndexUnknownWithEndIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.$i:2 += b.$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsStartIndexUnknownAndRhsEndIndexUnknownWithKnownIndicesValuesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.$i:2 += b.2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsEndIndexUnknownAndRhsEndIndexUnknownWithStartIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2:$i += b.2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithSingleValueWithoutDimensionAndBitrangeAccessAndRhsOperandWithoutDimensionAndBitRangeAccessWithLhsEndIndexUnknownAndRhsStartIndexUnknownWithKnownIndicesValuesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 58),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b(4)) for $i = 0 to 3 step 1 do b.2:$i += b.$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithoutBitrangeAccessAndRhsWithoutBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) b[0] += b[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithoutBitrangeAccessAndRhsWithBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 33), 4, 1);
    performTestExecution("module main(out b[2](4)) b[0] += b[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithoutBitrangeAccessAndRhsWithBitrangeAccessWithKnownIndicesValues) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 33), 4, 2);
    performTestExecution("module main(out b[2](4)) b[0] += b[0].2:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithoutBitrangeAccessAndRhsWithBitrangeAccessWithStartIndexUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 59),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 step 1 do b[0] += b[0].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithoutBitrangeAccessAndRhsWithBitrangeAccessWithEndIndexUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 59),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 step 1 do b[0] += b[0].1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithoutBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 4);
    performTestExecution("module main(out b[2](4)) b[1].2 += b[1]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) b[1].2 += b[1].2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesOverlappingBitFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 4);
    performTestExecution("module main(out b[2](4)) b[1].2 += b[1].0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesOverlappingBitFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 3);
    performTestExecution("module main(out b[2](4)) b[1].2 += b[1].3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesWithStartIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 2);
    performTestExecution("module main(out b[2](4)) b[1].2 += b[1].3:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesWithEndIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 2);
    performTestExecution("module main(out b[2](4)) b[1].2 += b[1].1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 61),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 step 1 do b[1].2 += b[1].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithLhsWithBitAccessAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 61),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 step 1 do b[1].2 += b[1].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithoutBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 3).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 2, 4);
    performTestExecution("module main(out b[2](4)) b[1].3:2 += b[1]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeOverlappingBitFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 3, 1);
    performTestExecution("module main(out b[2](4)) b[1].0:2 += b[1].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithoutBitrangeAccessOverlappingBitFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 4, 1);
    performTestExecution("module main(out b[2](4)) b[1].3:0 += b[1].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 3, 1);
    performTestExecution("module main(out b[2](4)) b[1].3:1 += b[1].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 3).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 3, 1);
    performTestExecution("module main(out b[2](4)) b[1].1:3 += b[1].3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitAccessWithBitrangeEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[1].$i:3 += b[1].3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitAccessWithBitrangeStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({1U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[1].3:$i += b[1].3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeEnclosingBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 3, 1);
    performTestExecution("module main(out b[2](4)) b[0].0:2 += b[0].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessMatchingExactly) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) b[0].0:2 += b[0].2:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithLhsBitrangeEnclosingRhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 4, 2);
    performTestExecution("module main(out b[2](4)) b[0].0:3 += b[0].1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithRhsBitrangeEnclosingLhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 2, 4);
    performTestExecution("module main(out b[2](4)) b[0].2:1 += b[0].0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithLhsOverlappingRhsFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) b[0].2:0 += b[0].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithLhsOverlappingRhsFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 3, 2);
    performTestExecution("module main(out b[2](4)) b[0].1:3 += b[0].1:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:3 += b[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:3 += b[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndEqualByLhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:3 += b[0].$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndEqualByLhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:3 += b[0].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartEqualByLhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:3 += b[0].1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartEqualByLhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:3 += b[0].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsEndEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].$i:2 += b[0].0:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsEndEqualToRhsStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].$i:2 += b[0].2:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsEndEqualToRhsEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].$i:2 += b[0].0:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsStartEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:$i += b[0].0:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsStartEqualToRhsStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:$i += b[0].1:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsStartEqualToRhsEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].1:$i += b[0].3:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessStartIndexUnknownWithEndIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].$i:1 += b[0].$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessEndIndexUnknownWithStartIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].3:$i += b[0].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessEndIndexUnknownWithKnownIndicesValuesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].$i:2 += b[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentFor1DSignalWithMultipleValuesWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessStartIndexUnknownWithKnownIndicesValuesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2](4)) for $i = 0 to 3 do b[0].2:$i += b[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithoutBitrangeAccessAndRhsWithoutBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 39),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) b[0][1] += b[0][1]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithoutBitrangeAccessAndRhsWithBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 39),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 39), 4, 1);
    performTestExecution("module main(out b[2][3](4)) b[0][1] += b[0][1].2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithoutBitrangeAccessAndRhsWithBitrangeAccessWithKnownIndicesValues) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 39),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 39), 4, 2);
    performTestExecution("module main(out b[2][3](4)) b[0][1] += b[0][1].2:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithoutBitrangeAccessAndRhsWithBitrangeAccessWithStartIndexUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 65),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 step 1 do b[0][1] += b[0][1].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithoutBitrangeAccessAndRhsWithBitrangeAccessWithEndIndexUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 65),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 step 1 do b[0][1] += b[0][1].1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithBitAccessAndRhsWithoutBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 41),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 41), 1, 4);
    performTestExecution("module main(out b[2][3](4)) b[1][2].2 += b[1][2]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithBitAccessAndRhsWithBitAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 41),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) b[1][2].2 += b[1][2].2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesOverlappingBitFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 41),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 41), 1, 4);
    performTestExecution("module main(out b[2][3](4)) b[1][2].2 += b[1][2].0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesOverlappingBitFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 41),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 41), 1, 3);
    performTestExecution("module main(out b[2][3](4)) b[1][2].2 += b[1][2].3:1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesWithStartIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 41),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 41), 1, 2);
    performTestExecution("module main(out b[2][3](4)) b[1][2].2 += b[1][2].3:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithLhsWithBitAccessAndRhsWithBitrangeAccessWithKnownIndicesValuesWithEndIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 41),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 41), 1, 2);
    performTestExecution("module main(out b[2][3](4)) b[1][2].2 += b[1][2].1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithBitAccessAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 67),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 step 1 do b[1][2].2 += b[1][2].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithLhsWithBitAccessAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartIndexEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 67),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 step 1 do b[1][2].2 += b[1][2].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithKnownValuesAndRhsWithoutBitrangeAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 3).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 2, 4);
    performTestExecution("module main(out b[2][3](4)) b[1][2].3:2 += b[1][2]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeOverlappingBitFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 3, 1);
    performTestExecution("module main(out b[2][3](4)) b[1][2].0:2 += b[1][2].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithoutBitrangeAccessOverlappingBitFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 4, 1);
    performTestExecution("module main(out b[2][3](4)) b[1][2].3:0 += b[1][2].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 3, 1);
    performTestExecution("module main(out b[2][3](4)) b[1][2].3:1 += b[1][2].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 3).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 3, 1);
    performTestExecution("module main(out b[2][3](4)) b[1][2].1:3 += b[1][2].3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitAccessWithBitrangeEndEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[1][2].$i:3 += b[1][2].3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitAccessWithBitrangeStartEqualToBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[1][2].3:$i += b[1][2].3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitAccessWithBitrangeEnclosingBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 3, 1);
    performTestExecution("module main(out b[2][3](4)) b[0][1].0:2 += b[0][1].1");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessMatchingExactly) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) b[0][1].0:2 += b[0][1].2:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithLhsBitrangeEnclosingRhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 4, 2);
    performTestExecution("module main(out b[2][3](4)) b[0][1].0:3 += b[0][1].1:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithRhsBitrangeEnclosingLhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 2, 4);
    performTestExecution("module main(out b[2][3](4)) b[0][1].2:1 += b[0][1].0:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithLhsOverlappingRhsFromTheLeft) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) b[0][1].2:0 += b[0][1].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithLhsOverlappingRhsFromTheRight) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 43), 3, 2);
    performTestExecution("module main(out b[2][3](4)) b[0][1].1:3 += b[0][1].1:0");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:3 += b[0][1].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:3 += b[0][1].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndEqualByLhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:3 += b[0][1].$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithStartIndexUnknownAndEndEqualByLhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:3 += b[0][1].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartEqualByLhsBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:3 += b[0][1].1:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithKnownValuesAndRhsWithBitrangeAccessWithEndIndexUnknownAndStartEqualByLhsBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:3 += b[0][1].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsEndEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].$i:2 += b[0][1].0:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsEndEqualToRhsStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].$i:2 += b[0][1].2:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsEndEqualToRhsEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].$i:2 += b[0][1].0:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsStartEnclosedByLhsBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:$i += b[0][1].0:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsStartEqualToRhsStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:$i += b[0][1].1:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessWithKnownValuesAndLhsStartEqualToRhsEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].1:$i += b[0][1].3:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForANDSignalWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessStartIndexUnknownWithEndIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].$i:1 += b[0][1].$i:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessEndIndexUnknownWithStartIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].3:$i += b[0][1].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithStartIndexUnknownAndRhsWithBitrangeAccessEndIndexUnknownWithKnownIndicesValuesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].$i:2 += b[0][1].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInAssignmentForNDSignalWithBitrangeAccessWithEndIndexUnknownAndRhsWithBitrangeAccessStartIndexUnknownWithKnownIndicesValuesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 63),
            generateVariableAccessOverlappingIndicesDataContainer({0, 1U}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(out b[2][3](4)) for $i = 0 to 3 do b[0][1].2:$i += b[0][1].$i:2 rof");
}

// We are only using a subset of the variable access overlap tests defined for the assignment to determine whether the same checks are performed for the operands of a SwapStatement
TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithFullbitwidthAccessWithLhsUsingImplicitDimensionAccessAndRhsUsingImplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) a <=> a");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithFullbitwidthAccessWithLhsUsingImplicitDimensionAccessAndRhsUsingExplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 30),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) a <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithFullbitwidthAccessWithLhsUsingExplicitDimensionAccessAndRhsUsingImplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) a[0] <=> a");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithFullbitwidthAccessWithLhsUsingExplicitDimensionAccessAndRhsUsingExplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 33),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) a[0] <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBit) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) a[0].2 <=> a[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitUsingConstantIndexAndRhsAccessingBitUsingBitrangeWithConstantIndicesAndAccessedBitEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 3);
    performTestExecution("module main(inout a(4)) a[0].2 <=> a[0].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitUsingConstantIndexAndRhsAccessingBitUsingBitrangeWithConstantIndicesAndAccessedBitEqualToBitrangeStart) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 2);
    performTestExecution("module main(inout a(4)) a[0].2 <=> a[0].2:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitUsingConstantIndexAndRhsAccessingBitUsingBitrangeWithConstantIndicesAndAccessedBitEqualToBitrangeEnd) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 3);
    performTestExecution("module main(inout a(4)) a[0].2 <=> a[0].0:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitUsingConstantIndexAndRhsAccessingBitUsingBitrangeWithStartIndexUnknownAndEndIndexUsingConstantValue) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 61),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) for $i = 0 to (#a - 1) do a[0].2 <=> a[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitUsingConstantIndexAndRhsAccessingBitUsingBitrangeWithStartIndexUsingConstantValueAndEndIndexUnknown) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 61),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) for $i = 0 to (#a - 1) do a[0].2 <=> a[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitUsingConstantIndexAndRhsAccessingFullBitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 35),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 35), 1, 4);
    performTestExecution("module main(inout a(4)) a[0].2 <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnOverlappingBitrangeWithLhsAccessingBitrangeUsingConstantIndicesAndRhsAccessingBitrangeUsingConstantIndices) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6)) a[0].1:3 <=> a[0].0:2");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnEnclosedBitrangeWithLhsAccessingBitrangeUsingConstantIndicesAndRhsAccessingBitrangeUsingConstantIndicesWithLhsBitrangeEnclosedByRhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 2, 4);
    performTestExecution("module main(inout a(6)) a[0].2:3 <=> a[0].1:4");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnEnclosedBitrangeWithLhsAccessingBitrangeUsingConstantIndicesAndRhsAccessingBitrangeUsingConstantIndicesWithRhsBitrangeEnclosedByLhs) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 37),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 37), 4, 2);
    performTestExecution("module main(inout a(6)) a[0].1:4 <=> a[0].2:3");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapFor1DSignalWithAccessOnSameBitWithLhsAccessingBitrangeUsingConstantValueForStartAndUnknownValueForEndIndexAndRhsAccessingBitUsingConstantIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 64),
            generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4)) for $i = 0 to (#a - 1) do a[0].2:$i <=> a[0].2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, VariableAccessOverlapInSwapForNDSignalWithAccessOnSameBitWithLhsAccessingFullBitwidthAndRhsAccessingBitrangeUsingConstantIndices) {
    buildAndRecordExpectedSemanticError<SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 42),
            generateVariableAccessOverlappingIndicesDataContainer({1U, 2U}, 2).stringifyOverlappingIndicesInformation());
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 42), 6, 2);
    performTestExecution("module main(inout a[2][4](6)) a[1][2] <=> a[1][2].2:3");
}
