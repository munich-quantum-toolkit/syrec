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

TEST_F(SyrecParserErrorTestsFixture, OmittingUncallKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 95), "no viable alternative at input 'add('");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) add(a, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidUncallKeywordCausesError) {
    recordSyntaxError(Message::Position(1, 104), "no viable alternative at input 'performCall add'");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) performCall add(a, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingModuleIdentifierInUncallStatementCausesError) {
    recordSyntaxError(Message::Position(1, 47), "extraneous input '(' expecting IDENT");
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingIdentifier>(Message::Position(1, 48), "a");
    recordSyntaxError(Message::Position(1, 49), "mismatched input ',' expecting '('");
    performTestExecution("module main(in a(4), in b(4), out c(4)) uncall (a, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleIdentifierNotMatchingAnyDeclaredModuleIdentifierInUncallStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingIdentifier>(Message::Position(1, 47), "add");
    performTestExecution("module main(in a(4), in b(4), out c(4)) uncall add(a, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingUncallStatementParameterOpeningBracket) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingIdentifier>(Message::Position(1, 99), "adda");
    recordSyntaxError(Message::Position(1, 103), "mismatched input ',' expecting '('");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall adda, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidUncallStatementParameterOpeningBracket) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 99));
    recordSyntaxError(Message::Position(1, 102), "mismatched input '[' expecting '('");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall add[a, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingUncallStatementParameterIdentifierCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 99));
    recordSyntaxError(Message::Position(1, 103), "extraneous input ',' expecting IDENT");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall add(, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, UncallStatementParameterIdentifierNotMatchingAnyDeclaredVariableCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 99));
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 103), "d");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall add(d, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingUncallStatementParameterDelimiterCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 99));
    recordSyntaxError(Message::Position(1, 105), "extraneous input 'b' expecting {',', ')'}");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall add(a b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, OmittingUncallStatementParameterClosingBracket) {
    recordSyntaxError(Message::Position(1, 110), "extraneous input '<EOF>' expecting {',', ')'}");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall add(a, b, c");
}

TEST_F(SyrecParserErrorTestsFixture, InvalidUncallStatementParameterClosingBracket) {
    recordSyntaxError(Message::Position(1, 110), "extraneous input ']' expecting {',', ')'}");
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in a(4), in b(4), out c(4)) uncall add(a, b, c]");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToVariableBitwidthMismatchCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 153));
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module add(in a(8), in b(8), out c(8)) c ^= (a + b) module main(in a(12), in b(8), out c(10)) uncall add(a, b, c)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToVariableTypeMismatchCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 104));
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(in res(4)) wire tmp1(4), tmp2(4) uncall add(tmp1, tmp2, res)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToNumberOfValuesForDimensionBeingSmallerThanExpectedForParameterCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 111));
    performTestExecution("module add(in a[2](4), in b(4), out c(4)) c ^= (a[0] + b) module main(out res(4)) wire tmp1(4), tmp2(4) uncall add(tmp1, tmp2, res)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToNumberOfValuesForDimensionBeingLargerThanExpectedForParameterCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 108));
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(out res[2](4)) wire tmp1(4), tmp2(4) uncall add(tmp1, tmp2, res)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToNumberOfDimensionsOfUserVariableSmallerThanExpectedForParameterCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 117));
    performTestExecution("module add(in a(4), in b[2][3](4), out c(4)) c ^= (a + b[0][1]) module main(out res(4)) wire tmp1(4), tmp2(4) uncall add(tmp1, tmp2, res)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToNumberOfDimensionsOfUserVariableLargerThanExpectedForParameterCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 169));
    performTestExecution("module add(in a(4), in b[2](4), out c(4)) c ^= (a + b[0]) module add(in a(4), in b(4), out c(4)) c ^= (a + b) module main(out res[2](4)) wire tmp1(4), tmp2[2](4) uncall add(tmp1, tmp2, res)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToTooManyUserDefinedParametersCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 136));
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module add(in a(4), out c(4)) c ^= a module main(in a(4), in b(4), out c(4)) uncall add(a, b, c, c)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionFailedDueToTooFewUserDefinedParametersCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoModuleMatchingCallSignature>(Message::Position(1, 136));
    performTestExecution("module add(in a(4), in b(4), out c(4)) c ^= (a + b) module add(in a(4), out c(4)) c ^= a module main(in a(4), in b(4), out c(4)) uncall add(a, b)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOfImplicitlyDefinedMainModuleCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::CannotCallMainModule>(Message::Position(1, 55));
    performTestExecution("module increment(out c(4)) wire one(4) ++= one; uncall add(c, one, c) module add(in a(4), in b(4), out c(4)) c ^= (a + b)");
}

TEST_F(SyrecParserErrorTestsFixture, ModuleUncallOverloadResolutionResolvesToOfImplicitlyDefinedMainModuleCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::CannotCallMainModule>(Message::Position(1, 55));
    performTestExecution("module increment(out c(4)) wire one(4) ++= one; uncall add(c, one) module add(in a(4), in b(4), out c(4)) c ^= (a + b) module add(inout a(4), in b(4)) a += b");
}
