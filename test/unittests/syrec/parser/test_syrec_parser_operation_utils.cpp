/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/expression.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"

#include <climits>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <utility>
#include <variant>

using namespace utils;

namespace {
    constexpr unsigned int MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION = 32;

    void assertIntegerConstantTruncationResultMatchesExpectedOne(unsigned int integerConstantToTruncate, unsigned int expectedBitwidth, utils::IntegerConstantTruncationOperation integerConstantTruncationOperation, const std::optional<unsigned int>& expectedTruncationResult) {
        const std::optional<unsigned int> actualTruncationResult = utils::truncateConstantValueToExpectedBitwidth(integerConstantToTruncate, expectedBitwidth, integerConstantTruncationOperation);
        if (expectedTruncationResult.has_value()) {
            ASSERT_TRUE(actualTruncationResult.has_value()) << "Expected that integer constant truncation succeeds and returns a value but none was returned";
            ASSERT_EQ(expectedTruncationResult.value(), actualTruncationResult.value());
        } else {
            ASSERT_FALSE(actualTruncationResult.has_value()) << "Expected that integer constant truncation does not return a result but was actually " << std::to_string(actualTruncationResult.value());
        }
    }

    struct SyrecOperationDefinitionTestData {
        std::string                                                                                    testCaseName;
        std::optional<unsigned int>                                                                    lhsOperand;
        std::variant<syrec::BinaryExpression::BinaryOperation, syrec::ShiftExpression::ShiftOperation> operationVariant;
        std::optional<unsigned int>                                                                    rhsOperand;
        std::optional<unsigned int>                                                                    expectedEvaluationResult;

        explicit SyrecOperationDefinitionTestData(
                std::string&&                                                                                  testCaseName,
                const std::optional<unsigned int>&                                                             lhsOperand,
                std::variant<syrec::BinaryExpression::BinaryOperation, syrec::ShiftExpression::ShiftOperation> operationVariant,
                const std::optional<unsigned int>&                                                             rhsOperand,
                const std::optional<unsigned int>&                                                             expectedEvaluationResult):
            testCaseName(std::move(testCaseName)),
            lhsOperand(lhsOperand),
            operationVariant(operationVariant), rhsOperand(rhsOperand), expectedEvaluationResult(expectedEvaluationResult) {}
    };

    class SyrecParserOperationEvaluationUtilsTestFixture: public testing::TestWithParam<SyrecOperationDefinitionTestData> {};
} // namespace

TEST_P(SyrecParserOperationEvaluationUtilsTestFixture, OperationEvaluation) {
    const SyrecOperationDefinitionTestData& testCaseData = GetParam();
    std::optional<unsigned int>             actualEvaluationResult;

    if (std::holds_alternative<syrec::BinaryExpression::BinaryOperation>(testCaseData.operationVariant)) {
        actualEvaluationResult = utils::tryEvaluate(testCaseData.lhsOperand, std::get<syrec::BinaryExpression::BinaryOperation>(testCaseData.operationVariant), testCaseData.rhsOperand);
    } else if (std::holds_alternative<syrec::ShiftExpression::ShiftOperation>(testCaseData.operationVariant)) {
        actualEvaluationResult = utils::tryEvaluate(testCaseData.lhsOperand, std::get<syrec::ShiftExpression::ShiftOperation>(testCaseData.operationVariant), testCaseData.rhsOperand);
    } else {
        FAIL() << "Unknown operation variant";
    }

    if (const std::optional<unsigned int> expectedEvaluationResult = testCaseData.expectedEvaluationResult; expectedEvaluationResult.has_value()) {
        ASSERT_TRUE(actualEvaluationResult.has_value()) << "Expected evaluation of operation to return a result but none was actually returned";
        ASSERT_EQ(expectedEvaluationResult.value(), actualEvaluationResult.value());
    } else {
        ASSERT_FALSE(actualEvaluationResult.has_value()) << "Expected evaluation of operation to not return a result but was actually " << std::to_string(actualEvaluationResult.value());
    }
}

INSTANTIATE_TEST_SUITE_P(SyrecOperationEvaluationUtilsTests, SyrecParserOperationEvaluationUtilsTestFixture,
                         testing::ValuesIn({SyrecOperationDefinitionTestData("additionWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::Add, 4U, 6U),
                                            SyrecOperationDefinitionTestData("additionWithLhsOperandValueUnknownAndRhsOperandValueKnown", std::nullopt, syrec::BinaryExpression::BinaryOperation::Add, 4U, std::nullopt),
                                            SyrecOperationDefinitionTestData("additionWithLhsOperandValueKnownAndRhsOperandValueUnknown", 4U, syrec::BinaryExpression::BinaryOperation::Add, std::nullopt, std::nullopt),
                                            SyrecOperationDefinitionTestData("additionWithUnknownOperandValues", std::nullopt, syrec::BinaryExpression::BinaryOperation::Add, std::nullopt, std::nullopt),

                                            SyrecOperationDefinitionTestData("subtractWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::Subtract, 4U, UINT_MAX - 1U),

                                            SyrecOperationDefinitionTestData("exorWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::Exor, 4U, 6U),
                                            SyrecOperationDefinitionTestData("exorWithKnownOperandValuesEqualsToIdentityElement", 0U, syrec::BinaryExpression::BinaryOperation::Exor, 0U, 0U),
                                            SyrecOperationDefinitionTestData("exorWithKnownOperandValuesAndLhsOperandEqualToIdentityElement", 0U, syrec::BinaryExpression::BinaryOperation::Exor, 2U, 2U),
                                            SyrecOperationDefinitionTestData("exorWithKnownOperandValuesAndRhsOperandEqualToIdentityElement", 2U, syrec::BinaryExpression::BinaryOperation::Exor, 0U, 2U),

                                            SyrecOperationDefinitionTestData("multiplyWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::Multiply, 4U, 8U),

                                            SyrecOperationDefinitionTestData("divisionWithKnownOperandValues", 5U, syrec::BinaryExpression::BinaryOperation::Divide, 2U, 2U),
                                            SyrecOperationDefinitionTestData("divisionWithKnownOperandValuesAndLhsOperandEqualToZero", 0U, syrec::BinaryExpression::BinaryOperation::Divide, 4U, 0U),
                                            SyrecOperationDefinitionTestData("divisionWithKnownOperandValuesAndRhsOperandEqualToZero", 2U, syrec::BinaryExpression::BinaryOperation::Divide, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("divisionWithLhsOperandValueUnknownAndRhsOperandValueKnownAndEqualToZero", 2U, syrec::BinaryExpression::BinaryOperation::Divide, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("divisionWithLhsOperandValueKnownAndEqualToZeroAndRhsOperandValueUnknown", 0U, syrec::BinaryExpression::BinaryOperation::Divide, 2U, 0U),

                                            SyrecOperationDefinitionTestData("moduloWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::Modulo, 4U, 2U),
                                            SyrecOperationDefinitionTestData("moduloWithKnownOperandValuesAndLhsOperandEqualZero", 0U, syrec::BinaryExpression::BinaryOperation::Modulo, 4U, 0U),
                                            SyrecOperationDefinitionTestData("moduloWithKnownOperandValuesAndRhsOperandEqualToZero", 2U, syrec::BinaryExpression::BinaryOperation::Modulo, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("moduloWithLhsOperandValueUnknownAndRhsOperandKnownAndValueEqualToZero", std::nullopt, syrec::BinaryExpression::BinaryOperation::Modulo, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("moduloWithLhsOperandValueKnownAndEqualdToZeroAndRhsOperandValueUnknown", 0U, syrec::BinaryExpression::BinaryOperation::Modulo, std::nullopt, std::nullopt),

                                            SyrecOperationDefinitionTestData("fracDivideWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::FracDivide, 4U, 0U),
                                            SyrecOperationDefinitionTestData("fracDivideWithKnownOperandValuesAndLhsOperandEqualToZero", 0U, syrec::BinaryExpression::BinaryOperation::FracDivide, 4U, 0U),
                                            SyrecOperationDefinitionTestData("fracDivideWithKnownOperandValuesAndRhsOperandEqualToZero", 2U, syrec::BinaryExpression::BinaryOperation::FracDivide, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("fracDivideWithLhsOperandValueUnknownAndRhsOperandKnownAndValueEqualToZero", std::nullopt, syrec::BinaryExpression::BinaryOperation::FracDivide, 0U, std::nullopt),

                                            SyrecOperationDefinitionTestData("logicalAndWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::LogicalAnd, 4U, 1U),
                                            SyrecOperationDefinitionTestData("logicalOrWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::LogicalOr, 4U, 1U),

                                            SyrecOperationDefinitionTestData("bitwiseAndWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::BitwiseAnd, 4U, 0U),
                                            SyrecOperationDefinitionTestData("bitwiseOrWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::BitwiseOr, 4U, 6U),
                                            SyrecOperationDefinitionTestData("lessThanWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::LessThan, 4U, 1U),
                                            SyrecOperationDefinitionTestData("greaterThanWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::GreaterThan, 4U, 0U),
                                            SyrecOperationDefinitionTestData("equalsWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::Equals, 4U, 0U),
                                            SyrecOperationDefinitionTestData("NotEqualsWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::NotEquals, 4U, 1U),
                                            SyrecOperationDefinitionTestData("lessEqualsWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::LessEquals, 4U, 1U),
                                            SyrecOperationDefinitionTestData("greaterEqualsWithKnownOperandValues", 2U, syrec::BinaryExpression::BinaryOperation::GreaterEquals, 4U, 0U),

                                            SyrecOperationDefinitionTestData("leftShiftWithKnownOperandValues", 2U, syrec::ShiftExpression::ShiftOperation::Left, 4U, 32U),
                                            SyrecOperationDefinitionTestData("leftShiftWithToBeShiftedValueEqualToZeroAndShiftAmountUnknown", 0U, syrec::ShiftExpression::ShiftOperation::Left, std::nullopt, 0U),
                                            SyrecOperationDefinitionTestData("leftShiftWithToBeShiftedValueUnknownAndShiftAmountEqualToZero", std::nullopt, syrec::ShiftExpression::ShiftOperation::Left, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("leftShiftWithUnknownOperandValues", std::nullopt, syrec::ShiftExpression::ShiftOperation::Left, std::nullopt, std::nullopt),

                                            SyrecOperationDefinitionTestData("rightShiftWithKnownOperandValues", 32U, syrec::ShiftExpression::ShiftOperation::Right, 4U, 2U),
                                            SyrecOperationDefinitionTestData("rightShiftWithToBeShiftedValueEqualToZeroAndShiftAmountUnknown", 0U, syrec::ShiftExpression::ShiftOperation::Right, std::nullopt, 0U),
                                            SyrecOperationDefinitionTestData("rightShiftWithToBeShiftedValueUnknownAndShiftAmountEqualToZero", std::nullopt, syrec::ShiftExpression::ShiftOperation::Right, 0U, std::nullopt),
                                            SyrecOperationDefinitionTestData("rightShiftWithUnknownOperandValues", std::nullopt, syrec::ShiftExpression::ShiftOperation::Right, std::nullopt, std::nullopt)}),
                         [](const testing::TestParamInfo<SyrecOperationDefinitionTestData>& info) {
                             return info.param.testCaseName;
                         });

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantSmallerThanMaximumStorableInExpectedBitwidthUsingModuloOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, 4U, utils::IntegerConstantTruncationOperation::Modulo, 14U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantEqualOrLargerThanMaximumStorableInExpectedBitwidthUsingModuloOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(15U, 4U, utils::IntegerConstantTruncationOperation::Modulo, 0U);
    assertIntegerConstantTruncationResultMatchesExpectedOne(24U, 4U, utils::IntegerConstantTruncationOperation::Modulo, 9U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantWithExpectedBitwidthEqualToMaximumPossibleValueUsingModuloOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION, utils::IntegerConstantTruncationOperation::Modulo, 14U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantEqualToMaximumPossibleValueWithExpectedBitwidthEqualToMaximumPossibleValueUsingModuloOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(UINT_MAX, MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION, utils::IntegerConstantTruncationOperation::Modulo, 0U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantWithExpectedBitwidthLargerThanMaximumPossibleValueUsingModuloOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION + 1, utils::IntegerConstantTruncationOperation::Modulo, 14U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantWithExpectedBitwidthEqualToZeroUsingModuloOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, 0U, utils::IntegerConstantTruncationOperation::Modulo, 0U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantSmallerThanMaximumStorableInExpectedBitwidthUsingBitwiseAndOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, 4U, utils::IntegerConstantTruncationOperation::BitwiseAnd, 14U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantEqualOrLargerThanMaximumStorableInExpectedBitwidthUsingBitwiseAndOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(15U, 4U, utils::IntegerConstantTruncationOperation::BitwiseAnd, 15U);
    assertIntegerConstantTruncationResultMatchesExpectedOne(26U, 4U, utils::IntegerConstantTruncationOperation::BitwiseAnd, 10U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantWithExpectedBitwidthEqualToMaximumPossibleValueUsingBitwiseAndOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION, utils::IntegerConstantTruncationOperation::BitwiseAnd, 14U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantEqualToMaximumPossibleValueWithExpectedBitwidthEqualToMaximumPossibleValueUsingBitwiseAndOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(UINT_MAX, MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION, utils::IntegerConstantTruncationOperation::BitwiseAnd, UINT_MAX);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantWithExpectedBitwidthLargerThanMaximumPossibleValueUsingBitwiseAndOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, MAXIMUM_SUPPORTED_BITWIDTH_FOR_INTEGER_CONSTANT_TRUNCATION + 1, utils::IntegerConstantTruncationOperation::BitwiseAnd, 14U);
}

TEST(SyrecOperationEvaluationUtilsTests, TruncationOfIntegerConstantWithExpectedBitwidthEqualToZeroUsingBitwiseAndOperation) {
    assertIntegerConstantTruncationResultMatchesExpectedOne(14U, 0U, utils::IntegerConstantTruncationOperation::BitwiseAnd, 0U);
}
