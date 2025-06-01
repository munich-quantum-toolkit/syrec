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

TEST_F(SyrecParserErrorTestsFixture, OmittingForStatementKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 31), "extraneous input '$' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    recordSyntaxError(Message::Position(1, 34), "no viable alternative at input 'i ='");
    performTestExecution("module main(in a(4), out b(4)) $i = 0 to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidForStatementKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 31), "mismatched input 'do' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a(4), out b(4)) do $i = 0 to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingLoopVariableIdentPrefixCausesError) {
    recordSyntaxError(Message::Position(1, 35), "mismatched input 'i' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for i = 0 to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingLoopVariableInitialValueInitializationEqualSignCausesError) {
    recordSyntaxError(Message::Position(1, 38), "no viable alternative at input '$i 0'");
    performTestExecution("module main(in a(4), out b(4)) for $i 0 to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericLoopVariableInitialValueInitializationCausesError) {
    recordSyntaxError(Message::Position(1, 40), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for $i = b to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableInInitialValueInitializationUsingSingleOperandCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOfLoopVariableNotUsableInItsInitialValueDeclaration>(Message::Position(1, 40), "$i");
    performTestExecution("module main(in a(4), out b(4)) for $i = $i to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableInInitialValueInitializationUsingExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOfLoopVariableNotUsableInItsInitialValueDeclaration>(Message::Position(1, 78), "$i");
    performTestExecution("module main(in a(4), out b(4)) for $j = 0 to 2 step 1 do for $i = ((2 - $j) * $i) to 3 do b.$i ^= 1 rof rof");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingToKeywordInLoopVariableDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 42), "missing 'to' at '3'");
    performTestExecution("module main(in a(4), out b(4)) for $i = 0 3 do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingToKeywordInForLoopIterationNumbersCausesError) {
    recordSyntaxError(Message::Position(1, 37), "no viable alternative at input '0 3'");
    performTestExecution("module main(in a(4), out b(4)) for 0 3 do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericLoopIterationNumberStartValueInLoopVariableInitializationCausesError) {
    recordSyntaxError(Message::Position(1, 38), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for $i=b to 3 do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericLoopIterationNumberStartValueCausesError) {
    recordSyntaxError(Message::Position(1, 35), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for b to 3 do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericLoopIterationNumberEndValueCausesError) {
    recordSyntaxError(Message::Position(1, 40), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for 2 to b do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericLoopVariableEndValueCausesError) {
    recordSyntaxError(Message::Position(1, 45), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for $i = 0 to b do b.$i ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidForLoopStepSizeKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 37), "no viable alternative at input '3 incr'");
    performTestExecution("module main(in a(4), out b(4)) for 3 incr 1 do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, NonNumericLoopVariableStepsizeValueCausesError) {
    recordSyntaxError(Message::Position(1, 42), "mismatched input 'b' expecting {'-', '$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for 3 step b do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsingNonMinusSymbolAfterStepKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 44), "extraneous input '-' expecting 'do'");
    performTestExecution("module main(in a(4), out b(4)) for 3 step 1 - do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsingMultipleMinusSymbolsAfterStepkeywordCausesError) {
    recordSyntaxError(Message::Position(1, 43), "extraneous input '-' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for 3 step --1 do b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingDoKeywordAfterLoopHeaderDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 44), "missing 'do' at 'b'");
    performTestExecution("module main(in a(4), out b(4)) for 3 step 1 b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidDoKeywordAfterLoopHeaderDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 45), "mismatched input 'loop' expecting {'do', 'step'}");
    performTestExecution("module main(in a(4), out b(4)) for $i=0 to 3 loop b.0 ^= 1 rof");
}

TEST_F(SyrecParserErrorTestsFixture, EmptyLoopBodyCausesError) {
    recordSyntaxError(Message::Position(1, 48), "mismatched input 'rof' expecting {'++=', '--=', '~=', 'call', 'uncall', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a(4), out b(4)) for $i=0 to 3 do rof");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateDefinitionOfLoopVariableInNestedLoopCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 53), "$i");
    performTestExecution("module main(in a(4), out b(4)) for $i=0 to 3 do for $i=1 to 3 do skip rof rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableOutsideOfLoopBodyCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 67), "$i");
    performTestExecution("module main(inout a(4)) for $i = 0 to 3 step 1 do ++= a rof; --= a.$i");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableOfNestedLoopOutsideOfLoopBodyInParentLoopCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 86), "$i");
    performTestExecution("module main(inout a(4)) for 3 step 1 do for $i = 0 to (#a - 1) do ++= a.$i rof; --= a.$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNonNumericExpressionInLoopVariableInitializationCausesError) {
    recordSyntaxError(Message::Position(1, 41), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for $i = (b - 2) to 3 step 1 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNonNumericExpressionInLoopVariableEndValueDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 46), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for $i = 0 to (b - 2) step 1 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNonNumericExpressionInLoopVariableStepsizeDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 43), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for 3 step (b - 2) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNonNumericExpressionInForLoopIterationNumberStartValueDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 36), "no viable alternative at input '(b'");
    performTestExecution("module main(in a(4), out b(4)) for (b - 2) step 1 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNonNumericExpressionInForLoopIterationNumberEndValueDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 41), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for 0 to (b - 2) step 1 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNonNumericExpressionInForLoopIterationNumberStepsizeDefinitionCausesError) {
    recordSyntaxError(Message::Position(1, 48), "mismatched input 'b' expecting {'$', '#', '(', INT}");
    performTestExecution("module main(in a(4), out b(4)) for 0 to 3 step (b - 2) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingRofKeywordAfterLoopBodyCausesError) {
    recordSyntaxError(Message::Position(1, 52), "missing 'rof' at '<EOF>'");
    performTestExecution("module main(in a(4), out b(4)) for 3 step 1 do  ++=b");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidRofKeywordAfterLoopBodyCausesError) {
    recordSyntaxError(Message::Position(1, 51), "no viable alternative at input 'done;'");
    recordSyntaxError(Message::Position(1, 57), "missing 'rof' at '<EOF>'");
    performTestExecution("module main(in a(4), out b(4)) for 3 step 1 do done; ++=b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithConstantValuedStartAndEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 14), 1, 3, 0);
    performTestExecution("module main() for 1 to 3 step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithEvaluatedValueOfZeroWithConstantValuedStartAndEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 2, 0, 0);
    performTestExecution("module main(in b(2)) for 2 to 0 step ((2 - 1) * 0) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithImplicitStartAndConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 14), 0, 2, 0);
    performTestExecution("module main() for 2 step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithEvaluatedValueOfZeroWithImplicitStartAndEvaluatedValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, 3, 0);
    performTestExecution("module main(in b(2)) for (#b + 1) step (#b * 0) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithEvaluatedConstantValueForStartAndConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, 2, 0);
    performTestExecution("module main(in b(2)) for (#b - 2) to 2 step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithEvaluatedConstantValuesForStartAndEndValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, 1, 0);
    performTestExecution("module main(in b(2)) for (#b - 2) to (2 - 1) step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithConstantValueForStartAndEvaluatedConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 1, 3, 0);
    performTestExecution("module main(in b(2)) for 1 to (#b + 1) step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithEvaluatedConstantValueOfZeroWithEvaluatedConstantValueForStartAndConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, 2, 0);
    performTestExecution("module main(in b(2)) for (#b - 2) to 2 step (2 - #b) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithEvaluatedConstantValueOfZeroWithEvaluatedConstantValuesForStartAndEndValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, 1, 0);
    performTestExecution("module main(in b(2)) for (#b - 2) to (2 - 1) step ((#b - 1) - 1) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithEvaluatedConstantValueOfZeroWithConstantValueForStartAndEvaluatedConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 1, 3, 0);
    performTestExecution("module main(in b(2)) for 1 to (#b + 1) step ((#b + 10) * 0) do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithTruncatedNegativeEvaluatedConstantValueForStartAndConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), UINT_MAX, 1, 0);
    performTestExecution("module main(in b(2)) for $i = (#b - 3) to 1 step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithConstantValueOfStartAndTruncatedNegativeEvaluatedConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, UINT_MAX, 0);
    performTestExecution("module main(in b(2)) for $i = 0 to (#b - 3) step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithTruncatedNegativeEvaluatedConstantValueForStartAndEndValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), UINT_MAX - 1, UINT_MAX, 0);
    performTestExecution("module main(in b(2)) for $i = (#b - 4) to (#b - 3) step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithTruncatedOverflowingEvaluatedConstantValueForStartAndConstantValueOfEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 1, 2, 0);
    performTestExecution("module main(in b(2)) for $i = ((#b - 3) + 2) to 2 step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfStepsizeWithConstantValueOfZeroWithImplicitConstantValueForStartAndTruncatedOverflowingEvaluatedConstantValueForEndCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::InfiniteLoopDetected>(Message::Position(1, 21), 0, 1, 0);
    performTestExecution("module main(in b(2)) for ((#b - 3) + 2) step 0 do skip rof");
}

TEST_F(SyrecParserErrorTestsFixture, IndexOutOfRangeErrorInDimensionAccessOfVariableAccessDetectedByPropagationOfLoopVariableInLoopPerformingSingleIterationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 64), 2, 0, 2);
    performTestExecution("module main(inout a[2](4), in b(2)) for $i = 2 to 4 step 3 do a[$i].0:1 += b rof");
}

TEST_F(SyrecParserErrorTestsFixture, IndexOutOfRangeErrorInBitrangeAccessOfVariableAccessDetectedByPropagationOfLoopVariableInLoopPerformingSingleIterationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 69), 4, 4);
    performTestExecution("module main(inout a[2](4), in b(5)) for $i = 2 to 4 step 3 do a[1].0:($i + 2) += b rof");
}

TEST_F(SyrecParserErrorTestsFixture, SemanticErrorInNestedLoopTriggeredByPropagationOfLoopVariableInLoopPerformingSingleIterationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 76));
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 96), 4, 4);
    performTestExecution("module main(inout a[2](4), in b(5)) for $i = 2 to 4 step 3 do for 0 to (4 / ($i - 2)) do a[1].0:($i + 2) += b rof rof");
}

TEST_F(SyrecParserErrorTestsFixture, StartAndEndValueOfLoopEvaluatingToSameIntegerConstantWillCauseLoopToPerformNoIterationAndWillNotCausePropagationOfLoopVariableValueButStillReportsSemanticErrors) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 55), 5, 4);
    performTestExecution("module main(inout a(4)) for $i = 4 to 4 step 5 do a.$i:5 += 2 rof");
}

TEST_F(SyrecParserErrorTestsFixture, StartAndEndValueOfLoopEvaluatingToSameIntegerConstantWillCauseLoopWithoutLoopVariableDefinitionToPerformNoIterationAndWillNotCausePropagationOfLoopVariableValueButStillReportsSemanticErrors) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 49), 5, 4);
    performTestExecution("module main(inout a(4)) for 4 to 4 step 5 do a.0:5 += 2 rof");
}
