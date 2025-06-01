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

TEST_F(SyrecParserErrorTestsFixture, UsageOfVariableOfTypeInOnLhsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 31), "a");
    performTestExecution("module main(in a(4), out b(4)) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfVariableOfTypeStateOnLhsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 33), "a");
    performTestExecution("module main(out b(4)) state a(4) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfVariableOfTypeInOnRhsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 39), "b");
    performTestExecution("module main(inout a(4), in b(4)) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfVariableOfTypeStateOnRhsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 39), "b");
    performTestExecution("module main(out a(4)) state b(4) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfReadonlyVariablesForBothOperandsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 32), "a");
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 38), "b");
    performTestExecution("module main(in a(4)) state b(4) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableOnLhsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 31), "c");
    performTestExecution("module main(in a(4), out b(4)) c <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableOnRhsOfSwapStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 40), "c");
    performTestExecution("module main(inout a(4), out b(4)) a <=> c");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfExpressionOnLhsOfSwapStatementCausesError) {
    recordSyntaxError(Message::Position(1, 34), "extraneous input '(' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    recordSyntaxError(Message::Position(1, 37), "no viable alternative at input 'a -'");
    performTestExecution("module main(inout a(4), out b(4)) (a - 2) <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfExpressionOnRhsOfSwapStatementCausesError) {
    recordSyntaxError(Message::Position(1, 40), "extraneous input '(' expecting IDENT");
    recordSyntaxError(Message::Position(1, 43), "extraneous input '-' expecting {<EOF>, 'module'}");
    performTestExecution("module main(inout a(4), out b(4)) a <=> (b - 2)");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsDefiningNoBitAccessAndRhsDefiningBitAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 40), 4, 1);
    performTestExecution("module main(inout a(4), out b(4)) a <=> b.1");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsDefiningNoBitAccessAndRhsDefiningBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 40), 4, 2);
    performTestExecution("module main(inout a(4), out b(4)) a <=> b.0:1");
}

TEST_F(SyrecParserErrorTestsFixture, MissmatchInSwapOperationBitwidthsWithLhsDefiningNoBitAccessAndRhsDefiningBitrangeWithStartLargerThanEndAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 40), 4, 2);
    performTestExecution("module main(inout a(4), out b(4)) a <=> b.1:0");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitAccessAndRhsDefiningNoBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 42), 1, 4);
    performTestExecution("module main(inout a(4), out b(4)) a.1 <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitAccessAndRhsDefiningBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 42), 1, 2);
    performTestExecution("module main(inout a(4), out b(4)) a.1 <=> b.2:3");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitAccessAndRhsDefiningBitrangeWithStartLargerThanEndAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 42), 1, 2);
    performTestExecution("module main(inout a(4), out b(4)) a.1 <=> b.3:2");
}

TEST_F(SyrecParserErrorTestsFixture, MissmatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessAndRhsDefiningNoBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 4);
    performTestExecution("module main(inout a(4), out b(4)) a.1:2 <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessWithStartLargerThanEndAndRhsDefiningNoBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 4);
    performTestExecution("module main(inout a(4), out b(4)) a.2:1 <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessAndRhsDefiningBitAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 1);
    performTestExecution("module main(inout a(4), out b(4)) a.2:3 <=> b.1");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessWithStartLargerThanEndAndRhsDefiningBitAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 1);
    performTestExecution("module main(inout a(4), out b(4)) a.3:2 <=> b.1");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessAndRhsDefiningBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 3);
    performTestExecution("module main(inout a(4), out b(4)) a.0:1 <=> b.1:3");
}

TEST_F(SyrecParserErrorTestsFixture, MissmatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessAndRhsDefiningBitrangeWithStartLargerThanEndAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 3);
    performTestExecution("module main(inout a(4), out b(4)) a.0:1 <=> b.3:1");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessWithStartLargerThanEndAndRhsDefiningBitrangeWithStartLargerThanEndAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 44), 2, 3);
    performTestExecution("module main(inout a(4), out b(4)) a.1:0 <=> b.3:1");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitAccessWithUnknownValueForIndexAndRhsBeingBitrangeAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 62), 1, 3);
    performTestExecution("module main(inout a(4), out b(4)) for $i = 0 to 3 do a.$i <=> b.3:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, MismatchInSwapOperationBitwidthsWithLhsBeingBitAccessWithUnknownValueForIndexAndRhsBeingFullBitwidthAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 62), 1, 4);
    performTestExecution("module main(inout a(4), out b(4)) for $i = 0 to 3 do a.$i <=> b rof");
}

TEST_F(SyrecParserErrorTestsFixture, MissmatchInSwapOperationBitwidthsWithLhsBeingBitrangeAccessAndRhsBeingBitAccessWithUnknownValueForIndexAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 63), 3, 1);
    performTestExecution("module main(inout a(4), out b(4)) for $i = 0 to 3 do a.3:1 <=> b.$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, MissmatchInSwapOperationBitwidthsWithLhsBeingFullBitwidthAccessAndRhsBeingBitAccessWithUnknownValueForIndexAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 59), 4, 1);
    performTestExecution("module main(inout a(4), out b(4)) for $i = 0 to 3 do a <=> b.$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, Non1DSignalInEvaluatedVariableAccessOfLhsSwapOperandCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 37));
    performTestExecution("module main(inout a[2](4), out b(4)) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, Non1DSignalInEvaluatedVariableAccessOfRhsSwapOperandCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 43));
    performTestExecution("module main(inout a(4), out b[2](4)) a <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfConstantOperandAsLhsSwapOperandCausesError) {
    recordSyntaxError(Message::Position(1, 22), "mismatched input '2' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(out b(4)) 2 <=> b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfConstantOperandAsRhsSwapOperandCausesError) {
    recordSyntaxError(Message::Position(1, 28), "mismatched input '2' expecting IDENT");
    performTestExecution("module main(out b(4)) b <=> 2");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidSwapOperationCausesError) {
    recordSyntaxError(Message::Position(1, 36), "no viable alternative at input 'a ='");
    performTestExecution("module main(inout a(4), out b(4)) a => b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableAsLhsOperandOfSwapOperationCausesError) {
    recordSyntaxError(Message::Position(1, 41), "extraneous input '$' expecting {'++=', '--=', '~=', 'call', 'uncall', 'for', 'if', 'skip', IDENT}");
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 42), "i");
    performTestExecution("module main(out b(4)) for $i = 0 to 3 do $i <=> b rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableAsRhsOperandOfSwapOperationCausesError) {
    recordSyntaxError(Message::Position(1, 47), "extraneous input '$' expecting IDENT");
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 48), "i");
    performTestExecution("module main(out b(4)) for $i = 0 to 3 do b <=> $i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) a[0] <=> b[a[0]]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndexValue) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) a[0] <=> b[a[0].1]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthUsingImplicitDimensionAccessOfLhsOfSwapStatementInDimensionAccessOfVariableAccesOfRhsUsingImplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 44), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) a <=> b[a]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthUsingImplicitDimensionAccessOfLhsOfSwapStatementInDimensionAccessOfVariableAccesOfRhsUsingExplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 44), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) a <=> b[a[0]]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthUsingExplicitDimensionAccessOfLhsOfSwapStatementInDimensionAccessOfVariableAccesOfRhsUsingImplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) a[0] <=> b[a]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeUsingConstantIndices) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) a[0] <=> b[a[0].1:2]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantValueStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 66), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0] <=> b[a[0].1:($i + 1)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantValueEndIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 66), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0] <=> b[a[0].($i + 1):1] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 49), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) a[0].2 <=> b[a[0]]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndexValueAndBitIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 49), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) a[0].2 <=> b[a[0].2]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesWithBitEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 49), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) a[0].2 <=> b[a[0].0:3]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesWithBitIndexEqualToBitrangeStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 49), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) a[0].2 <=> b[a[0].2:3]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesWithBitIndexEqualToBitrangeEndIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 49), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) a[0].3 <=> b[a[0].2:3]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) a[0].1:3 <=> b[a]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndexEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) a[0].1:3 <=> b[a.2]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndexEqualToBitrangeStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) a[0].1:3 <=> b[a.1]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndexEqualToBitrangeEndIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) a[0].1:3 <=> b[a.3]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitrangeEnclosedByRhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(2)) a[0].2:3 <=> b[a.1:4]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndRhsBitrangeEnclosedByLhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) a[0].1:4 <=> b[a.2:3]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitrangeOverlapping) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) a[0].1:4 <=> b[a.0:3]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitrangesBeingEqual) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 51), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) a[0].1:4 <=> b[a.4:1]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantStartIndexEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 70), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do a[0].1:4 <=> b[a[0].2:($i + 1)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantStartIndexEqualToBitrangeBounds) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 70), generateVariableAccessOverlappingIndicesDataContainer({0}, 4).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do a[0].4:1 <=> b[a[0].4:($i + 1)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantEndIndexEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 70), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do a[0].1:4 <=> b[a[0].($i + 1):2] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantEndIndexEqualToBitrangeBounds) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 70), generateVariableAccessOverlappingIndicesDataContainer({0}, 4).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do a[0].1:4 <=> b[a[0].($i + 1):4] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].2:$i <=> b[a[0]] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].2:$i <=> b[a[0].2] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitEnclosedByRhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].2:$i <=> b[a[0].0:3] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitEqualToStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].3:$i <=> b[a[0].0:3] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantStartIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].3:$i <=> b[a[0].3:(1 - $i)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantEndIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].3:$i <=> b[a[0].(1 - $i):3] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].$i:2 <=> b[a[0]] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitWithConstantIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].$i:2 <=> b[a[0].2] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitEnclosedByRhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].$i:2 <=> b[a[0].0:3] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantIndicesAndLhsBitEqualToStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].$i:3 <=> b[a[0].0:3] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantStartIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].$i:3 <=> b[a[0].3:(1 - $i)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfLhsOfSwapStatementInDimensionAccessOfVariableAccessOfRhsAccessingBitrangeWithConstantEndIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 71), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do a[0].$i:3 <=> b[a[0].(1 - $i):3] rof");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) a[0][2][1] <=> b[a[0][2][1]]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingBitaccessWithConstantValueAndLhsAccessingFullbitwidthWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) a[0][2][1] <=> b[a[0][2][1].1]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingBitrangeAccessWithConstantIndicesAndLhsAccessingFullbitwidthWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 62), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) a[0][2][1] <=> b[a[0][2][1].1:2]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingBitrangeAccessWithStartIndexHavingConstantValueAndLhsAccessingFullbitwidthWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 88), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) for $i = 0 to 3 step 2 do a[0][2][1] <=> b[a[0][2][1].1:$i] rof");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingBitAccessWithConstantValueAndLhsAccessingBitUsingConstantValueAndAccessedBitsOverlappingWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 64), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(1)) a[0][2][1].0 <=> b[a[0][2][1].0]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingBitrangeAccessWithConstantValuesAndLhsAccessingBitUsingConstantValueAndAccessedBitEnclosedInBitrangeWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 64), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(1)) a[0][2][1].1 <=> b[a[0][2][1].2:0]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingBitAccessWithConstantValuesAndLhsAccessingBitrangeUsingConstantIndicesWithAccessedBitEnclosedByBitrangeWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 66), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(3)) a[0][2][1].2:0 <=> b[a[0][2][1].1]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnRhsOfSwapStatementUsingFullbitwidthAndLhsAccessingBitrangeUsingConstantIndicesWithAcessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 66), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(3)) a[0][2][1].2:0 <=> b[a[0][2][1]]");
}

TEST_F(SyrecParserErrorTestsFixture, accessignLhsOperandInBinaryExpressionInDimensionAccessOfRhsOperand) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 56), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) a[0].0:2 <=> b[(2 + a[0].1:2)]");
}

TEST_F(SyrecParserErrorTestsFixture, accessingLhsOperandInShiftExpressionInDimensionAccessOfRhsOperand) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 52), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) a[0].0:2 <=> b[(a[0].1:3 << 2)]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) b[a[0]] <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndexValue) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) b[a[0].1] <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthUsingImplicitDimensionAccessOfRhsOfSwapStatementInDimensionAccessOfVariableAccesOfLhsUsingImplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) b[a] <=> a");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthUsingImplicitDimensionAccessOfRhsOfSwapStatementInDimensionAccessOfVariableAccesOfLhsUsingExplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) b[a[0]] <=> a");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthUsingExplicitDimensionAccessOfRhsOfSwapStatementInDimensionAccessOfVariableAccesOfLhsUsingImplicitDimensionAccess) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) b[a] <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeUsingConstantIndices) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) b[a[0].1:2] <=> a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantValueStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].1:($i + 1)] <=> a[0] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToFullSignalBitwidthOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantValueEndIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].($i + 1):1] <=> a[0] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) b[a[0]] <=> a[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndexValueAndBitIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) b[a[0].2] <=> a[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesWithBitEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) b[a[0].0:3] <=> a[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesWithBitIndexEqualToBitrangeStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) b[a[0].2:3] <=> a[0].2");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitUsingConstantIndexValueOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesWithBitIndexEqualToBitrangeEndIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(1)) b[a[0].2:3] <=> a[0].3");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) b[a] <=> a[0].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndexEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) b[a.2] <=> a[0].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndexEqualToBitrangeStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) b[a.1] <=> a[0].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndexEqualToBitrangeEndIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) b[a.3] <=> a[0].1:3");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitrangeEnclosedByRhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(2)) b[a.1:4] <=> a[0].2:3");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndRhsBitrangeEnclosedByLhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) b[a.2:3] <=> a[0].1:4");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitrangeOverlapping) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) b[a.0:3] <=> a[0].1:4");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitrangesBeingEqual) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 38), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) b[a.4:1] <=> a[0].1:4");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantStartIndexEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do b[a[0].2:($i + 1)] <=> a[0].1:4 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantStartIndexEqualToBitrangeBounds) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 4).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do b[a[0].4:($i + 1)] <=> a[0].4:1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantEndIndexEnclosedByBitrange) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do b[a[0].($i + 1):2] <=> a[0].1:4 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantIndicesOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantEndIndexEqualToBitrangeBounds) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 4).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(6), inout b(4)) for $i = 0 to 3 do b[a[0].($i + 1):4] <=> a[0].1:4 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0]] <=> a[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].2] <=> a[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitEnclosedByRhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].0:3] <=> a[0].2:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitEqualToStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].0:3] <=> a[0].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantStartIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].3:(1 - $i)] <=> a[0].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForStartIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantEndIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].(1 - $i):3] <=> a[0].3:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingFullbitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0]] <=> a[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitWithConstantIndicesMatching) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].2] <=> a[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitEnclosedByRhsOne) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].0:3] <=> a[0].$i:2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantIndicesAndLhsBitEqualToStartIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].0:3] <=> a[0].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantStartIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].3:(1 - $i)] <=> a[0].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OverlappingAccessOnAssignedToBitrangeUsingConstantValueForEndIndexOfRhsOfSwapStatementInDimensionAccessOfVariableAccessOfLhsAccessingBitrangeWithConstantEndIndexEqualToLhsIndex) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 57), generateVariableAccessOverlappingIndicesDataContainer({0}, 3).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(4)) for $i = 3 to 0 do b[a[0].(1 - $i):3] <=> a[0].$i:3 rof");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) b[a[0][2][1]] <=> a[0][2][1]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingBitaccessWithConstantValueAndRhsAccessingFullbitwidthWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) b[a[0][2][1].1] <=> a[0][2][1]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingBitrangeAccessWithConstantIndicesAndRhsAccessingFullbitwidthWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) b[a[0][2][1].1:2] <=> a[0][2][1]");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingBitrangeAccessWithStartIndexHavingConstantValueAndRhsAccessingFullbitwidthWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 73), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(6)) for $i = 0 to 3 step 2 do b[a[0][2][1].1:$i] <=> a[0][2][1] rof");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingBitAccessWithConstantValueAndRhsAccessingBitUsingConstantValueAndAccessedBitsOverlappingWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 0).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(1)) b[a[0][2][1].0] <=> a[0][2][1].0");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingBitrangeAccessWithConstantValuesAndRhsAccessingBitUsingConstantValueAndAccessedBitEnclosedInBitrangeWithAccessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(1)) b[a[0][2][1].2:0] <=> a[0][2][1].1");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingBitAccessWithConstantValuesAndRhsAccessingBitrangeUsingConstantIndicesWithAccessedBitEnclosedByBitrangeWithAcessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(3)) b[a[0][2][1].1] <=> a[0][2][1].2:0");
}

TEST_F(SyrecParserErrorTestsFixture, accessOnValueOfDimensionsOfAssignedToNDimensionalSignalOnLhsOfSwapStatementUsingFullbitwidthAndRhsAccessingBitrangeUsingConstantIndicesWithAcessOnSameValuesOfDimensions) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 47), generateVariableAccessOverlappingIndicesDataContainer({0, 2, 1}, 2).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a[2][4][3](6), inout b(3)) b[a[0][2][1]] <=> a[0][2][1].2:0");
}

TEST_F(SyrecParserErrorTestsFixture, accessignRhsOperandInBinaryExpressionInDimensionAccessOfLhsOperand) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 43), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) b[(2 + a[0].1:2)] <=> a[0].0:2");
}

TEST_F(SyrecParserErrorTestsFixture, accessingRhsOperandInShiftExpressionInDimensionAccessOfLhsOperand) {
    buildAndRecordExpectedSemanticError<SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts>(
            Message::Position(1, 39), generateVariableAccessOverlappingIndicesDataContainer({0}, 1).stringifyOverlappingIndicesInformation());
    performTestExecution("module main(inout a(4), inout b(3)) b[(a[0].1:3 << 2)] <=> a[0].0:2");
}
