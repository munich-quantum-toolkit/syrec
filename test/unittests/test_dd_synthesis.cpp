/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/dd_synthesis.hpp"
#include "core/io/pla_parser.hpp"
#include "core/truthTable/truth_table.hpp"
#include "dd/FunctionalityConstruction.hpp"
#include "dd/Package.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>

using namespace syrec;

class TestDDSynth: public testing::TestWithParam<std::string> {
protected:
    TruthTable                   tt{};
    std::string                  testCircuitsDir = "./circuits/";
    std::unique_ptr<dd::Package> dd              = std::make_unique<dd::Package>(15U);
    std::string                  fileName;

    void SetUp() override {
        fileName = testCircuitsDir + GetParam() + ".pla";
    }
};

INSTANTIATE_TEST_SUITE_P(TestDDSynth, TestDDSynth,
                         testing::Values(
                                 "swap",
                                 "toffoli",
                                 "x2Bit",
                                 "test_dd_synthesis_1",
                                 "test_dd_synthesis_2",
                                 "3_17_6",
                                 "bitwiseXor2Bit",
                                 "adder2Bit",
                                 "adder3Bit",
                                 "4_49_7",
                                 "hwb4_12",
                                 "hwb5_13",
                                 "hwb6_14",
                                 "hwb7_15",
                                 "hwb8_64",
                                 "hwb9_65",
                                 "graycode",
                                 "hamming_7",
                                 "mod4096",
                                 "mod8192",
                                 "mod638192",
                                 "14_bit",
                                 "urf1",
                                 "urf2",
                                 "urf3",
                                 "urf4",
                                 "urf5"),
                         [](const testing::TestParamInfo<TestDDSynth::ParamType>& info) {
                             auto s = info.param;
                             std::ranges::replace(s, '-', '_');
                             return s; });

TEST_P(TestDDSynth, GenericDDSynthesisTest) {
    EXPECT_TRUE(readPla(tt, fileName));

    const auto ttDD = buildDD(tt, dd);
    EXPECT_TRUE(ttDD.p != nullptr);
    DDSynthesizer synthesizer{};

    const auto  qc   = synthesizer.synthesize(ttDD, dd);
    const auto& qcDD = dd::buildFunctionality(*qc, *dd);
    EXPECT_TRUE(ttDD == qcDD);

    std::cout << synthesizer.numGate() << "\n";
    std::cout << synthesizer.getExecutionTime() << "\n";
}
