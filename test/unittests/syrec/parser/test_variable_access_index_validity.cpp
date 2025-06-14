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
#include "core/syrec/number.hpp"
#include "core/syrec/variable.hpp"

#include <core/syrec/parser/utils/variable_access_index_check.hpp>
#include <cstddef>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace utils;

namespace {
    const std::string      DEFAULT_VARIABLE_IDENTIFIER = "varIdent";
    constexpr unsigned int DEFAULT_SIGNAL_BITWIDTH     = 16;

    syrec::Variable::ptr generateVariableInstance(const std::string& variableIdentifier, const std::vector<unsigned int>& variableDimensions, unsigned int variableBitwidth) {
        return std::make_shared<syrec::Variable>(syrec::Variable::Type::In, variableIdentifier, variableDimensions, variableBitwidth);
    }

    syrec::Expression::ptr createExprForAccessOnValueOfDimensionUsingConstantValue(unsigned int value) {
        return std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(value), 0);
    }

    syrec::Number::ptr createContainerForBitrangeAccessComponent(unsigned int value) {
        return std::make_shared<syrec::Number>(value);
    }

    VariableAccessIndicesValidity buildExpectedResult(const std::initializer_list<VariableAccessIndicesValidity::IndexValidationResult>& expectedValidityPerDimension, const std::optional<VariableAccessIndicesValidity::BitRangeValidityResult>& expectedBitrangeValidity) {
        VariableAccessIndicesValidity result;
        result.accessedValuePerDimensionValidity.reserve(expectedValidityPerDimension.size());

        for (const auto& expectedValidityOfDimension: expectedValidityPerDimension) {
            result.accessedValuePerDimensionValidity.emplace_back(expectedValidityOfDimension);
        }
        result.bitRangeAccessValidity = expectedBitrangeValidity;
        return result;
    }

    VariableAccessIndicesValidity::IndexValidationResult createValidIndexValidationResult(unsigned int value) {
        return VariableAccessIndicesValidity::IndexValidationResult(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Ok, value);
    }

    VariableAccessIndicesValidity::IndexValidationResult createUnknownIndexValidationResult(std::optional<unsigned int> accessedValueOfDimension) {
        return VariableAccessIndicesValidity::IndexValidationResult(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Unknown, accessedValueOfDimension);
    }

    VariableAccessIndicesValidity::IndexValidationResult createOutOfRangeIndexValidationResult(unsigned int value) {
        return VariableAccessIndicesValidity::IndexValidationResult(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange, value);
    }

    void assertIndexValidiationResultsMatch(const VariableAccessIndicesValidity::IndexValidationResult& expected, const VariableAccessIndicesValidity::IndexValidationResult& actual) {
        ASSERT_EQ(expected.indexValidity, actual.indexValidity);
        ASSERT_EQ(expected.indexValue.has_value(), actual.indexValue.has_value());
        if (expected.indexValue.has_value()) {
            ASSERT_EQ(expected.indexValue.value(), actual.indexValue.value());
        }
    }

    void assertValidationResultsMatch(const std::optional<VariableAccessIndicesValidity>& expectedValidationResult, const std::optional<VariableAccessIndicesValidity>& actualValidationResult) {
        ASSERT_EQ(expectedValidationResult.has_value(), actualValidationResult.has_value());
        if (!expectedValidationResult.has_value()) {
            return;
        }

        ASSERT_EQ(expectedValidationResult->isValid(), actualValidationResult->isValid());
        const auto& expectedIndexValidityPerValueOfDimension = expectedValidationResult->accessedValuePerDimensionValidity;
        const auto& actualIndexValidityPerValueOfDimension   = actualValidationResult->accessedValuePerDimensionValidity;
        ASSERT_EQ(expectedIndexValidityPerValueOfDimension.size(), actualIndexValidityPerValueOfDimension.size());

        for (std::size_t i = 0; i < expectedIndexValidityPerValueOfDimension.size(); ++i) {
            ASSERT_NO_FATAL_FAILURE(assertIndexValidiationResultsMatch(expectedIndexValidityPerValueOfDimension.at(i), actualIndexValidityPerValueOfDimension.at(i)));
        }

        ASSERT_EQ(expectedValidationResult->bitRangeAccessValidity.has_value(), actualValidationResult->bitRangeAccessValidity.has_value());
        if (!expectedValidationResult->bitRangeAccessValidity.has_value()) {
            return;
        }
        const auto& expectedBitRangeValidity = expectedValidationResult->bitRangeAccessValidity.value();
        const auto& actualBitRangeValidity   = actualValidationResult->bitRangeAccessValidity.value();

        ASSERT_NO_FATAL_FAILURE(assertIndexValidiationResultsMatch(expectedBitRangeValidity.bitRangeStartValidity, actualBitRangeValidity.bitRangeStartValidity));
        ASSERT_NO_FATAL_FAILURE(assertIndexValidiationResultsMatch(expectedBitRangeValidity.bitRangeEndValidity, actualBitRangeValidity.bitRangeEndValidity));
    }

    struct VariableDefinition {
        std::string               variableIdentifier;
        std::vector<unsigned int> variableDimensions;
        unsigned int              variableBitwidth;
    };

    struct VariableAccessData {
        std::vector<unsigned int>                            accessedValueOfDimension;
        std::optional<std::pair<unsigned int, unsigned int>> accessedBitRange;
    };

    struct VariableAccessTestData {
        std::string        testCaseName;
        VariableDefinition variableDefinition;
        VariableAccessData variableAccessData;

        VariableAccessTestData(std::string testCaseName, VariableDefinition&& variableDefinition, VariableAccessData&& variableAccessData):
            testCaseName(std::move(testCaseName)), variableDefinition(std::move(variableDefinition)), variableAccessData(std::move(variableAccessData)) {}
    };

    class VariableAccessSuccessTestFixture: public testing::TestWithParam<VariableAccessTestData> {
    };
} // namespace

TEST_P(VariableAccessSuccessTestFixture, SuccessCase) {
    const VariableDefinition& variableDefinition = GetParam().variableDefinition;
    const VariableAccessData& variableAccessData = GetParam().variableAccessData;

    const auto variableInstance = generateVariableInstance(variableDefinition.variableIdentifier, variableDefinition.variableDimensions, variableDefinition.variableBitwidth);
    auto       variableAccess   = syrec::VariableAccess();
    variableAccess.var          = variableInstance;

    variableAccess.indexes.reserve(variableAccessData.accessedValueOfDimension.size());
    for (const auto accessedValueOfDimension: variableAccessData.accessedValueOfDimension) {
        variableAccess.indexes.emplace_back(createExprForAccessOnValueOfDimensionUsingConstantValue(accessedValueOfDimension));
    }

    if (variableAccessData.accessedBitRange.has_value()) {
        const auto [bitRangeStart, bitRangeEnd] = variableAccessData.accessedBitRange.value();
        if (bitRangeStart == bitRangeEnd) {
            const auto containerForBitAccess = createContainerForBitrangeAccessComponent(bitRangeStart);
            variableAccess.range             = std::make_pair(containerForBitAccess, containerForBitAccess);
        } else {
            variableAccess.range = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStart), createContainerForBitrangeAccessComponent(bitRangeEnd));
        }
    }

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));

    auto expectedValidationResult = VariableAccessIndicesValidity();
    for (const auto accessedValueOfDimension: variableAccessData.accessedValueOfDimension) {
        expectedValidationResult.accessedValuePerDimensionValidity.emplace_back(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Ok, accessedValueOfDimension);
    }

    if (variableAccessData.accessedBitRange.has_value()) {
        expectedValidationResult.bitRangeAccessValidity = VariableAccessIndicesValidity::BitRangeValidityResult({createValidIndexValidationResult(variableAccessData.accessedBitRange->first), createValidIndexValidationResult(variableAccessData.accessedBitRange->second)});
    }
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

INSTANTIATE_TEST_SUITE_P(VariableAccessIndexValidityTests, VariableAccessSuccessTestFixture,
                         testing::ValuesIn({VariableAccessTestData({"AccessOnSomeDimensionsInRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1}, std::nullopt})}),
                                            VariableAccessTestData({"AccessOnNoDimensionsOf1DimensionalSignalOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {2}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{}, std::nullopt})}),
                                            VariableAccessTestData({"AccessOnNoDimensionsOfNDimensionalSignalOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{}, std::nullopt})}),
                                            VariableAccessTestData({"AccessOnOneValuePerDimensionOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {2, 1, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{1, 0, 2}, std::nullopt})}),
                                            VariableAccessTestData({"AccessOnBitWithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0}, std::make_pair(4U, 4U)})}),
                                            VariableAccessTestData({"AccessOnBitWithinRangeAtStartOfSignalOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0}, std::make_pair(0, 0)})}),
                                            VariableAccessTestData({"AccessOnBitWithinRangeAtEndOfSignalOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0}, std::make_pair(DEFAULT_SIGNAL_BITWIDTH - 1, DEFAULT_SIGNAL_BITWIDTH - 1)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartSmallerThanEnd_StartAtInitialPositionEndSmallerThanEnd_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(0, DEFAULT_SIGNAL_BITWIDTH - 4)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartSmallerThanEnd_StartAtInitialPositionEndAtEnd_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(0, DEFAULT_SIGNAL_BITWIDTH - 1)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartSmallerThanEnd_StartAtIntermediatePositionEndSmallerThanEnd_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(4, DEFAULT_SIGNAL_BITWIDTH - 4)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartSmallerThanEnd_StartAtIntermediatePositionEndAtEnd_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(4, DEFAULT_SIGNAL_BITWIDTH - 1)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartLargerThanEnd_StartAtEndPositionEndAtIntermediatePosition_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(DEFAULT_SIGNAL_BITWIDTH - 1, DEFAULT_SIGNAL_BITWIDTH - 4)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartLargerThanEnd_StartAtEndPositionEndAtStart_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(DEFAULT_SIGNAL_BITWIDTH - 1, 0)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartLargerThanEnd_StartAtIntermediatePositionEndAtIntermediatePosition_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(DEFAULT_SIGNAL_BITWIDTH - 4, 4)})}),
                                            VariableAccessTestData({"AccessOnBitRangeWithStartLargerThanEnd_StartAtIntermediatePositionEndAtStart_WithinRangeOk",
                                                                    VariableDefinition({DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH}),
                                                                    VariableAccessData({{0, 1, 2}, std::make_pair(DEFAULT_SIGNAL_BITWIDTH - 4, 0)})})}),
                         [](const testing::TestParamInfo<VariableAccessSuccessTestFixture::ParamType>& info) {
                             return info.param.testCaseName;
                         });

// Error cases
TEST(VariableAccessIndexValidityTests, InvalidReferenceToVariableDoesNotCrashCheck) {
    auto variableAccess    = syrec::VariableAccess();
    variableAccess.indexes = {
            createExprForAccessOnValueOfDimensionUsingConstantValue(0),
            createExprForAccessOnValueOfDimensionUsingConstantValue(1)};
    variableAccess.range = std::make_pair(createContainerForBitrangeAccessComponent(0), createContainerForBitrangeAccessComponent(2));

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(std::nullopt, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, AccessOnValueOfDimensionDefinedViaExpressionIsNotValid) {
    const auto                          variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1, 2, 3}, DEFAULT_SIGNAL_BITWIDTH);
    std::vector<syrec::Expression::ptr> nonConstantValueExpressionsForAccessOnDimension;

    const std::string loopVariableIdentifier = "$test";
    const auto        loopVariableInstance   = std::make_shared<syrec::Number>(loopVariableIdentifier);
    nonConstantValueExpressionsForAccessOnDimension.emplace_back(std::make_shared<syrec::NumericExpression>(loopVariableInstance, 2));
    nonConstantValueExpressionsForAccessOnDimension.emplace_back(std::make_shared<syrec::ShiftExpression>(createExprForAccessOnValueOfDimensionUsingConstantValue(2), syrec::ShiftExpression::ShiftOperation::Left, std::make_shared<syrec::Number>(1)));
    nonConstantValueExpressionsForAccessOnDimension.emplace_back(std::make_shared<syrec::BinaryExpression>(nonConstantValueExpressionsForAccessOnDimension.front(), syrec::BinaryExpression::BinaryOperation::Add, std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(1), 2)));

    constexpr unsigned int accessedValueForFirstDimension = 0;
    auto                   variableAccess                 = syrec::VariableAccess();
    variableAccess.var                                    = variableInstance;
    variableAccess.indexes.emplace_back(createExprForAccessOnValueOfDimensionUsingConstantValue(accessedValueForFirstDimension));
    variableAccess.indexes.emplace_back(nullptr);

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    VariableAccessIndicesValidity                expectedValidationResult;
    expectedValidationResult.accessedValuePerDimensionValidity.emplace_back(createValidIndexValidationResult(accessedValueForFirstDimension));
    expectedValidationResult.accessedValuePerDimensionValidity.emplace_back(createUnknownIndexValidationResult(std::nullopt));

    for (const auto& nonConstantValueExpressionForAccessOnDimension: nonConstantValueExpressionsForAccessOnDimension) {
        variableAccess.indexes.at(1) = nonConstantValueExpressionForAccessOnDimension;

        ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
        ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
    }
}

TEST(VariableAccessIndexValidityTests, AccessedValueOfDimensionOutOfRangeIsNotValid) {
    const std::vector<unsigned int> numberOfValuesPerDimension  = {1, 2, 3};
    const auto                      variableInstance            = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, numberOfValuesPerDimension, DEFAULT_SIGNAL_BITWIDTH);
    const std::vector               outOfRangeValuePerDimension = {
            numberOfValuesPerDimension.at(0) + 1,
            numberOfValuesPerDimension.at(1) + 10,
            numberOfValuesPerDimension.at(2) + 3};
    ASSERT_EQ(numberOfValuesPerDimension.size(), outOfRangeValuePerDimension.size());

    auto variableAccess    = syrec::VariableAccess();
    variableAccess.var     = variableInstance;
    variableAccess.indexes = std::vector<syrec::Expression::ptr>(outOfRangeValuePerDimension.size(), nullptr);

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    VariableAccessIndicesValidity                expectedValidationResult;
    expectedValidationResult.accessedValuePerDimensionValidity = std::vector(outOfRangeValuePerDimension.size(), createUnknownIndexValidationResult(std::nullopt));

    for (std::size_t i = 0; i < outOfRangeValuePerDimension.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            variableAccess.indexes.at(j)                                  = createExprForAccessOnValueOfDimensionUsingConstantValue(numberOfValuesPerDimension.at(j) - 1);
            expectedValidationResult.accessedValuePerDimensionValidity[j] = createValidIndexValidationResult(numberOfValuesPerDimension.at(j) - 1);
        }

        expectedValidationResult.accessedValuePerDimensionValidity[i] = createOutOfRangeIndexValidationResult(outOfRangeValuePerDimension.at(i));
        variableAccess.indexes.at(i)                                  = createExprForAccessOnValueOfDimensionUsingConstantValue(outOfRangeValuePerDimension.at(i));

        for (std::size_t k = i + 1; k < outOfRangeValuePerDimension.size(); ++k) {
            variableAccess.indexes.at(k)                                  = createExprForAccessOnValueOfDimensionUsingConstantValue(numberOfValuesPerDimension.at(k) - 1);
            expectedValidationResult.accessedValuePerDimensionValidity[k] = createValidIndexValidationResult(numberOfValuesPerDimension.at(k) - 1);
        }

        ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
        ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
    }
}

TEST(VariableAccessIndexValidityTests, AccessedValueOfDimensionOutOfRangeWithValueOfPriorDimensionUnknownIsNotValid) {
    const std::vector<unsigned int> numberOfValuesPerDimension = {1, 2, 3};
    const auto                      variableInstance           = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, numberOfValuesPerDimension, DEFAULT_SIGNAL_BITWIDTH);

    const std::string  loopVariableIdentifier = "$k";
    const unsigned int outOfRangeBit          = variableInstance->bitwidth + 2;

    auto variableAccess       = syrec::VariableAccess();
    variableAccess.var        = variableInstance;
    variableAccess.indexes    = syrec::Expression::vec(numberOfValuesPerDimension.size(), nullptr);
    variableAccess.indexes[0] = std::make_shared<syrec::NumericExpression>(std::make_shared<syrec::Number>(loopVariableIdentifier), 2);
    variableAccess.indexes[1] = createExprForAccessOnValueOfDimensionUsingConstantValue(outOfRangeBit);
    variableAccess.indexes[2] = createExprForAccessOnValueOfDimensionUsingConstantValue(0);

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult({createUnknownIndexValidationResult(std::nullopt),
                                                                                  createOutOfRangeIndexValidationResult(outOfRangeBit),
                                                                                  createValidIndexValidationResult(0)},
                                                                                 std::nullopt);

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, AccessedValueForMultipleDimensionsOutOfRangeIsNotValid) {
    const std::vector<unsigned int> numberOfValuesPerDimension         = {1, 2, 3};
    const auto                      variableInstance                   = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, numberOfValuesPerDimension, DEFAULT_SIGNAL_BITWIDTH);
    const unsigned int              outOfRangeValueForFirstDimension   = numberOfValuesPerDimension.front();
    const unsigned int              validIndexForIntermediateDimension = numberOfValuesPerDimension.at(1) - 1;
    const unsigned int              outOfRangeValueForLastDimension    = numberOfValuesPerDimension.back() + 1;

    auto variableAccess       = syrec::VariableAccess();
    variableAccess.var        = variableInstance;
    variableAccess.indexes    = syrec::Expression::vec(numberOfValuesPerDimension.size(), nullptr);
    variableAccess.indexes[0] = createExprForAccessOnValueOfDimensionUsingConstantValue(outOfRangeValueForFirstDimension);
    variableAccess.indexes[1] = createExprForAccessOnValueOfDimensionUsingConstantValue(validIndexForIntermediateDimension);
    variableAccess.indexes[2] = createExprForAccessOnValueOfDimensionUsingConstantValue(outOfRangeValueForLastDimension);

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult({createOutOfRangeIndexValidationResult(outOfRangeValueForFirstDimension),
                                                                                  createValidIndexValidationResult(validIndexForIntermediateDimension),
                                                                                  createOutOfRangeIndexValidationResult(outOfRangeValueForLastDimension)},
                                                                                 std::nullopt);

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, AccessOnMoreDimensionsThanDeclaredVariableIsNotValid) {
    const std::vector<unsigned int> numberOfValuesPerDimension = {1, 2, 3};
    const auto                      variableInstance           = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, numberOfValuesPerDimension, DEFAULT_SIGNAL_BITWIDTH);

    const std::vector<unsigned int> userDefinedAccessedValuePerDimension = {0, 0, 2, 1, 2};
    auto                            variableAccess                       = syrec::VariableAccess();
    variableAccess.var                                                   = variableInstance;
    variableAccess.indexes                                               = syrec::Expression::vec(userDefinedAccessedValuePerDimension.size(), nullptr);

    VariableAccessIndicesValidity expectedValidationResult;
    expectedValidationResult.accessedValuePerDimensionValidity = std::vector(userDefinedAccessedValuePerDimension.size(), createUnknownIndexValidationResult(std::nullopt));

    for (std::size_t dimensionIdx = 0; dimensionIdx < userDefinedAccessedValuePerDimension.size(); ++dimensionIdx) {
        variableAccess.indexes[dimensionIdx]                                     = createExprForAccessOnValueOfDimensionUsingConstantValue(userDefinedAccessedValuePerDimension[dimensionIdx]);
        expectedValidationResult.accessedValuePerDimensionValidity[dimensionIdx] = dimensionIdx < numberOfValuesPerDimension.size() ? createValidIndexValidationResult(userDefinedAccessedValuePerDimension[dimensionIdx]) : createUnknownIndexValidationResult(userDefinedAccessedValuePerDimension[dimensionIdx]);
    }

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, InvalidValueForValueOfDimensionIsNotValid) {
    const std::vector<unsigned int> numberOfValuesPerDimension = {1, 2, 3};
    const auto                      variableInstance           = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, numberOfValuesPerDimension, DEFAULT_SIGNAL_BITWIDTH);

    auto variableAccess       = syrec::VariableAccess();
    variableAccess.var        = variableInstance;
    variableAccess.indexes    = syrec::Expression::vec(numberOfValuesPerDimension.size(), nullptr);
    variableAccess.indexes[0] = createExprForAccessOnValueOfDimensionUsingConstantValue(numberOfValuesPerDimension.front() - 1);
    variableAccess.indexes[1] = nullptr;
    variableAccess.indexes[2] = createExprForAccessOnValueOfDimensionUsingConstantValue(numberOfValuesPerDimension.back() - 1);

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult({createValidIndexValidationResult(numberOfValuesPerDimension.front() - 1),
                                                                                  createUnknownIndexValidationResult(std::nullopt),
                                                                                  createValidIndexValidationResult(numberOfValuesPerDimension.back() - 1)},
                                                                                 std::nullopt);

    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitrangeStartOutOfRangeIsNotValid) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int bitRangeStartBit = variableBitwidth + 1;
    constexpr unsigned int bitRangeEndBit   = bitRangeStartBit + 2;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};
    variableAccess.range                    = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStartBit), createContainerForBitrangeAccessComponent(bitRangeEndBit));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createOutOfRangeIndexValidationResult(bitRangeStartBit), createOutOfRangeIndexValidationResult(bitRangeEndBit)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, UnknownValueForBitrangeStartIsNotValid) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    const std::string      loopVariableIdentifier = "$i";
    constexpr unsigned int bitRangeEndBit         = variableBitwidth - 2;
    variableAccess.var                            = variableInstance;
    variableAccess.indexes                        = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};
    variableAccess.range                          = std::make_pair(std::make_shared<syrec::Number>(loopVariableIdentifier), createContainerForBitrangeAccessComponent(bitRangeEndBit));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createUnknownIndexValidationResult(std::nullopt), createValidIndexValidationResult(bitRangeEndBit)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitrangeEndOutOfRangeIsNotValid) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int bitRangeStartBit = 2;
    constexpr unsigned int bitRangeEndBit   = variableBitwidth + 2;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};
    variableAccess.range                    = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStartBit), createContainerForBitrangeAccessComponent(bitRangeEndBit));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createValidIndexValidationResult(bitRangeStartBit), createOutOfRangeIndexValidationResult(bitRangeEndBit)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, UnknownValueForBitrangeEndIsNotValid) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    const std::string      loopVariableIdentifier = "$i";
    constexpr unsigned int bitRangeStartBit       = 2;
    variableAccess.var                            = variableInstance;
    variableAccess.indexes                        = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};
    variableAccess.range                          = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStartBit), std::make_shared<syrec::Number>(loopVariableIdentifier));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createValidIndexValidationResult(bitRangeStartBit), createUnknownIndexValidationResult(std::nullopt)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitrangeStartOutOfRangeWithStartLargerThanEndIsNotValid) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int bitRangeStartBit = variableBitwidth + 2;
    constexpr unsigned int bitRangeEndBit   = 2;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};
    variableAccess.range                    = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStartBit), createContainerForBitrangeAccessComponent(bitRangeEndBit));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createOutOfRangeIndexValidationResult(bitRangeStartBit), createValidIndexValidationResult(bitRangeEndBit)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitrangeStartEqualToSignalBitwidthOutOfRangeIfValueOfEndIndexIsUnknown) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int bitRangeStartBit = variableBitwidth;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};

    const std::string loopVariableIdentifier              = "$j";
    const auto        containerForUnknownBitrangeEndIndex = std::make_shared<syrec::Number>(loopVariableIdentifier);
    variableAccess.range                                  = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStartBit), containerForUnknownBitrangeEndIndex);

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createOutOfRangeIndexValidationResult(bitRangeStartBit), createUnknownIndexValidationResult(std::nullopt)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitrangeEndEqualToSignalBitwidthOutOfRangeIfValueOfStartIndexIsUnknown) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int bitRangeStartEnd = variableBitwidth;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};

    const std::string loopVariableIdentifier                = "$j";
    const auto        containerForUnknownBitrangeStartIndex = std::make_shared<syrec::Number>(loopVariableIdentifier);
    variableAccess.range                                    = std::make_pair(containerForUnknownBitrangeStartIndex, createContainerForBitrangeAccessComponent(bitRangeStartEnd));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createUnknownIndexValidationResult(std::nullopt), createOutOfRangeIndexValidationResult(bitRangeStartEnd)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitrangeStartEqualToSignalBitwidthOutOfRangeIfValueOfEndIndexIsKnown) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int bitRangeStartBit = variableBitwidth;
    constexpr unsigned int bitRangeEndBit   = 2;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};
    variableAccess.range                    = std::make_pair(createContainerForBitrangeAccessComponent(bitRangeStartBit), createContainerForBitrangeAccessComponent(bitRangeEndBit));

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createOutOfRangeIndexValidationResult(bitRangeStartBit), createValidIndexValidationResult(bitRangeEndBit)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, BitIndexValueEqualToSignalBitwidthIsOutOfRange) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {1}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int accessedBitIndex = variableBitwidth;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(0)};

    const auto containerForAccessedBitIndex = createContainerForBitrangeAccessComponent(accessedBitIndex);
    variableAccess.range                    = std::make_pair(containerForAccessedBitIndex, containerForAccessedBitIndex);

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createValidIndexValidationResult(0)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createOutOfRangeIndexValidationResult(accessedBitIndex), createOutOfRangeIndexValidationResult(accessedBitIndex)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}

TEST(VariableAccessIndexValidityTests, AccessedValueOfDimensionEqualToNumberOfValuesOfDimensionIsOutOfRange) {
    constexpr unsigned int variableBitwidth = DEFAULT_SIGNAL_BITWIDTH;
    const auto             variableInstance = generateVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {2}, variableBitwidth);
    auto                   variableAccess   = syrec::VariableAccess();

    constexpr unsigned int accessedBitIndex = 1;
    variableAccess.var                      = variableInstance;
    variableAccess.indexes                  = {createExprForAccessOnValueOfDimensionUsingConstantValue(2)};

    const auto containerForAccessedBitIndex = createContainerForBitrangeAccessComponent(accessedBitIndex);
    variableAccess.range                    = std::make_pair(containerForAccessedBitIndex, containerForAccessedBitIndex);

    VariableAccessIndicesValidity expectedValidationResult = buildExpectedResult(
            {createOutOfRangeIndexValidationResult(2)},
            VariableAccessIndicesValidity::BitRangeValidityResult({createValidIndexValidationResult(accessedBitIndex), createValidIndexValidationResult(accessedBitIndex)}));
    std::optional<VariableAccessIndicesValidity> actualValidationResult;
    ASSERT_NO_FATAL_FAILURE(actualValidationResult = validateVariableAccessIndices(variableAccess));
    ASSERT_NO_FATAL_FAILURE(assertValidationResultsMatch(expectedValidationResult, actualValidationResult));
}
