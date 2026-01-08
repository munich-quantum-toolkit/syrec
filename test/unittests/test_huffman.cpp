/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/encoding.hpp"
#include "core/io/pla_parser.hpp"
#include "core/truthTable/truth_table.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace qc::literals;
using namespace syrec;

class TestHuff: public testing::Test {
protected:
    TruthTable  tt{};
    std::string testCircuitsDir = "./circuits/";
};

TEST_F(TestHuff, Ident2Bit) {
    const std::string circIdent2Bit = testCircuitsDir + "ident2Bit.pla";

    EXPECT_TRUE(readPla(tt, circIdent2Bit));

    EXPECT_EQ(tt.size(), 4U);

    auto search = tt.find(0b00U, 2U);

    EXPECT_TRUE(search != tt.end());

    EXPECT_TRUE(search->second.equals(0b00U, 2U));

    const TruthTable ttExtend(tt);

    encodeWithAdditionalLine(tt);

    ASSERT_TRUE(TruthTable::equal(tt, ttExtend, false));
}

TEST_F(TestHuff, HUFF1) {
    const std::string circHUFF1 = testCircuitsDir + "huff_1.pla";

    EXPECT_TRUE(readPla(tt, circHUFF1));

    EXPECT_EQ(tt.size(), 4U);

    auto search1 = tt.find(0b00U, 2U);

    EXPECT_TRUE(search1 != tt.end());

    EXPECT_TRUE(search1->second.equals(0b00U, 2U));

    EXPECT_EQ(tt.nOutputs(), 2U);

    encodeWithAdditionalLine(tt);

    EXPECT_EQ(tt.nOutputs(), 2U);

    const std::vector<std::uint64_t> encInput{0b01U, 0b10U};

    for (const auto& in1: encInput) {
        auto search = tt.find(in1, 2U);
        EXPECT_TRUE(search != tt.end());
        EXPECT_TRUE(search->second.equals("1-"));
    }

    auto search2 = tt.find(0b00U, 2U);

    EXPECT_TRUE(search2 != tt.end());

    EXPECT_TRUE(search2->second.equals(0b00U, 2U));

    auto search3 = tt.find(0b11U, 2U);

    EXPECT_TRUE(search3 != tt.end());

    EXPECT_TRUE(search3->second.equals(0b01U, 2U));
}

TEST_F(TestHuff, HUFF2) {
    const std::string circHUFF2 = testCircuitsDir + "huff_2.pla";

    EXPECT_TRUE(readPla(tt, circHUFF2));

    EXPECT_EQ(tt.size(), 4U);

    auto search1 = tt.find(0b00U, 2U);

    EXPECT_TRUE(search1 != tt.end());

    EXPECT_TRUE(search1->second.equals(0b00U, 2U));

    EXPECT_EQ(tt.nOutputs(), 2U);

    encodeWithAdditionalLine(tt);

    EXPECT_EQ(tt.nOutputs(), 3U);

    const std::vector<std::uint64_t> encInput{0b01U, 0b10U, 0b11U};

    for (const auto& in1: encInput) {
        auto search2 = tt.find(in1, 2U);
        EXPECT_TRUE(search2 != tt.end());

        EXPECT_TRUE(search2->second.equals("1--"));
    }

    auto search3 = tt.find(0b00U, 2U);

    EXPECT_TRUE(search3 != tt.end());

    EXPECT_TRUE(search3->second.equals("0--"));

    EXPECT_EQ(tt.nInputs(), 2U);

    augmentWithConstants(tt, 4U);

    EXPECT_EQ(tt.nInputs(), 4U);

    const std::vector<std::uint64_t> augInput{0b0000U, 0b0001U, 0b0010U, 0b0011U};

    for (const auto& in2: augInput) {
        auto search4 = tt.find(in2, 4U);

        EXPECT_TRUE(search4 != tt.end());
    }
}
