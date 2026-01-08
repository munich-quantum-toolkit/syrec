/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/circuit_to_truthtable.hpp"
#include "algorithms/synthesis/dd_synthesis.hpp"
#include "core/io/pla_parser.hpp"
#include "core/truthTable/truth_table.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace qc::literals;
using namespace syrec;

class TestDDSynthDc: public testing::TestWithParam<std::string> {
protected:
    TruthTable  tt{};
    TruthTable  ttqc{};
    std::string testCircuitsDir = "./circuits/";
    std::string fileName;

    void SetUp() override {
        fileName = testCircuitsDir + GetParam() + ".pla";
    }
};

INSTANTIATE_TEST_SUITE_P(TestDDSynth, TestDDSynthDc,
                         testing::Values(
                                 "huff_1",
                                 "dcX2bit",
                                 "dc3bit",
                                 "dc3bitNew",
                                 "dcX4bit",
                                 "sym6_32",
                                 "huff_2",
                                 "huff_21",
                                 "huff_31",
                                 "huff_32",
                                 "huff_33",
                                 "huff_34",
                                 "rd32_19",
                                 "c17",
                                 "con1",
                                 "sqr",
                                 "aludc",
                                 "minialu",
                                 "majority",
                                 "4gt10",
                                 "counter",
                                 "4gt5",
                                 "4mod5",
                                 "rd53",
                                 "wim",
                                 "z4",
                                 "z4ml"),
                         [](const testing::TestParamInfo<TestDDSynthDc::ParamType>& info) {
                             auto s = info.param;
                             std::ranges::replace(s, '-', '_');
                             return s; });

TEST_P(TestDDSynthDc, GenericDDSynthesisDcTest) {
    EXPECT_TRUE(readPla(tt, fileName));

    const auto& qc = DDSynthesizer::synthesizeCodingTechniques(tt);

    buildTruthTable(*qc, ttqc);

    EXPECT_TRUE(TruthTable::equal(ttqc, tt));
    EXPECT_TRUE(TruthTable::equal(tt, ttqc));

    std::cout << qc->getNops() << "\n";
}

TEST_P(TestDDSynthDc, GenericDDSynthesisDcTestEncodingWithoutAdditionalLine) {
    EXPECT_TRUE(readPla(tt, fileName));

    const auto& qc = DDSynthesizer::synthesizeCodingTechniques(tt, false);

    buildTruthTable(*qc, ttqc);

    EXPECT_TRUE(TruthTable::equal(ttqc, tt));
    EXPECT_TRUE(TruthTable::equal(tt, ttqc));

    std::cout << qc->getNops() << "\n";
}

TEST_P(TestDDSynthDc, GenericDDSynthesisOnePass) {
    EXPECT_TRUE(readPla(tt, fileName));

    const auto& qc = DDSynthesizer::synthesizeOnePass(tt);

    buildTruthTable(*qc, ttqc);

    EXPECT_TRUE(TruthTable::equal(ttqc, tt));
    EXPECT_TRUE(TruthTable::equal(tt, ttqc));

    std::cout << qc->getNops() << "\n";
}
