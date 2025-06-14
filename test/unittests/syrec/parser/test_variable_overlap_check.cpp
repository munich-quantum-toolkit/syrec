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

#include <core/syrec/parser/utils/variable_overlap_check.hpp>
#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// NOTE: The tests defined in this file assume that the variable accesses compared are accessing the same number of dimensions while the implemented functionality
// also supports checks between 'partial' variable accesses. Additionally, currently only loop variables are used to 'mock' non-constant indices in the accessed value of a dimension
// or bitrange component. Future tests should also test the correct behaviour using syrec::Expressions.

namespace {
    const std::string      DEFAULT_VARIABLE_IDENTIFIER = "varIdent";
    constexpr unsigned int DEFAULT_VARIABLE_BITWIDTH   = 16;

    syrec::Number::ptr createNumberContainerForConstantValue(unsigned int value) {
        return std::make_shared<syrec::Number>(value);
    }

    syrec::Number::ptr createNumberContainerForLoopVariableIdentifier(const std::string& loopVariableIdentifier) {
        return std::make_shared<syrec::Number>(loopVariableIdentifier);
    }

    syrec::Expression::ptr createExpressionForConstantValue(unsigned int value) {
        const auto containerForConstantValue = createNumberContainerForConstantValue(value);
        return std::make_shared<syrec::NumericExpression>(containerForConstantValue, 2U);
    }

    syrec::Expression::ptr createExpressionForLoopVariableIdentifier(const std::string& loopVariableIdentifier) {
        return std::make_shared<syrec::NumericExpression>(createNumberContainerForLoopVariableIdentifier(loopVariableIdentifier), 1);
    }

    syrec::Variable::ptr createVariableInstance(const std::string& variableIdentifier, const std::vector<unsigned int>& variableDimensions, unsigned int variableBitwidth) {
        return std::make_shared<syrec::Variable>(syrec::Variable::Type::In, variableIdentifier, variableDimensions, variableBitwidth);
    }

    void assertOverlapDataMatches(const std::optional<utils::VariableAccessOverlapCheckResult::OverlappingIndicesContainer>& expectedOverlapData, const std::optional<utils::VariableAccessOverlapCheckResult::OverlappingIndicesContainer>& actualOverlapData) {
        if (!expectedOverlapData.has_value()) {
            ASSERT_FALSE(actualOverlapData.has_value()) << "Expected that no overlap data is available";
            return;
        }

        ASSERT_TRUE(actualOverlapData.has_value()) << "Expected that actual overlap data is available";
        ASSERT_THAT(actualOverlapData->knownValueOfAccessedValuePerDimension, testing::ElementsAreArray(expectedOverlapData->knownValueOfAccessedValuePerDimension));
        ASSERT_EQ(expectedOverlapData->overlappingBit, actualOverlapData->overlappingBit);
    }

    void assertEquivalenceBetweenVariableAccessOverlapOperands(const syrec::VariableAccess& lhsOperand, const syrec::VariableAccess& rhsOperand, const utils::VariableAccessOverlapCheckResult& expectedVariableAccessOverlapCheckResult) {
        std::optional<utils::VariableAccessOverlapCheckResult> actualVariableAccessOverlapCheckResult;
        ASSERT_NO_FATAL_FAILURE(actualVariableAccessOverlapCheckResult = utils::checkOverlapBetweenVariableAccesses(lhsOperand, rhsOperand));
        ASSERT_TRUE(actualVariableAccessOverlapCheckResult.has_value());
        ASSERT_EQ(expectedVariableAccessOverlapCheckResult.overlapState, actualVariableAccessOverlapCheckResult->overlapState);
        ASSERT_NO_FATAL_FAILURE(assertOverlapDataMatches(expectedVariableAccessOverlapCheckResult.overlappingIndicesInformation, actualVariableAccessOverlapCheckResult->overlappingIndicesInformation));
    }

    void assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(const syrec::VariableAccess& lhsOperand, const syrec::VariableAccess& rhsOperand) {
        std::optional<utils::VariableAccessOverlapCheckResult> actualVariableAccessOverlapCheckResult;
        ASSERT_NO_FATAL_FAILURE(actualVariableAccessOverlapCheckResult = utils::checkOverlapBetweenVariableAccesses(lhsOperand, rhsOperand));
        ASSERT_TRUE(actualVariableAccessOverlapCheckResult.has_value());
        ASSERT_EQ(utils::VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping, actualVariableAccessOverlapCheckResult->overlapState);
        ASSERT_FALSE(actualVariableAccessOverlapCheckResult->overlappingIndicesInformation.has_value());

        ASSERT_NO_FATAL_FAILURE(actualVariableAccessOverlapCheckResult = utils::checkOverlapBetweenVariableAccesses(rhsOperand, lhsOperand));
        ASSERT_TRUE(actualVariableAccessOverlapCheckResult.has_value());
        ASSERT_EQ(utils::VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping, actualVariableAccessOverlapCheckResult->overlapState);
        ASSERT_FALSE(actualVariableAccessOverlapCheckResult->overlappingIndicesInformation.has_value());
    }

    void assertSymmetricVariableAccessOverlapResultCannotBeDetermined(const syrec::VariableAccess& lhsOperand, const syrec::VariableAccess& rhsOperand) {
        std::optional<utils::VariableAccessOverlapCheckResult> actualVariableAccessOverlapCheckResult;
        ASSERT_NO_FATAL_FAILURE(actualVariableAccessOverlapCheckResult = utils::checkOverlapBetweenVariableAccesses(lhsOperand, rhsOperand));
        ASSERT_FALSE(actualVariableAccessOverlapCheckResult.has_value());

        // Operation must be symmmetric (a OP b) = (b OP a)
        ASSERT_NO_FATAL_FAILURE(actualVariableAccessOverlapCheckResult = utils::checkOverlapBetweenVariableAccesses(lhsOperand, rhsOperand));
        ASSERT_FALSE(actualVariableAccessOverlapCheckResult.has_value());
    }

    struct VariableDefinition {
        std::string               identifier;
        std::vector<unsigned int> dimensions;
        unsigned int              bitwidth;

        VariableDefinition(std::string variableIdentifier, std::vector<unsigned int>&& variableDimensions, unsigned int bitwidth):
            identifier(std::move(variableIdentifier)), dimensions(std::move(variableDimensions)), bitwidth(bitwidth) {}
    };

    struct VariableAccessDefinition {
        using IndexDataVariant = std::variant<unsigned int, std::string>;

        std::vector<IndexDataVariant>                                    accessedValuePerDimension;
        std::optional<std::pair<syrec::Number::ptr, syrec::Number::ptr>> accessedBitRange;

        [[nodiscard]] static syrec::Number::ptr createNumberContainerForBitrangeIndexDataVariant(const IndexDataVariant& bitRangeIndexData) {
            if (std::holds_alternative<unsigned int>(bitRangeIndexData)) {
                return createNumberContainerForConstantValue(std::get<unsigned int>(bitRangeIndexData));
            }
            return createNumberContainerForLoopVariableIdentifier(std::get<std::string>(bitRangeIndexData));
        }

        explicit VariableAccessDefinition():
            accessedValuePerDimension({}), accessedBitRange(std::nullopt) {}

        explicit VariableAccessDefinition(const IndexDataVariant& accessedBit):
            accessedValuePerDimension({}) {
            const auto containerForAccessedBit = createNumberContainerForBitrangeIndexDataVariant(accessedBit);
            accessedBitRange                   = std::make_pair(containerForAccessedBit, containerForAccessedBit);
        }

        explicit VariableAccessDefinition(const std::pair<IndexDataVariant, IndexDataVariant>& accessedBitRange):
            accessedValuePerDimension({}) {
            this->accessedBitRange = std::make_pair(createNumberContainerForBitrangeIndexDataVariant(accessedBitRange.first), createNumberContainerForBitrangeIndexDataVariant(accessedBitRange.second));
        }

        explicit VariableAccessDefinition(const std::vector<IndexDataVariant>& accessedValuePerDimension):
            accessedValuePerDimension(accessedValuePerDimension), accessedBitRange(std::nullopt) {}

        explicit VariableAccessDefinition(const std::vector<IndexDataVariant>& accessedValuePerDimension, const IndexDataVariant& accessedBit):
            accessedValuePerDimension(accessedValuePerDimension) {
            const auto containerForAccessedBit = createNumberContainerForBitrangeIndexDataVariant(accessedBit);
            accessedBitRange                   = std::make_pair(containerForAccessedBit, containerForAccessedBit);
        }

        explicit VariableAccessDefinition(const std::vector<IndexDataVariant>& accessedValuePerDimension, const std::pair<IndexDataVariant, IndexDataVariant>& accessedBitRange):
            accessedValuePerDimension(accessedValuePerDimension) {
            this->accessedBitRange = std::make_pair(createNumberContainerForBitrangeIndexDataVariant(accessedBitRange.first), createNumberContainerForBitrangeIndexDataVariant(accessedBitRange.second));
        }
    };

    struct ExpectedSymmetricOverlapCheckResultData {
        std::vector<unsigned int> accessedValuePerDimension;
        unsigned int              overlappingBitForLhsOperand;
        unsigned int              overlappingBitForRhsOperand;

        explicit ExpectedSymmetricOverlapCheckResultData(std::vector<unsigned int>&& accessedValuePerDimension, unsigned int overlappingBitForLhsOperand):
            accessedValuePerDimension(std::move(accessedValuePerDimension)), overlappingBitForLhsOperand(overlappingBitForLhsOperand), overlappingBitForRhsOperand(overlappingBitForLhsOperand) {}

        explicit ExpectedSymmetricOverlapCheckResultData(std::vector<unsigned int>&& accessedValuePerDimension, unsigned int overlappingBitForLhsOperand, unsigned int overlappingBitForRhsOperand):
            accessedValuePerDimension(std::move(accessedValuePerDimension)), overlappingBitForLhsOperand(overlappingBitForLhsOperand), overlappingBitForRhsOperand(overlappingBitForRhsOperand) {}
    };

    struct OverlapTestData {
        std::string                                            testName;
        VariableDefinition                                     variableDefinition;
        VariableAccessDefinition                               lVarAccessDefinition;
        VariableAccessDefinition                               rVarAccessDefinition;
        std::optional<ExpectedSymmetricOverlapCheckResultData> optionalExpectedOverlapData;

        explicit OverlapTestData(std::string&& testName, VariableDefinition&& variableDefinition, VariableAccessDefinition&& lVarAccessDefinition, VariableAccessDefinition&& rVarAccessDefinition):
            testName(std::move(testName)), variableDefinition(std::move(variableDefinition)), lVarAccessDefinition(std::move(lVarAccessDefinition)), rVarAccessDefinition(std::move(rVarAccessDefinition)) {}

        explicit OverlapTestData(std::string&& testName, VariableDefinition&& variableDefinition, VariableAccessDefinition&& lVarAccessDefinition, VariableAccessDefinition&& rVarAccessDefinition, ExpectedSymmetricOverlapCheckResultData&& overlapData):
            testName(std::move(testName)), variableDefinition(std::move(variableDefinition)), lVarAccessDefinition(std::move(lVarAccessDefinition)), rVarAccessDefinition(std::move(rVarAccessDefinition)), optionalExpectedOverlapData(std::move(overlapData)) {}
    };

    class BaseOverlapInVariableAccessesTestFixture: public testing::TestWithParam<OverlapTestData> {
    public:
        virtual utils::VariableAccessOverlapCheckResult::OverlapState getExpectedVariableAccessOverlapCheckResult() = 0;

        void performTestExecution() {
            const auto& [_, variableDefinition, lVarAccessDefinition, rVarAccessDefinition, optionalExpectedOverlapData] = GetParam();
            const auto variableInstance                                                                                  = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, variableDefinition.identifier, variableDefinition.dimensions, variableDefinition.bitwidth);

            const syrec::VariableAccess lVarAccessInstance = createVariableAccessFromDefinition(variableInstance, lVarAccessDefinition);
            const syrec::VariableAccess rVarAccessInstance = createVariableAccessFromDefinition(variableInstance, rVarAccessDefinition);

            auto expectedOverlapCheckResultUsingLhsOperandAsBase = utils::VariableAccessOverlapCheckResult(getExpectedVariableAccessOverlapCheckResult());
            if (optionalExpectedOverlapData.has_value()) {
                expectedOverlapCheckResultUsingLhsOperandAsBase.overlappingIndicesInformation = utils::VariableAccessOverlapCheckResult::OverlappingIndicesContainer({optionalExpectedOverlapData->accessedValuePerDimension, optionalExpectedOverlapData->overlappingBitForLhsOperand});
            }
            ASSERT_NO_FATAL_FAILURE(assertEquivalenceBetweenVariableAccessOverlapOperands(lVarAccessInstance, rVarAccessInstance, expectedOverlapCheckResultUsingLhsOperandAsBase));

            auto expectedOverlapCheckResultUsingRhsOperandAsBase = utils::VariableAccessOverlapCheckResult(getExpectedVariableAccessOverlapCheckResult());
            if (optionalExpectedOverlapData.has_value()) {
                expectedOverlapCheckResultUsingRhsOperandAsBase.overlappingIndicesInformation = utils::VariableAccessOverlapCheckResult::OverlappingIndicesContainer({optionalExpectedOverlapData->accessedValuePerDimension, optionalExpectedOverlapData->overlappingBitForRhsOperand});
            }
            ASSERT_NO_FATAL_FAILURE(assertEquivalenceBetweenVariableAccessOverlapOperands(rVarAccessInstance, lVarAccessInstance, expectedOverlapCheckResultUsingRhsOperandAsBase));
        }

    protected:
        [[nodiscard]] static syrec::VariableAccess createVariableAccessFromDefinition(const syrec::Variable::ptr& referenceVar, const VariableAccessDefinition& variableAccessDefinition) {
            syrec::VariableAccess variableAccess;
            variableAccess.setVar(referenceVar);
            variableAccess.indexes.reserve(variableAccessDefinition.accessedValuePerDimension.size());

            for (const auto& accessedValuePerDimension: variableAccessDefinition.accessedValuePerDimension) {
                if (std::holds_alternative<unsigned int>(accessedValuePerDimension)) {
                    variableAccess.indexes.emplace_back(createExpressionForConstantValue(std::get<unsigned int>(accessedValuePerDimension)));
                } else {
                    variableAccess.indexes.emplace_back(createExpressionForLoopVariableIdentifier(std::get<std::string>(accessedValuePerDimension)));
                }
            }

            if (variableAccessDefinition.accessedBitRange.has_value()) {
                const auto accessedBitrangeStartContainer = variableAccessDefinition.accessedBitRange->first;
                const auto accessedBitrangeEndContainer   = variableAccessDefinition.accessedBitRange->second;
                variableAccess.range                      = std::make_pair(accessedBitrangeStartContainer, accessedBitrangeEndContainer);
            }
            return variableAccess;
        }
    };

    class ExpectingOverlappingVariableAccessesTestFixture: public BaseOverlapInVariableAccessesTestFixture {
        utils::VariableAccessOverlapCheckResult::OverlapState getExpectedVariableAccessOverlapCheckResult() override {
            return utils::VariableAccessOverlapCheckResult::OverlapState::Overlapping;
        }
    };

    class ExpectingPotentiallyOverlappingVariableAccessesTestFixture: public BaseOverlapInVariableAccessesTestFixture {
        utils::VariableAccessOverlapCheckResult::OverlapState getExpectedVariableAccessOverlapCheckResult() override {
            return utils::VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping;
        }
    };

    class ExpectingNotOverlappingVariableAccessesTestFixture: public BaseOverlapInVariableAccessesTestFixture {
        utils::VariableAccessOverlapCheckResult::OverlapState getExpectedVariableAccessOverlapCheckResult() override {
            return utils::VariableAccessOverlapCheckResult::OverlapState::NotOverlapping;
        }
    };
} // namespace

TEST_P(ExpectingOverlappingVariableAccessesTestFixture, ExpectingOverlap) {
    ASSERT_NO_FATAL_FAILURE(performTestExecution());
}

TEST_P(ExpectingPotentiallyOverlappingVariableAccessesTestFixture, ExpectingPotentialOverlap) {
    ASSERT_NO_FATAL_FAILURE(performTestExecution());
}

TEST_P(ExpectingNotOverlappingVariableAccessesTestFixture, ExpectingNoOverlap) {
    ASSERT_NO_FATAL_FAILURE(performTestExecution());
}

INSTANTIATE_TEST_SUITE_P(VariableAccessOverlapTests, ExpectingOverlappingVariableAccessesTestFixture,
                         testing::ValuesIn({
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(),
                                                 VariableAccessDefinition(),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 0U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(),
                                                 VariableAccessDefinition(2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(),
                                                 VariableAccessDefinition(std::make_pair("$i", DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(),
                                                 VariableAccessDefinition(std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),

                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(2U),
                                                 VariableAccessDefinition(),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(2U),
                                                 VariableAccessDefinition(2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessStartEqualToBit",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(2U),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessEndEqualToBit",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(14U),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 14U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessOverlappingBit",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(5U),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(2U),
                                                 VariableAccessDefinition(std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(2U),
                                                 VariableAccessDefinition(std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),

                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(5U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccessStartEqualToBit_rAccess_NoDimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccessEndEqualToBit_rAccess_NoDimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(14U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 14U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEqualToStartBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEqualToEndBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEnclosedByAccessedBitRangeOfLVar",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEqualToStartBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEqualToEndBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEnclosedByAccessedBitRangeOfLVar",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessOverlappingFromTheLeft",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::make_pair(1U, 3U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessOverlappingFromTheRight",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::make_pair(1U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessBeingEqualToLeftOne",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 2U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessInLeftOne",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::make_pair(5U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeStartInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeStart",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                 VariableAccessDefinition(std::make_pair(5U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeStartInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair("$i", 8U)),
                                                 VariableAccessDefinition(std::make_pair(8U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 8U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeStartInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeStartWithNonConstantEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                 VariableAccessDefinition(std::make_pair(5U, "$j")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeStartInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeEndWithNonConstantStart",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair("$i", 8U)),
                                                 VariableAccessDefinition(std::make_pair("$j", 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 8U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeEndInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeStart",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                 VariableAccessDefinition(std::make_pair(5U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeEndInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(8U, "$i")),
                                                 VariableAccessDefinition(std::make_pair(8U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 8U)),
                                 OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRange_nonConstantBitrangeEndInLVarBitrangeAccessAndBitrangeEndEqualToRVarBitrangeStartWithNonConstantEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                 VariableAccessDefinition(std::make_pair(5U, "$j")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U}), 5U)),

                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessNoBitRangeAccess_rAccess_DimensionAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 0U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessNoBitRangeAccess_rAccess_DimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessNoBitRangeAccess_rAccess_DimensionAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),

                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitAccess_rAccess_DimensionAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitAccess_rAccess_DimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitAccess_rAccess_DimensionAccessBitRangeAccessOverlappingFromTheLeft",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitAccess_rAccess_DimensionAccessBitRangeAccessOverlappingFromTheRight",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(0U, 3U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitAccess_rAccess_DimensionAccessBitRangeAccessEqualToBit",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),

                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitRangeAccess_rAccess_DimensionAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitRangeAccess_rAccess_DimensionAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitRangeAccess_rAccess_DimensionAccessBitRangeAccessOverlappingFromTheLeft",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(0U, 3U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitRangeAccess_rAccess_DimensionAccessBitRangeAccessOverlappingFromTheRight",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(1U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitRangeAccess_rAccess_DimensionAccessBitRangeAccessEqualToLeftAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessBitRangeAccess_rAccess_DimensionAccessBitRangeAccessInLeftOne",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, 14U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(5U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 5U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEqualToStartBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEqualToEndBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEnclosedByAccessedBitRangeOfLVar",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 5U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEqualToStartBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEqualToEndBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 2U)),
                                 OverlapTestData("1DSignalMultipleValues_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEnclosedByAccessedBitRangeOfLVar",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(5U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({1U}), 5U)),

                                 OverlapTestData("ANDSignal_lAccess_NoBitRangeAccess_rAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 0U)),
                                 OverlapTestData("ANDSignal_lAccess_NoBitRangeAccess_rAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoBitRangeAccess_rAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair("$i", DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessNoBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),

                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessBitRangeAccessOverlappingBitFromTheLeft",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessBitRangeAccessWithStartEqualToBit",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessBitRangeAccessWithEndEqualToBit",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessBitRangeAccessEnclosingBitOfLAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 8U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitAccess_rAccessBitRangeAccessOverlappingBitFromTheRight",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(8U, 0U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValue",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),

                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccess_rAccessNoBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U})),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessStartEqualToBit_rAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 2U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessEndEqualToBit_rAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 5U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 5U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessEnclosingBit_rAccessBitAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 4U),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 4U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessOverlappingOtherFromTheLeft_rAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 4U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessOverlappingOtherFromTheLeft_rAccessBitRangeAccessWithLhsBitrangeStartLargerThanEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(4U, 0U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 4U, 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessOverlappingOtherFromTheLeft_rAccessBitRangeAccessWithRhsBitrangeStartLargerThanEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 4U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(5U, 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U, 4U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessOverlappingOtherFromTheRight_rAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(8U, 0U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 5U, 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessOverlappingOtherFromTheRight_rAccessBitRangeAccessWithRhsBitrangeStartLargerThanEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(8U, 0U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(5U, 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 5U, 5U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessEqualToOther_rAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessEnclosingOther_rAccessBitRangeAccess",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 8U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_BitRangeAccessEnclosingOther_rAccessBitRangeAccess_BothBitrangesWithStartLargerThanEnd",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(8U, 0U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(5U, 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 5U, 5U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEqualToStartBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEqualToEndBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair("$i", 2U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantStartValueEnclosedByAccessedBitRangeOfLVar",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair("$i", 5U)),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 5U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEqualToStartBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEqualToEndBitOfLVarBitrange",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 2U)),
                                 OverlapTestData("ANDSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_NoDimensionAccessBitRangeAccessWithNonConstantEndValueEnclosedByAccessedBitRangeOfLVar",
                                                 VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(2U, DEFAULT_VARIABLE_BITWIDTH - 1U)),
                                                 VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(5U, "$i")),
                                                 ExpectedSymmetricOverlapCheckResultData(std::vector({0U, 1U}), 5U)),
                         }),
                         [](const testing::TestParamInfo<ExpectingOverlappingVariableAccessesTestFixture::ParamType>& info) {
                             return info.param.testName;
                         });

INSTANTIATE_TEST_SUITE_P(VariableAccessOverlapTests, ExpectingPotentiallyOverlappingVariableAccessesTestFixture,
                         testing::ValuesIn({OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ImplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition("$i")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition(std::make_pair("$i", "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$j")}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$j")}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$j")}), std::make_pair("$i", "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ExplicitDimensionAccessUsingConstantIndexWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithNoBitrangeAccess_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$j", "$k"))),

                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessNoBitRangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$j"}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), "$w")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::make_pair("$j", 5U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::make_pair(2U, "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(2U, 5U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$j", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(2U, "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$j", "$k"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessUsingConstantIndexWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), "$j")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$j", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(2U, "$k"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithUnknownBitAccess_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition("$k"),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", "$k"))),

                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ImplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition("$i")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ImplicitDimensionAccessWithBitrangeWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::make_pair("$i", 7U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ImplicitDimensionAccessWithBitrangeWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::make_pair(1U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ImplicitDimensionAccessWithStartAndEndIndexUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::make_pair("$k", "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessWithBitrangeWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$k", 6U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessWithBitrangeWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(1U, "$k"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessWithStartAndEndIndexUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$i", "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessUsingConstantIndexWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", 1U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(6U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessUsingConstantIndices_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(2U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair("$i", "$k"))),

                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ImplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition("$i")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ImplicitDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::make_pair(2U, 4U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::make_pair("$i", 3U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::make_pair(3U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::make_pair("$k", "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}), "$i")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}), std::make_pair(2U, 4U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}), std::make_pair("$i", 4U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}), std::make_pair(2U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$k")}), std::make_pair("$k", "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), "$i")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$j", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair(2U, "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$i", 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$j", "$k"))),

                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ImplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition("$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ImplicitDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::make_pair(6U, 8U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::make_pair("$j", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::make_pair(2U, "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::make_pair("$i", "j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(4U, 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$i", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(2U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$j", "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), "$j")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$j", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair(2U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$j", "$i"))),

                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_NoDimensionAccessNoBitRangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition()),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ImplicitDimensionAccessWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition("$i")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ImplicitDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::make_pair(5U, 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::make_pair("$i", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::make_pair(5U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknownn_rAccess_ExplicitDimensionAccessOnUnknownValueWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), "$k")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(2U, 5U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$j", 5U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair(2U, "$j"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessOnUnknownValueWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant("$i")}), std::make_pair("$k", "$l"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithFullBitwidthAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithUnknownBitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), "$j")),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), 2U)),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$i", 2U))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair(2U, "$i"))),
                                            OverlapTestData("1DSignal_lAccess_ImplicitDimensionAccessWithBitrangeAccessWithStartAndEndUnknown_rAccess_ExplicitDimensionAccessUsingConstantIndexWithBitrangeAccessWithStartAndEndUnknown",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair("$j", "$i")),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(0U)}), std::make_pair("$i", "$s"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$k", "$i"}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$k", 2U}))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$i", 2U})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$i", 2U})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$i", 2U})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$k", "$i"}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$i", 2U})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$k", 2U}))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingNonConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$j", "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({"$k", "$i"}))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithNoBitrangeAccess_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"})),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 3U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 3U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 3U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 1U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithWithBitAccessUsingNonConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$i"),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithWithBitrangeAccessUsingNonConstantStartIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 2U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 3U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 6U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 1U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 3U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 2U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 2U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 3U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 3U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 3U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 3U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", 2U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithWithBitrangeAccessUsingNonConstantEndIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(3U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(6U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(1U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(3U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(3U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(1U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(3U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(3U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$i")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithWithBitrangeAccessUsingNonConstantIndices_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$s", "$i"))),

                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingNonConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), "$s")),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitAccessUsingConstantIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(5U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantStartIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", 2U))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantEndIndex",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(2U, "$s"))),
                                            OverlapTestData("ANDSignal_lAccess_AccessOnFirstDimensionUsingConstantIndexAndSecondDimensionUsingNonConstantIndexWithBitrangeAccessUsingNonConstantIndices_rAccess_UsingOnlyConstantIndicesInDimensionAccessWithBitrangeAccessUsingNonConstantIndices",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 4U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, "$i"}), std::make_pair("$i", "$k")),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair("$s", "$i")))}),
                         [](const testing::TestParamInfo<ExpectingPotentiallyOverlappingVariableAccessesTestFixture::ParamType>& info) {
                             return info.param.testName;
                         });

INSTANTIATE_TEST_SUITE_P(VariableAccessOverlapTests, ExpectingNotOverlappingVariableAccessesTestFixture,
                         testing::ValuesIn({OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_BitAccess",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(2U),
                                                            VariableAccessDefinition(4U)),
                                            OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_BitRangeAccessSmallerThanBit",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(4U),
                                                            VariableAccessDefinition(std::make_pair(0U, 2U))),
                                            OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitAccess_rAccess_BitRangeAccessLargerThanBit",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(4U),
                                                            VariableAccessDefinition(std::make_pair(5U, 7U))),

                                            OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_BitAccessSmallerThanBitrange",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, 7U)),
                                                            VariableAccessDefinition(2U)),
                                            OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_BitAccessLargerThanBitrange",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, 7U)),
                                                            VariableAccessDefinition(8U)),
                                            OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_BitRangeAccessSmallerThanBitrange",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, 7U)),
                                                            VariableAccessDefinition(std::make_pair(0U, 3U))),
                                            OverlapTestData("1DSignal_lAccess_NoDimensionAccessBitRangeAccess_rAccess_BitRangeAccessLargerThanBitrange",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({1U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::make_pair(5U, 7U)),
                                                            VariableAccessDefinition(std::make_pair(9U, 11U))),

                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessNoBitRangeAccess_rAccess_DimensionAccessWithNoBitAccessAndDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessNoBitRangeAccess_rAccess_DimensionAccessWithBitAccessAndDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), 2U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessNoBitRangeAccess_rAccess_DimensionAccessWithBitRangeAccessAndDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)})),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(2U, 4U))),

                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithAccessOnSameBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), 2U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithAccessOnDifferentBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), 3U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndSmallerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 3U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(0U, 2U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndLargerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 3U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(5U, 7U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeOverlappingAndSmallerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 3U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(0U, 6U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeOverlappingAndLargerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 3U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(6U, 2U))),

                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithAccessOnDifferentBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 3U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 3U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndSmallerThanBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(0U, 1U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndLargerThanBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), 2U),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 5U))),

                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitAccessInBitrangeWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 5U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), 4U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitAccessEqualToBitrangeStartWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), 3U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitAccessEqualToBitrangeEndWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), 7U)),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitSmallerThanBitrangeWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(0U, 2U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitLargerThanBitrangeWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(9U, 11U))),

                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeEqualToOtherWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(3U, 7U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeEnclosedToOtherWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(5U, 6U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeOverlappingOtherFromTheLeftWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(0U, 5U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeOverlappingOtherFromTheRightWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(2U)}), std::make_pair(10U, 5U))),

                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeSmallerThanOtherWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(0U, 2U))),
                                            OverlapTestData("1DSignalMultipleValues_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeLargerThanOtherWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector({VariableAccessDefinition::IndexDataVariant(1U)}), std::make_pair(9U, 11U))),

                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithDifferentValueOfDimension_FirstAccessedValueDifferent",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithDifferentValueOfDimension_OtherAccessedValueDifferent",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 2U}))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithDifferentValueOfDimension_MultipleAccessedValuesDifferent",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 1U}))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithAccessOnSameBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 2U}), 2U)),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithAccessOnDifferentBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), 3U)),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndSmallerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 2U}), std::make_pair(0U, 1U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndLargerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 2U}), std::make_pair(5U, 7U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeOverlappingAndSmallerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 2U}), std::make_pair(0U, 3U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeOverlappingAndLargerThanBitWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(7U, 0U))),

                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithAccessOnDifferentBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 3U)),

                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndSmallerThanBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 1U)),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitAccess_rAccess_DimensionAccessWithBitrangeNotOverlappingAndLargerThanBitWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 2U),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 5U)),

                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 0U}))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitAccessInBitrangeWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 1U}), 5U)),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitAccessEqualToBitrangeStartWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), 3U)),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitAccessEqualToBitrangeEndWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 1U}), 7U)),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitSmallerThanBitrangeWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(0U, 2U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitLargerThanBitrangeWithSameValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(10U, 11U))),

                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeEqualToOtherWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 1U}), std::make_pair(3U, 7U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeEnclosedToOtherWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(4U, 6U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeOverlappingOtherFromTheLeftWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 2U}), std::make_pair(1U, 5U))),
                                            OverlapTestData("ANDSignal_lAccess_DimensionAccessWithBitRangeAccess_rAccess_DimensionAccessWithBitRangeOverlappingOtherFromTheRightWithDifferentValueOfDimension",
                                                            VariableDefinition(DEFAULT_VARIABLE_IDENTIFIER, std::vector({2U, 3U}), DEFAULT_VARIABLE_BITWIDTH),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({0U, 1U}), std::make_pair(3U, 7U)),
                                                            VariableAccessDefinition(std::vector<VariableAccessDefinition::IndexDataVariant>({1U, 2U}), std::make_pair(11U, 0U)))

                         }),
                         [](const testing::TestParamInfo<ExpectingNotOverlappingVariableAccessesTestFixture::ParamType>& info) {
                             return info.param.testName;
                         });

TEST(VariableAccessOverlapTests, ReferenceVariableNotSetCorrectlyDetected) {
    const std::vector<unsigned int> variableDimensions        = {1U, 2U};
    const auto                      rOperandReferenceVariable = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER + "otherIdent", variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(rOperandReferenceVariable, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(nullptr);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(rOperandReferenceVariable);
    rhsVariableAccess.indexes = lhsVariableAccess.indexes;
    ASSERT_NO_FATAL_FAILURE(assertSymmetricVariableAccessOverlapResultCannotBeDetermined(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, MismatchInReferenceVariableIdentifierDetectedCorrectly) {
    const std::vector<unsigned int> variableDimensions        = {1U, 2U};
    const auto                      lOperandReferenceVariable = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(lOperandReferenceVariable, testing::NotNull());

    const auto rOperandReferenceVariable = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER + "otherIdent", variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(rOperandReferenceVariable, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(lOperandReferenceVariable);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(rOperandReferenceVariable);
    rhsVariableAccess.indexes = lhsVariableAccess.indexes;
    ASSERT_NO_FATAL_FAILURE(assertSymmetricVariableAccessOverlapResultCannotBeDetermined(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, MismatchInReferenceVariableBitwidthDetectedCorrectly) {
    const std::vector<unsigned int> variableDimensions        = {1U, 2U};
    const auto                      lOperandReferenceVariable = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(lOperandReferenceVariable, testing::NotNull());

    const auto rOperandReferenceVariable = createVariableInstance(lOperandReferenceVariable->name, variableDimensions, lOperandReferenceVariable->bitwidth + 2U);
    ASSERT_THAT(rOperandReferenceVariable, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(lOperandReferenceVariable);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(rOperandReferenceVariable);
    rhsVariableAccess.indexes = lhsVariableAccess.indexes;
    ASSERT_NO_FATAL_FAILURE(assertSymmetricVariableAccessOverlapResultCannotBeDetermined(lhsVariableAccess, rhsVariableAccess));

    auto rOperandReferenceVariableWithSmallerBitwidth = std::make_shared<syrec::Variable>(*rOperandReferenceVariable);
    rOperandReferenceVariable->bitwidth               = lOperandReferenceVariable->bitwidth - 2U;
    rhsVariableAccess.setVar(rOperandReferenceVariable);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricVariableAccessOverlapResultCannotBeDetermined(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, MismatchInReferenceVariableDimensionsDetectedCorrectly) {
    const std::vector<unsigned int> variableDimensions        = {1U, 2U};
    const auto                      lOperandReferenceVariable = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(lOperandReferenceVariable, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(lOperandReferenceVariable);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};

    const std::vector<std::vector<unsigned int>> rOperandReferenceVariableValuesPerDimension = {
            {1U, 2U, 3U}, {1U}, {2U, 1U}, {0U, 2U}};

    const auto rOperandReferenceVariable = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, {}, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(rOperandReferenceVariable, testing::NotNull());

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(rOperandReferenceVariable);

    for (const auto& numValuesPerDimensionOfRhsVariableAccess: rOperandReferenceVariableValuesPerDimension) {
        rOperandReferenceVariable->dimensions = numValuesPerDimensionOfRhsVariableAccess;
        rhsVariableAccess.indexes.clear();
        for (std::size_t i = 0U; i < numValuesPerDimensionOfRhsVariableAccess.size(); ++i) {
            rhsVariableAccess.indexes.emplace_back(createExpressionForConstantValue(0U));
        }

        ASSERT_NO_FATAL_FAILURE(assertSymmetricVariableAccessOverlapResultCannotBeDetermined(lhsVariableAccess, rhsVariableAccess));
    }
}

TEST(VariableAccessOverlapTests, InvalidValueForBitrangeStartValueResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(nullptr, createNumberContainerForConstantValue(DEFAULT_VARIABLE_BITWIDTH - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, InvalidValueForBitrangeEndValueResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), nullptr);

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(2U), createNumberContainerForConstantValue(DEFAULT_VARIABLE_BITWIDTH - 1U));
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, InvalidValueForBothBitrangeStartAndAendResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(nullptr, nullptr);

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, NonConstantValueForBitrangeStartValueResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(createNumberContainerForLoopVariableIdentifier("$i"), createNumberContainerForConstantValue(DEFAULT_VARIABLE_BITWIDTH - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

// Maybe overlapping
TEST(VariableAccessOverlapTests, NonConstantValueForBothBitrangeStartAndAendResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(createNumberContainerForLoopVariableIdentifier("$j"), createNumberContainerForLoopVariableIdentifier("$i"));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, InvalidValueForAccessedValueOfDimensionResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {nullptr, createExpressionForConstantValue(1U)};

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, NonConstantValueForAccessedValueOfDimensionResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    const auto binaryExpressionDefiningAccessedValueOfFirstDimension = std::make_shared<syrec::BinaryExpression>(
            createExpressionForConstantValue(2U),
            syrec::BinaryExpression::BinaryOperation::Add,
            std::make_shared<syrec::NumericExpression>(createNumberContainerForLoopVariableIdentifier("$i"), 2U));

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), binaryExpressionDefiningAccessedValueOfFirstDimension};
    lhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(5U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, InvalidValueInAccessedBitrangesOfBothOperandsResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(nullptr, createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), nullptr);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));

    lhsVariableAccess.range = std::make_pair(nullptr, createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));
    rhsVariableAccess.range = std::make_pair(nullptr, createNumberContainerForConstantValue(5U));
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));

    lhsVariableAccess.range = std::make_pair(nullptr, nullptr);
    rhsVariableAccess.range = std::make_pair(nullptr, createNumberContainerForConstantValue(5U));
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, NonConstantValueInAccessedBitrangesOfBothOperandsResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    const auto containerForLoopVariable = createNumberContainerForLoopVariableIdentifier("$i");
    auto       lhsVariableAccess        = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    lhsVariableAccess.range   = std::make_pair(containerForLoopVariable, createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), createExpressionForConstantValue(1U)};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), containerForLoopVariable);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));

    lhsVariableAccess.range = std::make_pair(containerForLoopVariable, createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));
    rhsVariableAccess.range = std::make_pair(containerForLoopVariable, createNumberContainerForConstantValue(5U));
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));

    lhsVariableAccess.range = std::make_pair(containerForLoopVariable, containerForLoopVariable);
    rhsVariableAccess.range = std::make_pair(containerForLoopVariable, createNumberContainerForConstantValue(5U));
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, InvalidValueForAccessedValueOfDimensionOfBothOperandsResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    const auto binaryExpressionDefiningAccessedValueOfFirstDimensionUsingFirstLoopVariable = std::make_shared<syrec::BinaryExpression>(
            createExpressionForConstantValue(2U),
            syrec::BinaryExpression::BinaryOperation::Add,
            std::make_shared<syrec::NumericExpression>(createNumberContainerForLoopVariableIdentifier("$i"), 2U));

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {nullptr, binaryExpressionDefiningAccessedValueOfFirstDimensionUsingFirstLoopVariable};
    lhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), nullptr};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(5U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));

    rhsVariableAccess.indexes[0U] = nullptr;
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}

TEST(VariableAccessOverlapTests, NonConstantValueForAccessedValueOfDimensionOfBothOperandsResultsInPotentialOverlapAndNoCrash) {
    const std::vector<unsigned int> variableDimensions       = {1U, 2U};
    const auto                      accessedVariableInstance = createVariableInstance(DEFAULT_VARIABLE_IDENTIFIER, variableDimensions, DEFAULT_VARIABLE_BITWIDTH);
    ASSERT_THAT(accessedVariableInstance, testing::NotNull());

    const auto binaryExpressionDefiningAccessedValueOfFirstDimensionUsingFirstLoopVariable = std::make_shared<syrec::BinaryExpression>(
            createExpressionForConstantValue(2U),
            syrec::BinaryExpression::BinaryOperation::Add,
            std::make_shared<syrec::NumericExpression>(createNumberContainerForLoopVariableIdentifier("$i"), 2U));

    auto lhsVariableAccess = syrec::VariableAccess();
    lhsVariableAccess.setVar(accessedVariableInstance);
    lhsVariableAccess.indexes = {binaryExpressionDefiningAccessedValueOfFirstDimensionUsingFirstLoopVariable, binaryExpressionDefiningAccessedValueOfFirstDimensionUsingFirstLoopVariable};
    lhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(0U), createNumberContainerForConstantValue(accessedVariableInstance->bitwidth - 1U));

    auto rhsVariableAccess = syrec::VariableAccess();
    rhsVariableAccess.setVar(accessedVariableInstance);
    rhsVariableAccess.indexes = {createExpressionForConstantValue(0U), binaryExpressionDefiningAccessedValueOfFirstDimensionUsingFirstLoopVariable};
    rhsVariableAccess.range   = std::make_pair(createNumberContainerForConstantValue(5U), lhsVariableAccess.range->first);
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));

    const auto exprForSecondLoopVariable = std::make_shared<syrec::NumericExpression>(createNumberContainerForLoopVariableIdentifier("$j"), 2U);
    rhsVariableAccess.indexes[0U]        = exprForSecondLoopVariable;
    ASSERT_NO_FATAL_FAILURE(assertSymmetricEquivalenceBetweenPotentiallyVariableAccessOverlapOperands(lhsVariableAccess, rhsVariableAccess));
}
