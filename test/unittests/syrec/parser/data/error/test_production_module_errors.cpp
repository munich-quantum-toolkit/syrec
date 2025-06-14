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

TEST_F(SyrecParserErrorTestsFixture, OmittingModuleKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 0), "mismatched input 'main' expecting 'module'");
    performTestExecution("main() skip");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidSymbolInModuleIdentifierCausesError) {
    recordSyntaxError(Message::Position(1, 0), "mismatched input 'mod' expecting 'module'");
    performTestExecution("mod-ule main() skip");
}

TEST_F(SyrecParserErrorTestsFixture, EmptyModuleIdentifierCausesError) {
    recordSyntaxError(Message::Position(1, 7), "missing IDENT at '('");
    performTestExecution("module () skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingModuleParameterListOpeningBracketCausesError) {
    recordSyntaxError(Message::Position(1, 11), "missing '(' at ')'");
    performTestExecution("module main) skip");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfInvalidModuleParameterListOpeningBracketCausesError) {
    recordSyntaxError(Message::Position(1, 11), "mismatched input '[' expecting '('");
    performTestExecution("module main[) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingModuleParameterListClosingBracketCausesError) {
    recordSyntaxError(Message::Position(1, 13), "mismatched input 'skip' expecting {'in', 'out', 'inout', ')'}");
    performTestExecution("module main( skip");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfInvalidModuleParameterListClosingBracketCausesError) {
    recordSyntaxError(Message::Position(1, 12), "mismatched input ']' expecting {'in', 'out', 'inout', ')'}");
    performTestExecution("module main(] skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingModuleParameterDelimiterCausesError) {
    recordSyntaxError(Message::Position(1, 24), "mismatched input 'out' expecting ')'");
    performTestExecution("module main(in a[2](16) out b(16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingVariableDeclarationAfterModuleParameterDelimiterCausesError) {
    recordSyntaxError(Message::Position(1, 25), "mismatched input ')' expecting {'in', 'out', 'inout'}");
    performTestExecution("module main(in a[2](16), ) skip");
}

TEST_F(SyrecParserErrorTestsFixture, EmptyModuleBodyCausesError) {
    recordSyntaxError(Message::Position(1, 24), "mismatched input '<EOF>' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(in a[2](16))");
}

TEST_F(SyrecParserErrorTestsFixture, OverloadOfModuleNamedMainCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateMainModuleDefinition>(Message::Position(1, 37));
    performTestExecution("module main(in a[2](16)) skip module main(out b[1](16)) skip");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateDeclarationOfModuleUsingSameInParameterTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateModuleDeclaration>(Message::Position(1, 63), "add");
    performTestExecution("module add(in a(4), in b(4), out res(4)) res ^= (a + b) module add(in lOp(4), in rOp(4), out res(4)) res += lOp; res += rOp");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateDeclarationOfModuleUsingSameOutParameterTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateModuleDeclaration>(Message::Position(1, 58), "double");
    performTestExecution("module double(in a(4), out res(4)) res ^= (a << 1) module double(in lOp(4), out res(4)) res += (lOp * 2)");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateDeclarationOfModuleUsingSameInoutParameterTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateModuleDeclaration>(Message::Position(1, 46), "add");
    performTestExecution("module add(inout a(4), in b(4)) a += b module add(inout lOp(4), in rOp(4)) lOp ^= rOp");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateDeclarationOfModuleWithFirstModuleUsingOutParameterAndSecondModuleUsingInoutParameterTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateModuleDeclaration>(Message::Position(1, 46), "add");
    performTestExecution("module add(inout a(4), in b(4)) a += b module add(out lOp(4), in rOp(4)) lOp ^= rOp");
}

TEST_F(SyrecParserErrorTestsFixture, DuplicateDeclarationOfModuleWithFirstModuleUsingInoutParameterAndSecondModuleUsingOutParameterTypeCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::DuplicateModuleDeclaration>(Message::Position(1, 44), "add");
    performTestExecution("module add(out a(4), in b(4)) a += b module add(inout lOp(4), in rOp(4)) lOp ^= rOp");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredBitwidthOfModuleParameterExceedingMaximumPossibleValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(Message::Position(1, 17), "4294987296", UINT_MAX);
    // Maximum possible value: 4294967296
    performTestExecution("module add(out a(4294987296)) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredNumberOfValuesOfDimensionOfModuleParameterExceedingMaximumPossibleValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(Message::Position(1, 17), "4294987296", UINT_MAX);
    // Maximum possible value: 4294967296
    performTestExecution("module add(out a[4294987296]) ++= a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredBitwidthOfModuleVariableExceedingMaximumPossibleValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(Message::Position(1, 20), "4294987296", UINT_MAX);
    // Maximum possible value: 4294967296
    performTestExecution("module add() wire a(4294987296) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredNumberOfValuesOfDimensionOfModuleVariableExceedingMaximumPossibleValueCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed>(Message::Position(1, 20), "4294987296", UINT_MAX);
    // Maximum possible value: 4294967296
    performTestExecution("module add() wire a[4294987296] ++= a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredBitwidthOfModuleParameterEqualsZeroCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::VariableBitwidthEqualToZero>(Message::Position(1, 17));
    performTestExecution("module add(out a(0)) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredNumberOfValuesOfDimensionOfModuleParameterEqualsCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(Message::Position(1, 17), 0);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 27), 0, 0, 0);
    performTestExecution("module add(out a[0]) ++= a[0]");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredBitwidthOfModuleVariableEqualsZeroCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::VariableBitwidthEqualToZero>(Message::Position(1, 20));
    performTestExecution("module add() wire a(0) ++= a");
}

TEST_F(SyrecParserErrorTestsFixture, DeclaredNumberOfValuesOfDimensionOfModuleVariableEqualsZeroCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NumberOfValuesOfDimensionEqualToZero>(Message::Position(1, 20), 0);
    buildAndRecordExpectedSemanticError<SemanticError::IndexOfAccessedValueForDimensionOutOfRange>(Message::Position(1, 29), 0, 0, 0);
    performTestExecution("module add() wire a[0] ++= a[0]");
}
