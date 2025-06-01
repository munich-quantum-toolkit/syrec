/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/program.hpp"

#include "gtest/gtest.h"
#include <algorithm>
#include <string>

using namespace syrec;

class SyrecParserTest: public testing::TestWithParam<std::string> {
protected:
    std::string testCircuitsDir = "./circuits/";
    std::string fileName;

    void SetUp() override {
        fileName = testCircuitsDir + GetParam() + ".src";
    }
};

INSTANTIATE_TEST_SUITE_P(SyrecParserTest, SyrecParserTest,
                         testing::Values(
                                 "alu_2",
                                 "binary_numeric",
                                 "bitwise_and_2",
                                 "bitwise_or_2",
                                 "bn_2",
                                 "call_8",
                                 "divide_2",
                                 "for_4",
                                 "for_32",
                                 "gray_binary_conversion_16",
                                 "input_repeated_2",
                                 "input_repeated_4",
                                 "logical_and_1",
                                 "logical_or_1",
                                 "modulo_2",
                                 "multiply_2",
                                 "negate_8",
                                 "numeric_2",
                                 "operators_repeated_4",
                                 "parity_4",
                                 "parity_check_16",
                                 "shift_4",
                                 "simple_add_2",
                                 "single_longstatement_4",
                                 "skip",
                                 "swap_2"),
                         [](const testing::TestParamInfo<SyrecParserTest::ParamType>& info) {
                             auto s = info.param;
                             std::replace( s.begin(), s.end(), '-', '_');
                             return s; });

TEST_P(SyrecParserTest, GenericParserTest) {
    Program                   prog;
    const ReadProgramSettings settings;
    std::string               errorString;
    errorString = prog.read(fileName, settings);
    EXPECT_TRUE(errorString.empty());
}
