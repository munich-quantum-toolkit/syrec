/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "test_syrec_parser_errors_base.hpp"

#include <gtest/gtest.h>

using namespace syrec_parser_error_tests;

TEST_F(SyrecParserErrorTestsFixture, UsageOfLogicalNegationOperationInUnaryExpressionNotSupportedAndCausesError) {
    expectedErrorMessages.emplace_back(syrec_parser::Message::Type::Error, "UNKNOWN", syrec_parser::Message::Position(1, 36), "Unary expressions are currently not supported");
    performTestExecution("module main(in a(4), out b(1)) b ^= !(a > 2)");
}

TEST_F(SyrecParserErrorTestsFixture, UsageOfBitwiseNegationOperationInUnaryExpressionNotSupportedAndCausesError) {
    expectedErrorMessages.emplace_back(syrec_parser::Message::Type::Error, "UNKNOWN", syrec_parser::Message::Position(1, 35), "Unary expressions are currently not supported");
    expectedErrorMessages.emplace_back(syrec_parser::Message::Type::Error, "UNKNOWN", syrec_parser::Message::Position(1, 74), "Unary expressions are currently not supported");
    performTestExecution("module main(in a(4), out b(4)) if (~(a - 2) > 2) then ++= b else skip fi (~(a - 2) > 2)");
}
