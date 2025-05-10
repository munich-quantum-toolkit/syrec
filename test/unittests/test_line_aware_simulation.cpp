/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/simple_simulation.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/circuit.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"

#include "gtest/gtest.h"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// .clang-tidy reports a false positive here since are including the required nlohman json header file
using json = nlohmann::json; // NOLINT(misc-include-cleaner)

using namespace syrec;

class SyrecSimulationTest: public testing::TestWithParam<std::string> {
protected:
    std::string         testConfigsDir  = "./configs/";
    std::string         testCircuitsDir = "./circuits/";
    std::string         fileName;
    NBitValuesContainer input;
    NBitValuesContainer output;
    std::vector<int>    setLines;
    std::string         expectedSimOut;
    std::string         outputString;

    void SetUp() override {
        const std::string synthesisParam = GetParam();
        fileName                         = testCircuitsDir + GetParam() + ".src";
        std::ifstream i(testConfigsDir + "circuits_line_aware_simulation.json");
        json          j = json::parse(i);
        expectedSimOut  = j[synthesisParam]["sim_out"];
        setLines        = j[synthesisParam]["set_lines"].get<std::vector<int>>();
    }
};

INSTANTIATE_TEST_SUITE_P(SyrecSimulationTest, SyrecSimulationTest,
                         testing::Values(
                                 "alu_2",
                                 "swap_2",
                                 "simple_add_2",
                                 "multiply_2",
                                 "modulo_2",
                                 "negate_8"),
                         [](const testing::TestParamInfo<SyrecSimulationTest::ParamType>& info) {
                             auto s = info.param;
                             std::replace( s.begin(), s.end(), '-', '_');
                             return s; });

TEST_P(SyrecSimulationTest, GenericSimulationTest) {
    Circuit                   circ;
    Program                   prog;
    const ReadProgramSettings settings;
    const Properties::ptr     statistics;
    const std::string         errorString = prog.read(fileName, settings);
    EXPECT_TRUE(errorString.empty());
    EXPECT_TRUE(LineAwareSynthesis::synthesize(circ, prog));

    const std::size_t nCircuitLines = circ.getLines();
    input.resize(nCircuitLines);
    output.resize(nCircuitLines);

    for (const int line: setLines) {
        input.set(static_cast<std::size_t>(line));
    }
    simpleSimulation(output, circ, input, statistics);
    outputString = output.stringify();
    EXPECT_EQ(expectedSimOut, outputString);
}
