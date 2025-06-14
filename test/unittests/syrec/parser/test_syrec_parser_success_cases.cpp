/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "utils/test_syrec_parser_base.hpp"
#include "utils/test_syrec_parser_utils.hpp"

#include <gtest/gtest.h>

using namespace syrec_parser_test_utils;

namespace {
    constexpr auto PATH_TO_SUCCESS_CASES = "./unittests/syrec/parser/data/success";

    constexpr auto PRODUCTION_PROGRAM_FILE_NAME          = "test_production_program.json";
    constexpr auto PRODUCTION_MODULE_FILE_NAME           = "test_production_module.json";
    constexpr auto PRODUCTION_ASSIGN_STATEMENT_FILE_NAME = "test_production_assignStatement.json";
    constexpr auto PRODUCTION_CALL_STATEMENT_FILE_NAME   = "test_production_callStatement.json";
    constexpr auto PRODUCTION_UNCALL_STATEMENT_FILE_NAME = "test_production_uncallStatement.json";
    constexpr auto PRODUCTION_FOR_STATEMENT_FILE_NAME    = "test_production_forStatement.json";
    constexpr auto PRODUCTION_IF_STATEMENT_FILE_NAME     = "test_production_ifStatement.json";
    constexpr auto PRODUCTION_SWAP_STATEMENT_FILE_NAME   = "test_production_swapStatement.json";
    constexpr auto PRODUCTION_UNARY_STATEMENT_FILE_NAME  = "test_production_unaryStatement.json";

    constexpr auto PRODUCTION_BINARY_EXPRESSION_FILE_NAME = "test_production_binaryExpression.json";
    constexpr auto PRODUCTION_SHIFT_EXPRESSION_FILE_NAME  = "test_production_shiftExpression.json";
    // Since the IR currently does not support such expressions, no tests can be executed for it!
    //constexpr auto PRODUCTION_UNARY_EXPRESSION_FILE_NAME  = "test_production_unaryExpression.json";
    constexpr auto PRODUCTION_SIGNAL_FILE_NAME = "test_production_signal.json";
    constexpr auto PRODUCTION_NUMBER_FILE_NAME = "test_production_number.json";

    class SyrecParserSuccessCasesTestFixture: public SyrecParserBaseTestsFixture {
    public:
        void SetUp() override {
            SyrecParserBaseTestsFixture::SetUp();
        }
    };
} // namespace

TEST_P(SyrecParserSuccessCasesTestFixture, SyrecParserSuccessCases) {
    ASSERT_NO_FATAL_FAILURE(performTestExecution());
}

INSTANTIATE_TEST_SUITE_P(
        SyrecParserSuccessCases,
        SyrecParserSuccessCasesTestFixture,
        testing::ValuesIn(loadTestCaseNamesFromFiles({FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_PROGRAM_FILE_NAME, "production_program"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_MODULE_FILE_NAME, "production_module"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_ASSIGN_STATEMENT_FILE_NAME, "production_assignStatement"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_CALL_STATEMENT_FILE_NAME, "production_callStatement"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_UNCALL_STATEMENT_FILE_NAME, "production_uncallStatement"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_FOR_STATEMENT_FILE_NAME, "production_forStatement"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_IF_STATEMENT_FILE_NAME, "production_ifStatement"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_SWAP_STATEMENT_FILE_NAME, "production_swapStatement"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_UNARY_STATEMENT_FILE_NAME, "production_unaryStatement"}),

                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_BINARY_EXPRESSION_FILE_NAME, "production_binaryExpression"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_SHIFT_EXPRESSION_FILE_NAME, "production_shiftExpression"}),
                                                      // Since the IR currently does not support such expressions, no tests can be executed for it!
                                                      //FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_UNARY_EXPRESSION_FILE_NAME, "production_unaryExpression"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_SIGNAL_FILE_NAME, "production_signal"}),
                                                      FilenameAndTestNamePrefix({PATH_TO_SUCCESS_CASES, PRODUCTION_NUMBER_FILE_NAME, "production_number"})})),
        [](const testing::TestParamInfo<SyrecParserBaseTestsFixture::ParamType>& info) { return info.param.testCaseName; });
