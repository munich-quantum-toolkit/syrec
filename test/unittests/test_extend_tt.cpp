/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/io/pla_parser.hpp"
#include "core/truthTable/truth_table.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace qc::literals;
using namespace syrec;

class TruthTableExtend: public testing::Test {
protected:
    TruthTable  tt{};
    std::string testCircuitsDir = "./circuits/";
};

TEST_F(TruthTableExtend, Max) {
    const std::string circMax = testCircuitsDir + "max.pla";

    // the max.pla consist of 64 inputs. Currently, functions with less than 63 inputs are supported.
    EXPECT_ANY_THROW(readPla(tt, circMax));
}

TEST_F(TruthTableExtend, Ident2Bit) {
    // create identity truth table
    const std::string circIdent2Bit = testCircuitsDir + "ident2Bit.pla";

    EXPECT_TRUE(readPla(tt, circIdent2Bit));

    EXPECT_EQ(tt.size(), 4U);

    auto search = tt.find(0b00U, 2U);

    EXPECT_TRUE(search != tt.end());

    EXPECT_TRUE(search->second.equals(0b00U, 2U));
}

TEST_F(TruthTableExtend, X2Bit) {
    // create X truth table (X gate applied on both the bits)

    const std::string circX2Bit = testCircuitsDir + "x2Bit.pla";

    EXPECT_TRUE(readPla(tt, circX2Bit));

    EXPECT_EQ(tt.size(), 4U);

    auto search = tt.find(0b11U, 2U);

    EXPECT_TRUE(search != tt.end());

    EXPECT_TRUE(search->second.equals(0b00U, 2U));
}

TEST_F(TruthTableExtend, EXTENDTT) {
    const std::string circEXTENDTT = testCircuitsDir + "extend.pla";

    EXPECT_TRUE(readPla(tt, circEXTENDTT));

    EXPECT_EQ(tt.size(), 8U);

    const std::vector<std::uint64_t> outAssigned1{0b011U, 0b111U};

    for (const auto& in1: outAssigned1) {
        auto search = tt.find(in1, 3U);

        EXPECT_TRUE(search != tt.end());

        EXPECT_TRUE(search->second.equals(0b111U, 3U));
    }

    const std::vector<std::uint64_t> outAssigned2{0b100U, 0b110U};

    for (const auto& in2: outAssigned2) {
        auto search = tt.find(in2, 3U);

        EXPECT_TRUE(search != tt.end());

        EXPECT_TRUE(search->second.equals(0b101U, 3U));
    }

    const std::vector<std::uint64_t> notAssigned{0b000U, 0b001U, 0b010U, 0b101U};

    for (const auto& in3: notAssigned) {
        auto search = tt.find(in3, 3U);

        EXPECT_TRUE(search != tt.end());

        EXPECT_TRUE(search->second.equals(0b000U, 3U));
    }
}
