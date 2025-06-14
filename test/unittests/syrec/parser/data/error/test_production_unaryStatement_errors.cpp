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

TEST_F(SyrecParserErrorTestsFixture, UsageOfVariableOfTypeInAsAssignedToVariableInUnaryStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 25), "b");
    performTestExecution("module main(in b(4)) ++= b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfVariableOfTypeStateAsAssignedToVariableInUnaryStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::AssignmentToReadonlyVariable>(Message::Position(1, 29), "b");
    performTestExecution("module main() state b(4) ++= b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUndeclaredVariableAsAssignedToVariableInUnaryStatementCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 18), "b");
    performTestExecution("module main() ++= b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfUnknownUnaryAssignmentOperationInUnaryStatementCausesError) {
    recordSyntaxError(Message::Position(1, 22), "mismatched input '*' expecting {'++=', '--=', '~=', 'call', 'uncall', 'wire', 'state', 'for', 'if', 'skip', IDENT}");
    performTestExecution("module main(out b(4)) *= b");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfExpressionAsAssignedToVariableInUnaryStatementCausesError) {
    recordSyntaxError(Message::Position(1, 28), "extraneous input '(' expecting IDENT");
    recordSyntaxError(Message::Position(1, 31), "extraneous input '-' expecting {<EOF>, 'module'}");
    performTestExecution("module main(inout b(4)) ++= (b - 2)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfLoopVariableAsAssignedToVariableInUnaryAssignmentCausesError) {
    recordSyntaxError(Message::Position(1, 51), "extraneous input '$' expecting IDENT");
    buildAndRecordExpectedSemanticError<SemanticError::NoVariableMatchingIdentifier>(Message::Position(1, 52), "i");
    performTestExecution("module main(in b(4)) for $i = 0 to 3 step 1 do ++= $i rof");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfNon1DVariableInEvaluatedVariableAccessAsAssignedToVariableInUnaryAssignmentCausesError) {
    buildAndRecordExpectedSemanticError<SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue>(Message::Position(1, 31));
    performTestExecution("module main(inout b[2](4)) ++= b");
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingModuloAndOperationInDimensionAccessOfUnaryAssignment) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::Modulo);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 42));
    performTestExecution("module main(inout a[2](4), in b(2)) ++= a[((b + 6) / 3)]", customParserConfig);
}

TEST_F(SyrecParserErrorTestsFixture, DivisionByZeroDetectedDueToTruncationOfConstantValuesUsingBitwiseAndOperationInDimensionAccessOfUnaryAssignment) {
    const auto customParserConfig = syrec::ReadProgramSettings(32, utils::IntegerConstantTruncationOperation::BitwiseAnd);
    buildAndRecordExpectedSemanticError<SemanticError::ExpressionEvaluationFailedDueToDivisionByZero>(Message::Position(1, 42));
    performTestExecution("module main(inout a[2](4), in b(2)) ++= a[((b + 6) / 4)]", customParserConfig);
}
