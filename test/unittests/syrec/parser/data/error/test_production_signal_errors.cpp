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
#include "core/syrec/program.hpp"
#include "test_syrec_parser_errors_base.hpp"

#include <climits>
#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableIdentifierInVariableAccessCausesError) {
    recordSyntaxError(Message::Position(1, 26), "missing IDENT at '.'");
    performTestExecution("module main(out a(4)) ++= .0");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingOpeningBracketForAccessOnDimensionOfVariableCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 29), "a1");
    recordSyntaxError(Message::Position(1, 31), "extraneous input ']' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out a[2](4)) ++= a1].0");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidOpeningBracketForAccessOnDimensionOfVariableCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 29));
    recordSyntaxError(Message::Position(1, 30), "extraneous input '(' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out a[2](4)) ++= a(1].0");
}

TEST_F(SyrecParserErrorTestsFixture, IndexForAccessedValueOfDimensionOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 31), 2, 0, 2);
    performTestExecution("module main(out a[2](4)) ++= a[2].0");
}

TEST_F(SyrecParserErrorTestsFixture, IndexForAccessedValueOfDimensionInNonConstantExpressionOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 31), 4, 0, 2);
    performTestExecution("module main(out a[2](4)) ++= a[#a].0");
}

TEST_F(SyrecParserErrorTestsFixture, NumberOfAccessedDimensionsOfVariableOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::TooManyDimensionsAccessed>(Message::Position(1, 29), 2, 1);
    performTestExecution("module main(out a[2](4)) ++= a[0][1].0");
}

TEST_F(SyrecParserErrorTestsFixture, NumberOfAccessedDimensionsOfVariableImplicitlyDeclaredAs1DVariableWithSingleValueOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::TooManyDimensionsAccessed>(Message::Position(1, 26), 2, 1);
    performTestExecution("module main(out a(4)) ++= a[0][1].0");
}

TEST_F(SyrecParserErrorTestsFixture, IndexForAccessedValueDimensionsOfVariableImplicitlyDeclaredAs1DVariableWithSingleValueOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 28), 2, 0, 1);
    performTestExecution("module main(out a(4)) ++= a[2].0");
}

TEST_F(SyrecParserErrorTestsFixture, None1DSizeOfVariableAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 29));
    performTestExecution("module main(out a[2](4)) ++= a.0");
}

TEST_F(SyrecParserErrorTestsFixture, None1DAccessOnExpressionForValueOfDimensionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 44));
    performTestExecution("module main(out a[2](4), out b[2](2)) ++= a[b].0");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingClosingBracketForAccessOnDimensionOfVariableCausesError) {
    recordSyntaxError(Message::Position(1, 32), "missing ']' at '.'");
    performTestExecution("module main(out a[2](4)) ++= a[0.0");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidClosingBracketForAccessOnDimensionOfVariableCausesError) {
    recordSyntaxError(Message::Position(1, 32), "token recognition error at: '}'");
    recordSyntaxError(Message::Position(1, 33), "missing ']' at '.'");
    performTestExecution("module main(out a[2](4)) ++= a[0}.0");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingBitrangeStartSymbolCausesError) {
    recordSyntaxError(Message::Position(1, 33), "extraneous input '0' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out a[2](4)) ++= a[0]0");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidBitrangeStartSymbolCausesError) {
    recordSyntaxError(Message::Position(1, 33), "extraneous input ':' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out a[2](4)) ++= a[0]:0");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidBitrangEndSymbolCausesError) {
    recordSyntaxError(Message::Position(1, 35), "extraneous input '.' expecting {<EOF>, 'module'}");
    performTestExecution("module main(out a[2](4)) ++= a[0].0.0");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInDynamicBitrangeStartValueExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 33));
    performTestExecution("module main(out a(4)) ++= a.(2 / (#a - 4))");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInDynamicBitrangeEndValueExpressionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 35));
    performTestExecution("module main(out a(4)) ++= a.0:(2 / (#a - 4))");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroInDynamicExpressionForAccessValueOfDimensionCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 36));
    performTestExecution("module main(out a[2](4)) ++= a[(2 / (#a - 4))]");
}

TEST_F(SyrecParserErrorTestsFixture, BitrangeStartValueIsConstantAndOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), 5, 4);
    performTestExecution("module main(out a(4)) ++= a.5");
}

TEST_F(SyrecParserErrorTestsFixture, BitrangeEndValueIsConstantAndOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 30), 5, 4);
    performTestExecution("module main(out a(4)) ++= a.0:5");
}

TEST_F(SyrecParserErrorTestsFixture, BitrangeStartAndEndValueIsConstantAndOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), 7, 4);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 30), 5, 4);
    performTestExecution("module main(out a(4)) ++= a.7:5");
}

TEST_F(SyrecParserErrorTestsFixture, BitrangeStartValueIsDynamicExpressionAndOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), 5, 4);
    performTestExecution("module main(out a(4)) ++= a.(#a + 1):3");
}

TEST_F(SyrecParserErrorTestsFixture, BitrangeEndValueIsDynamicExpressionAndOutOfRangeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 30), 5, 4);
    performTestExecution("module main(out a(4)) ++= a.0:(#a + 1)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingExplicitDimensionAccessOn1DSignalWithMultipleValuesCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 29));
    performTestExecution("module main(out a[2](4)) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingTooFewDimensionsOfANDSignalInVariableAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::TooFewDimensionsAccessed>(Message::Position(1, 32), 1, 2);
    performTestExecution("module main(out a[2][3](4)) ++= a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingTooManyDimensionsOfANDSignalInVariableAccessCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::TooManyDimensionsAccessed>(Message::Position(1, 32), 3, 2);
    performTestExecution("module main(out a[2][3](4)) ++= a[0][1][2]");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitOfVariableWithBitwidthTakenFromUserConfiguration) {
    constexpr unsigned int defaultSignalBitwidth           = 3;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 25), 5, defaultSignalBitwidth);
    performTestExecution("module main(out a) ++= a.5", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeStartBitInBitrangeWithKnownBoundsOfVariableWithBitwidthTakenFromUserConfiguration) {
    constexpr unsigned int defaultSignalBitwidth           = 3;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 25), 5, defaultSignalBitwidth);
    performTestExecution("module main(out a) ++= a.5:2", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeEndBitInBitrangeWithKnownBoundsOfVariableWithBitwidthTakenFromUserConfiguration) {
    constexpr unsigned int defaultSignalBitwidth           = 3;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 27), 5, defaultSignalBitwidth);
    performTestExecution("module main(out a) ++= a.2:5", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeStartAndEndBitInBitrangeWithKnownBoundsOfVariableWithBitwidthTakenFromUserConfiguration) {
    constexpr unsigned int defaultSignalBitwidth           = 3;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 25), 7, defaultSignalBitwidth);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 27), 5, defaultSignalBitwidth);
    performTestExecution("module main(out a) ++= a.7:5", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeStartBitInBitrangeWithUnknownEndBoundOfVariableWithBitwidthTakenFromUserConfiguration) {
    constexpr unsigned int defaultSignalBitwidth           = 3;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 44), 5, defaultSignalBitwidth);
    performTestExecution("module main(out a) for $i = 0 to 3 do ++= a.5:$i rof", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeEndBitInBitrangeWithUnknownStartBoundOfVariableWithBitwidthTakenFromUserConfiguration) {
    constexpr unsigned int defaultSignalBitwidth           = 3;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 47), 5, defaultSignalBitwidth);
    performTestExecution("module main(out a) for $i = 0 to 3 do ++= a.$i:5 rof", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeStartBitInBitrangeWithUnknownEndBoundOfVariableWithExplicitlyDeclaredBitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 47), 5, 3);
    performTestExecution("module main(out a(3)) for $i = 0 to 3 do ++= a.5:$i rof");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeEndBitInBitrangeWithUnknownStartBoundOfVariableWithExplicitlyDeclaredBitwidth) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 50), 5, 3);
    performTestExecution("module main(out a(3)) for $i = 0 to 3 do ++= a.$i:5 rof");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitWithEvaluationOfValueLeadingToTruncationOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), UINT_MAX, 3);
    performTestExecution("module main(out a(3)) ++= a.(#a - 4)");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitWithEvaluationOfValueLeadingToOverflowOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), 4, 3);
    performTestExecution("module main(out a(3)) ++= a.((#a - 4) + 5)");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitrangeWithEvaluationOfStartValueLeadingToTruncationOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), UINT_MAX, 3);
    performTestExecution("module main(out a(3)) ++= a.(#a - 4):2");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitrangeWithEvaluationOfStartValueLeadingToOverflowOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 28), 4, 3);
    performTestExecution("module main(out a(3)) ++= a.((#a - 4) + 5):2");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitrangeWithEvaluationOfEndValueLeadingToTruncationOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 30), UINT_MAX, 3);
    performTestExecution("module main(out a(3)) ++= a.1:(#a - 4)");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeBitrangeWithEvaluationOfEndValueLeadingToOverflowOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedBitOutOfRange>(Message::Position(1, 30), 4, 3);
    performTestExecution("module main(out a(3)) ++= a.1:((#a - 4) + 5)");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeValueOfDimensionWithEvaluationOfEndValueLeadingToTruncationOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 28), UINT_MAX, 0, 1);
    performTestExecution("module main(out a(3)) ++= a[(#a - 4)]");
}

TEST_F(SyrecParserErrorTestsFixture, AccessingOutOfRangeValueOfDimensionWithEvaluationOfEndValueLeadingToOverflowOfValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 28), 4, 0, 1);
    performTestExecution("module main(out a(3)) ++= a[((#a - 4) + 5)]");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthRestrictionForUnknownAccessedBitInDimensionAccessIsOnlyLocalToCurrentExpressionAndCausesErrorOnOperandBitwidthMismatch) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 82), 1, 2);
    performTestExecution("module main(inout a(4), out b(4), in c(2)) for $i = 0 to 3 do ++= a[((b.$i + 2) - c)] rof");
}

TEST_F(SyrecParserErrorTestsFixture, OperandBitwidthRestrictionOnlyLocalToCurrentExpressionIsResetAfterDimensionWasProcessedWithNewRestrictionInNextDimensionNotBlocked) {
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionBitwidthMismatches>(Message::Position(1, 106), 2, 4);
    performTestExecution("module main(inout a[2][4](4), out b(4), in c(2)) for $i = 0 to 3 do ++= a[((b.$i + 2) - c.0)][((c << 1) + b[0])] rof");
}
