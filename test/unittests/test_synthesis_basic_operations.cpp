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
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <utility>

template<typename T>
class BasicOperationSynthesisResultSimulationTestFixture: public testing::Test {
public:
    void SetUp() override {
        static_assert(std::is_same_v<T, syrec::CostAwareSynthesis> || std::is_same_v<T, syrec::LineAwareSynthesis>);
    }

    void assertSimulationResultForStateMatchesExpectedOne(const syrec::NBitValuesContainer& inputState, const syrec::NBitValuesContainer& expectedOutputState) const {
        ASSERT_EQ(inputState.size(), expectedOutputState.size());

        syrec::NBitValuesContainer actualOutputState(inputState.size());
        ASSERT_NO_FATAL_FAILURE(syrec::simpleSimulation(actualOutputState, annotatableQuantumComputation, inputState));
        ASSERT_EQ(actualOutputState.size(), expectedOutputState.size());

        // We are assuming that the indices of the ancilla qubits are larger than the one of the inputs/output qubits.
        const std::size_t numQubitsToCheck = annotatableQuantumComputation.getNqubitsWithoutAncillae();
        for (std::size_t i = 0; i < numQubitsToCheck; ++i) {
            ASSERT_EQ(expectedOutputState[i], actualOutputState[i]) << "Value missmatch during simulation at qubit " << std::to_string(i) << ", expected: " << std::to_string(static_cast<int>(expectedOutputState[i])) << " but was " << std::to_string(static_cast<int>(actualOutputState[i]))
                                                                    << "!\nInput state: " << inputState.stringify() << " | Expected output state: " << expectedOutputState.stringify() << " | Actual output state: " << actualOutputState.stringify();
        }
    }

    void performProgramSynthesis(const syrec::Program& program) {
        if (std::is_same_v<T, syrec::CostAwareSynthesis>) {
            ASSERT_TRUE(syrec::CostAwareSynthesis::synthesize(annotatableQuantumComputation, program));
        } else {
            ASSERT_TRUE(syrec::LineAwareSynthesis::synthesize(annotatableQuantumComputation, program));
        }
    }

    syrec::AnnotatableQuantumComputation annotatableQuantumComputation;
};

TYPED_TEST_SUITE_P(BasicOperationSynthesisResultSimulationTestFixture);

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, LogicalNegationOfConstantZero) {
    // module main(out a(1)) a ^= !0
    syrec::Program program;
    auto           mainModule          = std::make_shared<syrec::Module>("main");
    const auto     modifiableParameter = std::make_shared<syrec::Variable>(syrec::Variable::Out, "a", std::vector<unsigned>({1}), 1U);
    mainModule->addParameter(modifiableParameter);

    const auto containerForConstantZero      = std::make_shared<syrec::Number>(0U);
    const auto containerExprForConstantValue = std::make_shared<syrec::NumericExpression>(containerForConstantZero, 1U);
    auto       unaryExpr                     = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::LogicalNegation, containerExprForConstantValue);

    auto accessOnModifiableParameter = std::make_shared<syrec::VariableAccess>();
    accessOnModifiableParameter->var = modifiableParameter;

    auto unaryAssignStmt = std::make_shared<syrec::AssignStatement>(accessOnModifiableParameter, syrec::AssignStatement::Exor, unaryExpr);
    mainModule->addStatement(unaryAssignStmt);
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

    constexpr std::size_t            inputStateSize = 3;
    const syrec::NBitValuesContainer inputState(inputStateSize, 0);
    const syrec::NBitValuesContainer expectedOutputState(inputStateSize, 1);
    ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
}

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, LogicalNegationOfConstantOne) {
    // module main(out a(1)) a ^= !1 with a initialized to one during simulation
    syrec::Program program;
    auto           mainModule          = std::make_shared<syrec::Module>("main");
    const auto     modifiableParameter = std::make_shared<syrec::Variable>(syrec::Variable::Out, "a", std::vector<unsigned>({1}), 1U);
    mainModule->addParameter(modifiableParameter);

    const auto containerForConstantOne       = std::make_shared<syrec::Number>(1U);
    const auto containerExprForConstantValue = std::make_shared<syrec::NumericExpression>(containerForConstantOne, 1U);
    auto       unaryExpr                     = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::LogicalNegation, containerExprForConstantValue);

    auto accessOnModifiableParameter = std::make_shared<syrec::VariableAccess>();
    accessOnModifiableParameter->var = modifiableParameter;
    accessOnModifiableParameter->indexes.emplace_back(std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(0), 1U));

    auto unaryAssignStmt = std::make_shared<syrec::AssignStatement>(accessOnModifiableParameter, syrec::AssignStatement::Exor, unaryExpr);
    mainModule->addStatement(unaryAssignStmt);
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

    constexpr std::size_t            inputStateSize = 3;
    const syrec::NBitValuesContainer inputState(inputStateSize, 1);
    const syrec::NBitValuesContainer expectedOutputState(inputStateSize, 1);
    ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
}

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, LogicalNegationOfNestedExpression) {
    // module main(in a(1), in b(1), out c(1)) c ^= !(a & b)
    syrec::Program program;
    auto           mainModule           = std::make_shared<syrec::Module>("main");
    const auto     nestedExprLhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 1U);
    const auto     nestedExprRhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "b", std::vector<unsigned>({1}), 1U);
    const auto     assignedToVariable   = std::make_shared<syrec::Variable>(syrec::Variable::Out, "c", std::vector<unsigned>({1}), 1U);
    mainModule->addParameter(nestedExprLhsOperand);
    mainModule->addParameter(nestedExprRhsOperand);
    mainModule->addParameter(assignedToVariable);

    auto accessOnNestedExprLhsOperand = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprLhsOperand->var = nestedExprLhsOperand;

    auto accessOnNestedExprRhsOperand = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprRhsOperand->var = nestedExprRhsOperand;

    auto nestedExpr        = std::make_shared<syrec::BinaryExpression>(std::make_shared<syrec::VariableExpression>(accessOnNestedExprLhsOperand), syrec::BinaryExpression::BitwiseAnd, std::make_shared<syrec::VariableExpression>(accessOnNestedExprRhsOperand));
    auto assignmentRhsExpr = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::LogicalNegation, nestedExpr);

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, assignmentRhsExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

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

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, LogicalNegationOfUnaryExpression) {
    // module main(in a(1), in b(1), out c(1)) c ^= !(~(a | b))
    syrec::Program program;
    auto           mainModule           = std::make_shared<syrec::Module>("main");
    const auto     nestedExprLhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 1U);
    const auto     nestedExprRhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "b", std::vector<unsigned>({1}), 1U);
    const auto     assignedToVariable   = std::make_shared<syrec::Variable>(syrec::Variable::Out, "c", std::vector<unsigned>({1}), 1U);
    mainModule->addParameter(nestedExprLhsOperand);
    mainModule->addParameter(nestedExprRhsOperand);
    mainModule->addParameter(assignedToVariable);

    auto accessOnNestedExprLhsOperand = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprLhsOperand->var = nestedExprLhsOperand;

    auto accessOnNestedExprRhsOperand = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprRhsOperand->var = nestedExprRhsOperand;

    auto innerBinaryExpr = std::make_shared<syrec::BinaryExpression>(std::make_shared<syrec::VariableExpression>(accessOnNestedExprLhsOperand), syrec::BinaryExpression::BitwiseOr, std::make_shared<syrec::VariableExpression>(accessOnNestedExprRhsOperand));
    auto innerUnaryExpr  = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, innerBinaryExpr);
    auto unaryExpr       = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::LogicalNegation, innerUnaryExpr);

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, unaryExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

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

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, LogicalNegationOfVariable) {
    // module main(in a(2), out b(1)) b ^= !a.1
    syrec::Program program;
    auto           mainModule         = std::make_shared<syrec::Module>("main");
    const auto     negatedVariable    = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 2U);
    const auto     assignedToVariable = std::make_shared<syrec::Variable>(syrec::Variable::Out, "b", std::vector<unsigned>({1}), 1U);
    mainModule->addParameter(negatedVariable);
    mainModule->addParameter(assignedToVariable);

    const auto containerForAccessedBitOfNegatedVariable = std::make_shared<syrec::Number>(1);
    auto       accessOnNegatedVariable                  = std::make_shared<syrec::VariableAccess>();
    accessOnNegatedVariable->var                        = negatedVariable;
    accessOnNegatedVariable->range                      = std::make_pair(containerForAccessedBitOfNegatedVariable, containerForAccessedBitOfNegatedVariable);

    auto assignmentRhsExpr = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, std::make_shared<syrec::VariableExpression>(accessOnNegatedVariable));

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, assignmentRhsExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

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

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, BitwiseNegationOfConstant) {
    // module main(out a(2)) a ^= ~2
    syrec::Program program;
    auto           mainModule          = std::make_shared<syrec::Module>("main");
    const auto     modifiableParameter = std::make_shared<syrec::Variable>(syrec::Variable::Out, "a", std::vector<unsigned>({1}), 2U);
    mainModule->addParameter(modifiableParameter);

    const auto containerForConstantValue     = std::make_shared<syrec::Number>(2U);
    const auto containerExprForConstantValue = std::make_shared<syrec::NumericExpression>(containerForConstantValue, 2U);
    auto       unaryExpr                     = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, containerExprForConstantValue);

    auto accessOnModifiableParameter = std::make_shared<syrec::VariableAccess>();
    accessOnModifiableParameter->var = modifiableParameter;

    auto unaryAssignStmt = std::make_shared<syrec::AssignStatement>(accessOnModifiableParameter, syrec::AssignStatement::Exor, unaryExpr);
    mainModule->addStatement(unaryAssignStmt);
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

    const std::vector<std::uint64_t> inputStatesToCheck   = {0, 1, 2, 3};
    const std::vector<std::uint64_t> expectedOutputStates = {
            1, // 00 ^ 01 = 01
            0, // 01 ^ 01 = 00
            3, // 10 ^ 01 = 11
            2  // 11 ^ 01 = 10
    };

    for (std::size_t i = 0; i < inputStatesToCheck.size(); ++i) {
        constexpr std::size_t            inputStateSize = 6;
        const syrec::NBitValuesContainer inputState(inputStateSize, inputStatesToCheck[i]);
        const syrec::NBitValuesContainer expectedOutputState(inputStateSize, expectedOutputStates[i]);
        ASSERT_NO_FATAL_FAILURE(this->assertSimulationResultForStateMatchesExpectedOne(inputState, expectedOutputState));
    }
}

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, BitwiseNegationOfVariable) {
    // module main(in a(2), out b(2)) b ^= ~a
    syrec::Program program;
    auto           mainModule         = std::make_shared<syrec::Module>("main");
    const auto     negatedVariable    = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 2U);
    const auto     assignedToVariable = std::make_shared<syrec::Variable>(syrec::Variable::Out, "b", std::vector<unsigned>({1}), 2U);
    mainModule->addParameter(negatedVariable);
    mainModule->addParameter(assignedToVariable);

    auto accessOnNegatedVariable = std::make_shared<syrec::VariableAccess>();
    accessOnNegatedVariable->var = negatedVariable;

    auto assignmentRhsExpr = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, std::make_shared<syrec::VariableExpression>(accessOnNegatedVariable));

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, assignmentRhsExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

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

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, BitwiseNegationOfBinaryExpression) {
    // module main(in a(2), in b(2), out c(2)) c ^= ~(a:1.0 & b.0:1)
    syrec::Program program;
    auto           mainModule           = std::make_shared<syrec::Module>("main");
    const auto     nestedExprLhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 2U);
    const auto     nestedExprRhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "b", std::vector<unsigned>({1}), 2U);
    const auto     assignedToVariable   = std::make_shared<syrec::Variable>(syrec::Variable::Out, "c", std::vector<unsigned>({1}), 2U);
    mainModule->addParameter(nestedExprLhsOperand);
    mainModule->addParameter(nestedExprRhsOperand);
    mainModule->addParameter(assignedToVariable);

    auto accessOnNestedExprLhsOperand   = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprLhsOperand->var   = nestedExprLhsOperand;
    accessOnNestedExprLhsOperand->range = std::make_pair(std::make_shared<syrec::Number>(1U), std::make_shared<syrec::Number>(0U));

    auto accessOnNestedExprRhsOperand   = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprRhsOperand->var   = nestedExprRhsOperand;
    accessOnNestedExprRhsOperand->range = std::make_pair(std::make_shared<syrec::Number>(0U), std::make_shared<syrec::Number>(1U));

    auto nestedExpr        = std::make_shared<syrec::BinaryExpression>(std::make_shared<syrec::VariableExpression>(accessOnNestedExprLhsOperand), syrec::BinaryExpression::BitwiseAnd, std::make_shared<syrec::VariableExpression>(accessOnNestedExprRhsOperand));
    auto assignmentRhsExpr = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, nestedExpr);

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, assignmentRhsExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

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

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, BitwiseNegationOfShiftExpression) {
    // module main(in a(4), out b(4)) b ^= ~(a >> 2)
    syrec::Program program;
    auto           mainModule         = std::make_shared<syrec::Module>("main");
    auto           toBeShiftedOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 4U);
    const auto     assignedToVariable = std::make_shared<syrec::Variable>(syrec::Variable::Out, "b", std::vector<unsigned>({1}), 4U);
    mainModule->addParameter(toBeShiftedOperand);
    mainModule->addParameter(assignedToVariable);

    auto accessOnToBeShiftedOperand = std::make_shared<syrec::VariableAccess>();
    accessOnToBeShiftedOperand->var = toBeShiftedOperand;

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;

    auto shiftExpr = std::make_shared<syrec::ShiftExpression>(std::make_shared<syrec::VariableExpression>(accessOnToBeShiftedOperand), syrec::ShiftExpression::Right, std::make_shared<syrec::Number>(2));
    auto unaryExpr = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, shiftExpr);
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, unaryExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

    const auto x = this->annotatableQuantumComputation.toQASM();

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

TYPED_TEST_P(BasicOperationSynthesisResultSimulationTestFixture, BitwiseNegationOfUnaryExpression) {
    // module main(in a(1), in b(1), out c(1)) c ^= ~(!(a | b))
    syrec::Program program;
    auto           mainModule           = std::make_shared<syrec::Module>("main");
    const auto     nestedExprLhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "a", std::vector<unsigned>({1}), 1U);
    const auto     nestedExprRhsOperand = std::make_shared<syrec::Variable>(syrec::Variable::In, "b", std::vector<unsigned>({1}), 1U);
    const auto     assignedToVariable   = std::make_shared<syrec::Variable>(syrec::Variable::Out, "c", std::vector<unsigned>({1}), 1U);
    mainModule->addParameter(nestedExprLhsOperand);
    mainModule->addParameter(nestedExprRhsOperand);
    mainModule->addParameter(assignedToVariable);

    auto accessOnNestedExprLhsOperand = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprLhsOperand->var = nestedExprLhsOperand;

    auto accessOnNestedExprRhsOperand = std::make_shared<syrec::VariableAccess>();
    accessOnNestedExprRhsOperand->var = nestedExprRhsOperand;

    auto innerBinaryExpr = std::make_shared<syrec::BinaryExpression>(std::make_shared<syrec::VariableExpression>(accessOnNestedExprLhsOperand), syrec::BinaryExpression::BitwiseOr, std::make_shared<syrec::VariableExpression>(accessOnNestedExprRhsOperand));
    auto innerUnaryExpr  = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::LogicalNegation, innerBinaryExpr);
    auto unaryExpr       = std::make_shared<syrec::UnaryExpression>(syrec::UnaryExpression::BitwiseNegation, innerUnaryExpr);

    auto accessOnAssignedToVariable = std::make_shared<syrec::VariableAccess>();
    accessOnAssignedToVariable->var = assignedToVariable;
    mainModule->addStatement(std::make_shared<syrec::AssignStatement>(accessOnAssignedToVariable, syrec::AssignStatement::Exor, unaryExpr));
    program.addModule(mainModule);
    this->performProgramSynthesis(program);

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

REGISTER_TYPED_TEST_SUITE_P(BasicOperationSynthesisResultSimulationTestFixture,
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
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BasicOperationSynthesisResultSimulationTestFixture, SynthesizerTypes,);