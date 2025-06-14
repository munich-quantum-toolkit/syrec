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

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableTypeCausesError) {
    recordSyntaxError(Message::Position(1, 12), "mismatched input 'a' expecting {'in', 'out', 'inout', ')'}");
    performTestExecution("module main(a[2](16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidVariableTypeCausesError) {
    recordSyntaxError(Message::Position(1, 12), "mismatched input 'int' expecting {'in', 'out', 'inout', ')'}");
    performTestExecution("module main(int a(16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableIdentifierInModuleParameterDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 15), "missing IDENT at '('");
    performTestExecution("module main(in (16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentifierSharingSameVariableTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 25), "a");
    performTestExecution("module main(in a(16), in a(8)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateVariableIdentifierUsingDifferentVariableTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateVariableDeclaration>(Message::Position(1, 26), "a");
    performTestExecution("module main(in a(16), out a(16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidSymbolInModuleParameterIdentifierCausesError) {
    recordSyntaxError(Message::Position(1, 16), "mismatched input '-' expecting ')'");
    performTestExecution("module main(in a-t(16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableValuesForDimensionOpeningBracketCausesError) {
    recordSyntaxError(Message::Position(1, 18), "mismatched input ']' expecting ')'");
    performTestExecution("module main(in a16](16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidVariableValuesForDimensionOpeningBracketCausesError) {
    recordSyntaxError(Message::Position(1, 16), "token recognition error at: '{'");
    recordSyntaxError(Message::Position(1, 17), "mismatched input '16' expecting ')'");
    performTestExecution("module main(in a{16](2)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableValuesForDimensionClosingBracketCausesError) {
    recordSyntaxError(Message::Position(1, 18), "missing ']' at ')'");
    performTestExecution("module main(in a[2) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidVariableValuesForDimensionClosingBracketCausesError) {
    recordSyntaxError(Message::Position(1, 18), "token recognition error at: '}'");
    recordSyntaxError(Message::Position(1, 19), "missing ']' at ')'");
    performTestExecution("module main(in a[2}) skip");
}

TEST_F(SyrecParserErrorTestsFixture, NoneNumericValueForNumberOfValuesForDimensionCausesError) {
    recordSyntaxError(Message::Position(1, 17), "mismatched input 'test' expecting INT");
    recordSyntaxError(Message::Position(1, 25), "extraneous input ')' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a[test](2)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingValueForNumberOfValuesForDimensionCausesError) {
    recordSyntaxError(Message::Position(1, 17), "missing INT at ']'");
    performTestExecution("module main(in a[]) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingOpeningBracketForModuleParameterBitwidthDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 19), "extraneous input ')' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidOpeningBracketForModuleParameterBitwidthDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 16), "token recognition error at: '{'");
    recordSyntaxError(Message::Position(1, 17), "extraneous input '16' expecting ')'");
    recordSyntaxError(Message::Position(1, 20), "extraneous input ')' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a{16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingClosingBracketForModuleParameterBitwidthDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 21), "missing ')' at 'skip'");
    performTestExecution("module main(in a(16) skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidClosingBracketForModuleParameterBitwidthDeclarationCausesError) {
    recordSyntaxError(Message::Position(1, 19), "token recognition error at: '}'");
    recordSyntaxError(Message::Position(1, 22), "missing ')' at 'skip'");
    performTestExecution("module main(in a(16}) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingModuleParameterBitwidthWithBracketsDefinedCausesError) {
    recordSyntaxError(Message::Position(1, 17), "missing INT at ')'");
    performTestExecution("module main(in a()) skip");
}

TEST_F(SyrecParserErrorTestsFixture, NoneNumericModuleParameterBitwidthCausesError) {
    recordSyntaxError(Message::Position(1, 19), "mismatched input '-' expecting ')'");
    recordSyntaxError(Message::Position(1, 22), "extraneous input ')' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a(2 -3)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredParameterBitwidthLongerThanMaximumSupportedValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 17), 33, 32);
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 40), 45, 32);
    performTestExecution("module main(in a(33), inout b(2), out c(45)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, ParameterBitwidthTakenFromUserConfigLongerThanMaximumSupportedValueCausesError) {
    constexpr unsigned int defaultSignalBitwidth           = 33;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 15), defaultSignalBitwidth, 32);
    buildAndRecordExpectedSemanticError<SemanticError::DeclaredVariableBitwidthTooLarge>(Message::Position(1, 34), defaultSignalBitwidth, 32);
    performTestExecution("module main(in a, inout b(2), out c) skip", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, ModuleParameterDeclarationWithExplicitlyDefinedBitwidthOfZeroNotPossible) {
    buildAndRecordExpectedSemanticError<SemanticError::VariableBitwidthEqualToZero>(Message::Position(1, 29));
    performTestExecution("module main(inout a(4), in b(0)) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleLocalVariableDeclarationWithExplicitlyDefinedBitwidthOfZeroNotPossible) {
    buildAndRecordExpectedSemanticError<SemanticError::VariableBitwidthEqualToZero>(Message::Position(1, 31));
    performTestExecution("module main(inout a(4)) wire b(0) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleParameterDeclarationWithImplicitlyDefinedBitwidthOfZeroNotPossible) {
    constexpr unsigned int defaultSignalBitwidth           = 0;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::VariableBitwidthEqualToZero>(Message::Position(1, 27));
    performTestExecution("module main(inout a(4), in b) ++= a", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, ModuleLocalVariableDeclarationWithImplicitlyDefinedBitwidthOfZeroNotPossible) {
    constexpr unsigned int defaultSignalBitwidth           = 0;
    const auto             userProvidedParserConfiguration = syrec::ReadProgramSettings(defaultSignalBitwidth);

    buildAndRecordExpectedSemanticError<SemanticError::VariableBitwidthEqualToZero>(Message::Position(1, 29));
    performTestExecution("module main(inout a(4)) wire b ++= a", userProvidedParserConfiguration);
}

TEST_F(SyrecParserErrorTestsFixture, ModuleParameterDeclarationWithExplicitlyDefinedNumberOfValuesForDimensionEqualToZeroNotPossible) {
    buildAndRecordExpectedSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(Message::Position(1, 20), 0);
    buildAndRecordExpectedSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(Message::Position(1, 26), 2);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 39), 1, 0, 0);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 45), 0, 2, 0);
    performTestExecution("module main(inout a[0][2][0](4)) ++= a[1][1][0]");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleLocalVariableDeclarationWithExplicitlyDefinedNumberOfValuesForDimensionEqualToZeroNotPossible) {
    buildAndRecordExpectedSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(Message::Position(1, 28), 0);
    buildAndRecordExpectedSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(Message::Position(1, 34), 2);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 46), 1, 0, 0);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 52), 0, 2, 0);
    performTestExecution("module main(in b(2)) wire a[0][2][0](4) ++= a[1][1][0]");
}
