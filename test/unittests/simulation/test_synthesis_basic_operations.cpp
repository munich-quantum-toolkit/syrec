/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "base_simulation_test_fixture.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/syrec/program.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {
    void parseSyrecProgram(const std::string_view& stringifiedSyrecProgram, syrec::Program& generatedIrContainer) {
        std::string foundErrors;
        ASSERT_NO_FATAL_FAILURE(foundErrors = generatedIrContainer.readFromString(stringifiedSyrecProgram)) << "Error during parsing of SyReC program";
        ASSERT_TRUE(foundErrors.empty()) << "Expected no errors to be reported when parsing the given SyReC program but actual found errors where: " << foundErrors;
    }
}; // namespace

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfConstantZero) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(out a(1)) a ^= !0", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    constexpr std::size_t            inputStateSize = 2;
    const syrec::NBitValuesContainer inputState(inputStateSize, 0);
    const syrec::NBitValuesContainer expectedOutputState(inputStateSize, 1);
    ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfConstantOne) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(out a(1)) a ^= !1", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    constexpr std::size_t            inputStateSize = 2;
    const syrec::NBitValuesContainer inputState(inputStateSize, 0);
    const syrec::NBitValuesContainer expectedOutputState(inputStateSize, 1);
    ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfNestedExpression) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(1), in b(1), out c(1)) c ^= !(a & b)", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck   = {0, 1, 2, 3};
    const std::vector<std::uint64_t> expectedOutputStates = {
            4, // 00100
            5, // 00101
            6, // 00110
            3  // 00011
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 5;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfUnaryExpression) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(1), in b(1), out c(1)) c ^= !~(a | b)", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck   = {0, 1, 2, 3};
    const std::vector<std::uint64_t> expectedOutputStates = {
            0,
            5, // 000101
            6, // 000110
            7  // 000111
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 6;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, LogicalNegationOfVariable) {
    // module main(in a(2), out b(1)) b ^= !a.1
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(2), out b(1)) b ^= !a.1", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck = {
            0, // X000
            1, // X001
            2, // X010
            3, // X011
            4, // X100
            5, // X101
            6, // X110
            7, // X111
    };
    const std::vector<std::uint64_t> expectedOutputStates = {
            4, // X100
            5, // X101
            2, // X010
            3, // X011
            0, // X000
            1, // X001
            6, // X110
            7, // X111
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 4;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfConstant) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(out a(2)) a ^= ~2", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck   = {0, 1, 2, 3};
    const std::vector<std::uint64_t> expectedOutputStates = {
            1, // 00 ^ 01 = 01
            0, // 01 ^ 01 = 00
            3, // 10 ^ 01 = 11
            2  // 11 ^ 01 = 10
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 4;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfVariable) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(2), out b(2)) b ^= ~a", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck = {
            0,
            1, //XX0001
            2, //XX0010
            3  //XX0011
    };
    const std::vector<std::uint64_t> expectedOutputStates = {
            12, // XX1100
            9,  // XX1001
            6,  // XX0110
            3   // XX0011
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 6;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfBinaryExpression) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(2), in b(2), out c(2)) c ^= ~(a.1:0 & b.0:1)", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck = {
            0,  //000000
            1,  //000001
            2,  //000010
            3,  //000011
            4,  //000100
            5,  //000101
            6,  //000110
            7,  //000111
            8,  //001000
            9,  //001001
            10, //001010
            11, //001011
            12, //001100
            13, //001101
            14, //001110
            15, //001111
    };
    const std::vector<std::uint64_t> expectedOutputStates = {
            48, // ~(00 & 00) = 11 (0000)
            49, // ~(10 & 00) = 11 (0001)
            50, // ~(01 & 00) = 11 (0010)
            51, // ~(11 & 00) = 11 (0011)
            52, // ~(00 & 01) = 11 (0100)
            53, // ~(10 & 01) = 11 (0101)
            38, // ~(01 & 01) = 10 (0110)
            39, // ~(11 & 01) = 10 (0111)
            56, // ~(00 & 10) = 11 (1000)
            25, // ~(10 & 10) = 01 (1001)
            58, // ~(01 & 10) = 11 (1010)
            27, // ~(11 & 10) = 01 (1011)
            60, // ~(00 & 11) = 11 (1100)
            29, // ~(10 & 11) = 01 (1101)
            46, // ~(01 & 11) = 10 (1110)
            15, // ~(11 & 11) = 00 (1111)
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 10;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfShiftExpression) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(4), out b(4)) b ^= ~(a >> 2)", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck = {
            0,
            1,  //0001
            2,  //0010
            3,  //0011
            4,  //0100
            5,  //0101
            6,  //0110
            7,  //0111
            8,  //1000
            9,  //1001
            10, //1010
            11, //1011
            12, //1100
            13, //1101
            14, //1110
            15, //1111

    };
    const std::vector<std::uint64_t> expectedOutputStates = {
            240, // 1111 0000
            241, // 1111 0001
            242, // 1111 0010
            243, // 1111 0011
            228, // 1110 0100
            229, // 1110 0101
            230, // 1110 0110
            231, // 1110 0111
            216, // 1101 1000
            217, // 1101 1001
            218, // 1101 1010
            219, // 1101 1011
            204, // 1100 1100
            205, // 1100 1101
            206, // 1100 1110
            207, // 1100 1111
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 16;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BaseSimulationTestFixture, BitwiseNegationOfUnaryExpression) {
    syrec::Program program;
    ASSERT_NO_FATAL_FAILURE(parseSyrecProgram("module main(in a(1), in b(1), out c(1)) c ^= ~!(a | b)", program));
    ASSERT_TRUE(this->performProgramSynthesis(program));

    const std::vector<std::uint64_t> inputStatesToCheck   = {0, 1, 2, 3};
    const std::vector<std::uint64_t> expectedOutputStates = {
            0,
            5, // 000101
            6, // 000110
            7  // 000111
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 6;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            LogicalNegationOfConstantZero,
                            LogicalNegationOfConstantOne,
                            LogicalNegationOfNestedExpression,
                            LogicalNegationOfUnaryExpression,
                            LogicalNegationOfVariable,
                            BitwiseNegationOfConstant,
                            BitwiseNegationOfVariable,
                            BitwiseNegationOfBinaryExpression,
                            BitwiseNegationOfShiftExpression,
                            BitwiseNegationOfUnaryExpression);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
