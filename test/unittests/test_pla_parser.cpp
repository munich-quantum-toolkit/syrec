/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/io/pla_parser.hpp"
#include "core/truthTable/truth_table.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace syrec;

class PlaParserTest: public testing::Test {
protected:
    std::string testCircuitsDir = "./circuits/";
    TruthTable  testPla;

    TruthTable::Cube::Value emptyVal;

    TruthTable::Cube c11;

    TruthTable::Cube c1;

    TruthTable::Cube cOneDc;
    TruthTable::Cube cDcOne;

    void SetUp() override {
        c11 = TruthTable::Cube::fromInteger(0b11U, 2U);

        c1 = TruthTable::Cube::fromInteger(0b1U, 1U);

        cOneDc.emplace_back(true);
        cOneDc.emplace_back(emptyVal);

        cDcOne.emplace_back(emptyVal);
        cDcOne.emplace_back(true);
    }
};

TEST_F(PlaParserTest, andTest) {
    const std::string circAnd = testCircuitsDir + "and.pla";

    EXPECT_TRUE(readPla(testPla, circAnd));

    EXPECT_EQ(testPla.nInputs(), 2U);
    EXPECT_EQ(testPla.nOutputs(), 1U);
    EXPECT_EQ(testPla.size(), 4U);

    auto it11 = testPla.find(0b11U, 2U);

    EXPECT_TRUE(it11 != testPla.end());
    EXPECT_TRUE(it11->second.equals(0b1U, 1U));
}

TEST_F(PlaParserTest, orTest) {
    const std::string circOr = testCircuitsDir + "or.pla";

    EXPECT_TRUE(readPla(testPla, circOr));

    EXPECT_EQ(testPla.nInputs(), 2U);
    EXPECT_EQ(testPla.nOutputs(), 1U);
    EXPECT_EQ(testPla.size(), 4U);

    auto itDcOne = testPla.find("11");

    EXPECT_TRUE(itDcOne != testPla.end());
    EXPECT_TRUE(itDcOne->second.equals(0b1U, 1U));

    auto it1 = testPla.find("10");

    EXPECT_TRUE(it1 != testPla.end());
    EXPECT_TRUE(it1->second.equals(0b1U, 1U));
}
