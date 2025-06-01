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

#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, OmittingLocalModuleParameterTypeInDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 15), "no viable alternative at input 'a('");
    performTestExecution("module main() a(16) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidLocalModuleParameterTypeInDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 18), "no viable alternative at input 'int a'");
    performTestExecution("module main() int a(16) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingLocalVariableDelimiterInDeclarationsSharingSameVariableType) {
    recordSyntaxError(Message::Position(1, 26), "no viable alternative at input 'b('");
    performTestExecution("module main() wire a(16) b(16) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableIdentifierInLocalModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 19), "missing IDENT at '('");
    performTestExecution("module main() wire (16) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableIdentifierInLocalModuleParameterDeclarationSharingSameVariableTypeCausesError) {
    recordSyntaxError(Message::Position(1, 26), "missing IDENT at '['");
    performTestExecution("module main() wire a(16), [2](4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentifierSharingSameVariableTypeInLocalModuleParameterDeclarationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 32), "a");
    performTestExecution("module main() wire a(16), b(8), a(4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentifierSharingSameVariableTypeInSeparateDeclarationInLocalModuleParameterDeclarationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 36), "a");
    performTestExecution("module main() wire a(16), b(8) wire a(4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentiferUsingDifferentVariableTypeInLocalModuleParameterDeclarationCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 37), "a");
    performTestExecution("module main() wire a(16) state b(8), a(4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentifierBetweenModuleParameterAndLocalVariableInSeparateLocalVariableDeclaration) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 38), "a");
    performTestExecution("module main(in a(16)) wire b(8) state a(4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentiferBetweenModuleParameterAndLocalVariableDeclarationWithLatterDefiningMultipleVariablesOfSameType) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 44), "a");
    performTestExecution("module main(in a(16)) wire c(4) state b(8), a(4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidSymbolInLocalModuleVariableIdentifierDefinedInSeparateDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 20), "extraneous input '-' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main() wire a-2(16) state b(4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidSymbolInLocalModuleVariableIdentifierDefinedInDeclarationSharingVariableTypesCausesError) {
    recordSyntaxError(Message::Position(1, 27), "extraneous input '#' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main() wire a(16), b#2 skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableValuesForDimensionOpeningBracketInLocalModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 21), "extraneous input ']' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main() wire a2](4) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidVariableValuesForDimensionOpeningBracketInLocalModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 20), "token recognition error at: '{'");
    recordSyntaxError(Message::Position(1, 21), "extraneous input '2' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main() wire a{2] skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableValuesForDimensionClosingBracketInLocalModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 23), "missing ']' at 'skip'");
    performTestExecution("module main() wire a[2 skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidVariableValuesForDimensionClosingBracketInLocalModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 22), "mismatched input ')' expecting ']'");
    performTestExecution("module main() wire a[2) skip");
}

TEST_F(SyrecParserErrorTestsFixture, NoneNumericValueForNumberOfValuesForDimensionInLocalModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 31), "mismatched input '#' expecting INT");
    performTestExecution("module main() wire b(4) wire a[#b) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingValueForNumberOfValuesForDimensionInLocalModuleVariableDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 21), "missing INT at ']'");
    performTestExecution("module main() wire a[] skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingOpeningBracketOfBitwidthInLocalModuleVariableDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 22), "extraneous input ')' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main() wire a16) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidOpeningBracketOfBitwidthInLocalModuleVariableDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 22), "token recognition error at: '}'");
    performTestExecution("module main() wire a16} skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingClosingBracketOfBitwidthInLocalModuleVariableDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 24), "missing ')' at 'skip'");
    performTestExecution("module main() wire a(16 skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidClosingBracketOfBitwidthInModuleVariableDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 23), "token recognition error at: '}'");
    recordSyntaxError(Message::Position(1, 25), "missing ')' at 'skip'");
    performTestExecution("module main() wire a(16} skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingLocalModuleVariableBitwidthValueWhenBracketsWereDefinedCausesError) {
    recordSyntaxError(Message::Position(1, 21), "missing INT at ')'");
    performTestExecution("module main() wire a() skip");
}

TEST_F(SyrecParserErrorTestsFixture, NoneNumericLocalModuleVariableBitwidthCausesError) {
    recordSyntaxError(Message::Position(1, 21), "mismatched input 'test' expecting INT");
    performTestExecution("module main() wire a(test) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredLocalVariableBitwidthLongerThanMaximumSupportedValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 41), 45, 32);
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 52), 33, 32);
    performTestExecution("module main(inout b(2), out c(4)) wire x(45) wire t(33) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredLocalVariableBitwidthInSignalListLongerThanMaximumSupportedValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 50), 45, 32);
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 67), 33, 32);
    performTestExecution("module main(inout b(2), out c(4)) wire test(2), x(45), y(4) wire t(33), z skip");
}

TEST_F(SyrecParserErrorTestsFixture, LocalVariableBitwidthTakenFromUserConfigurationLongerThanMaximumSupportedValueCausesError) {
    constexpr unsigned int defaultSignalBitwidth           = 33;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 39), defaultSignalBitwidth, 32);
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 46), defaultSignalBitwidth, 32);
    performTestExecution("module main(inout b(2), out c(4)) wire x wire t skip", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, LocalVariableBitwidthTakenFromUserConfigurationInSignalListLongerThanMaximumSupportedValueCausesError) {
    constexpr unsigned int defaultSignalBitwidth           = 33;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 48), defaultSignalBitwidth, 32);
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 61), defaultSignalBitwidth, 32);
    performTestExecution("module main(inout b(2), out c(4)) wire test(2), x, y(4) wire t, z(1) skip", userProvidedParserConfiguration);
}
