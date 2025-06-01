/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/variable_overlap_check.hpp"

#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/variable.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace utils;

namespace {
    std::pair<unsigned int, unsigned int> determineBitrangeConstantIndexPairOrderedAscendingly(unsigned int bitrangeStartIndexValue, unsigned int bitrangeEndIndexValue) {
        return bitrangeStartIndexValue <= bitrangeEndIndexValue ? std::make_pair(bitrangeStartIndexValue, bitrangeEndIndexValue) : std::make_pair(bitrangeEndIndexValue, bitrangeStartIndexValue);
    }

    std::optional<unsigned int> tryEvaluateNumber(const syrec::Number::ptr& numberToEvaluate) {
        return numberToEvaluate && numberToEvaluate->isConstant() ? numberToEvaluate->tryEvaluate({}) : std::nullopt;
    }

    std::optional<unsigned int> tryEvaluateNumericExpr(const syrec::Expression& expression) {
        if (const auto& numericExprOfContainerToEvaluate = dynamic_cast<const syrec::NumericExpression*>(&expression); numericExprOfContainerToEvaluate != nullptr) {
            return tryEvaluateNumber(numericExprOfContainerToEvaluate->value);
        }
        return std::nullopt;
    }

    bool doReferenceVariablesMatch(const syrec::Variable& lVarReference, const syrec::Variable& rVarReference) noexcept {
        return lVarReference.name == rVarReference.name && lVarReference.bitwidth == rVarReference.bitwidth && std::equal(lVarReference.dimensions.cbegin(), lVarReference.dimensions.cend(), rVarReference.dimensions.cbegin(), rVarReference.dimensions.cend());
    }
} // namespace

std::string VariableAccessOverlapCheckResult::stringifyOverlappingIndicesInformation() const {
    std::string stringificationBuffer;
    for (std::size_t dimensionIdx = 0; dimensionIdx < overlappingIndicesInformation->knownValueOfAccessedValuePerDimension.size(); ++dimensionIdx) {
        stringificationBuffer += "(" + std::to_string(dimensionIdx) + "," + std::to_string(overlappingIndicesInformation->knownValueOfAccessedValuePerDimension.at(dimensionIdx)) + ")";
    }
    stringificationBuffer += "| " + std::to_string(overlappingIndicesInformation->overlappingBit);
    return stringificationBuffer;
}

[[nodiscard]] std::optional<VariableAccessOverlapCheckResult> utils::checkOverlapBetweenVariableAccesses(const syrec::VariableAccess& lVariableAccess, const syrec::VariableAccess& rVariableAccess) {
    const syrec::Variable::ptr& lVarPtr = lVariableAccess.getVar();
    const syrec::Variable::ptr& rVarPtr = rVariableAccess.getVar();
    if (lVarPtr == nullptr || rVarPtr == nullptr || !doReferenceVariablesMatch(*lVarPtr, *rVarPtr)) {
        return std::nullopt;
    }

    const syrec::Variable& lVar = *lVarPtr;
    const syrec::Variable& rVar = *rVarPtr;

    const std::size_t         numDimensionsToCheck = std::min({lVar.dimensions.size(), lVariableAccess.indexes.size(),
                                                               rVar.dimensions.size(), rVariableAccess.indexes.size()});
    std::vector<unsigned int> constantIndicesOfAccessedValuesPerDimension;
    if (numDimensionsToCheck == 0) {
        const auto& exprDefiningAccessedValueOfDimensionInLVar = !lVariableAccess.indexes.empty() ? lVariableAccess.indexes.front() : nullptr;
        const auto& exprDefiningAccessedValueOfDimensionInRVar = !rVariableAccess.indexes.empty() ? rVariableAccess.indexes.front() : nullptr;

        std::optional<unsigned int> accessedValueInLVar;
        std::optional<unsigned int> accessedValueInRVar;
        if (exprDefiningAccessedValueOfDimensionInLVar == nullptr) {
            accessedValueInLVar = lVar.dimensions.size() == 1 && lVar.dimensions.front() == 1 ? std::make_optional(0) : std::nullopt;
        } else {
            accessedValueInLVar = tryEvaluateNumericExpr(*exprDefiningAccessedValueOfDimensionInLVar);
        }

        if (exprDefiningAccessedValueOfDimensionInRVar == nullptr) {
            accessedValueInRVar = rVar.dimensions.size() == 1 && rVar.dimensions.front() == 1 ? std::make_optional(0) : std::nullopt;
        } else {
            accessedValueInRVar = tryEvaluateNumericExpr(*exprDefiningAccessedValueOfDimensionInRVar);
        }

        // If one were to assume that all indices provided in the two variable accesses are within range of the formal bounds of the accessed variable, an index with an non-constant value
        // could be assumed to be overlapping for a dimension that has only one value. However, we do not assume in-range indices and thus can only report a potential overlap. The same reasoning
        // can be applied for non-constant indices of the accessed bitrange for a variable with defined bitwidth of 1.
        if (!accessedValueInLVar.has_value() || !accessedValueInRVar.has_value()) {
            return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping);
        }
        if (accessedValueInLVar.value() != accessedValueInRVar.value()) {
            return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::NotOverlapping);
        }
        constantIndicesOfAccessedValuesPerDimension.emplace_back(0);
    } else {
        constantIndicesOfAccessedValuesPerDimension.reserve(numDimensionsToCheck);
    }

    for (std::size_t i = 0; i < numDimensionsToCheck; ++i) {
        const auto& exprDefiningAccessedValueOfDimensionInLVar = lVariableAccess.indexes.at(i);
        const auto& exprDefiningAccessedValueOfDimensionInRVar = rVariableAccess.indexes.at(i);
        if (exprDefiningAccessedValueOfDimensionInLVar == nullptr || exprDefiningAccessedValueOfDimensionInRVar == nullptr) {
            return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping);
        }

        const std::optional<unsigned int> accessedValueInLVar = exprDefiningAccessedValueOfDimensionInLVar != nullptr ? tryEvaluateNumericExpr(*exprDefiningAccessedValueOfDimensionInLVar) : std::nullopt;
        const std::optional<unsigned int> accessedValueInRVar = exprDefiningAccessedValueOfDimensionInRVar != nullptr ? tryEvaluateNumericExpr(*exprDefiningAccessedValueOfDimensionInRVar) : std::nullopt;

        // If one were to assume that all indices provided in the two variable accesses are within range of the formal bounds of the accessed variable, an index with an non-constant value
        // could be assumed to be overlapping for a dimension that has only one value. However, we do not assume in-range indices and thus can only report a potential overlap. The same reasoning
        // can be applied for non-constant indices of the accessed bitrange for a variable with defined bitwidth of 1.
        if (!accessedValueInLVar.has_value() || !accessedValueInRVar.has_value()) {
            return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping);
        }
        if (accessedValueInLVar.value() != accessedValueInRVar.value()) {
            return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::NotOverlapping);
        }
        constantIndicesOfAccessedValuesPerDimension.emplace_back(accessedValueInLVar.value());
    }

    // The caller does not need to explicitly define the accessed bit range in the variable access if he wishes to access the whole variable bitwidth
    std::optional<unsigned int> evaluatedLVarBitRangeStart = 0;
    std::optional               evaluatedLVarBitRangeEnd   = lVariableAccess.var->bitwidth - 1;
    if (lVariableAccess.range.has_value()) {
        evaluatedLVarBitRangeStart = tryEvaluateNumber(lVariableAccess.range->first);
        evaluatedLVarBitRangeEnd   = lVariableAccess.range->first == lVariableAccess.range->second ? evaluatedLVarBitRangeStart : tryEvaluateNumber(lVariableAccess.range->second);
    }

    std::optional<unsigned int> evaluatedRVarBitRangeStart = 0;
    std::optional               evaluatedRVarBitRangeEnd   = rVariableAccess.var->bitwidth - 1;
    if (rVariableAccess.range.has_value()) {
        evaluatedRVarBitRangeStart = tryEvaluateNumber(rVariableAccess.range->first);
        evaluatedRVarBitRangeEnd   = rVariableAccess.range->first == rVariableAccess.range->second ? evaluatedRVarBitRangeStart : tryEvaluateNumber(rVariableAccess.range->second);
    }

    if ((!evaluatedLVarBitRangeStart.has_value() && !evaluatedLVarBitRangeEnd.has_value()) || (!evaluatedRVarBitRangeStart.has_value() && !evaluatedRVarBitRangeEnd.has_value())) {
        return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping);
    }

    std::optional<unsigned int> overlappingBit;
    // Either the bitrange start or end index of the lhs variable access has a constant value
    if (evaluatedLVarBitRangeStart.has_value() != evaluatedLVarBitRangeEnd.has_value()) {
        const auto lVarKnownBitIndexValue = evaluatedLVarBitRangeStart.has_value() ? *evaluatedLVarBitRangeStart : *evaluatedLVarBitRangeEnd;
        // Either the bitrange start or end index of the rhs variable access has a constant value
        if (evaluatedRVarBitRangeStart.has_value() != evaluatedRVarBitRangeEnd.has_value()) {
            const auto rVarKnownBitIndexValue = evaluatedRVarBitRangeStart.has_value() ? *evaluatedRVarBitRangeStart : *evaluatedRVarBitRangeEnd;
            if (lVarKnownBitIndexValue == rVarKnownBitIndexValue) {
                overlappingBit = lVarKnownBitIndexValue;
            }
        } else if (evaluatedRVarBitRangeStart.has_value()) {
            // Both bitrange start and end index of the rhs variable access have a constant value
            const std::pair<unsigned int, unsigned int> orderedIndicesOfBitrangeAccessOfRVar = determineBitrangeConstantIndexPairOrderedAscendingly(*evaluatedRVarBitRangeStart, *evaluatedRVarBitRangeEnd);
            if (orderedIndicesOfBitrangeAccessOfRVar.first <= lVarKnownBitIndexValue && lVarKnownBitIndexValue <= orderedIndicesOfBitrangeAccessOfRVar.second) {
                overlappingBit = lVarKnownBitIndexValue;
            }
        }
    } else if (evaluatedLVarBitRangeStart.has_value()) {
        // Either the bitrange start or end index of the rhs variable access has a constant value
        if (evaluatedRVarBitRangeStart.has_value() != evaluatedRVarBitRangeEnd.has_value()) {
            const auto                                  rVarKnownBitIndexValue               = evaluatedRVarBitRangeStart.has_value() ? *evaluatedRVarBitRangeStart : *evaluatedRVarBitRangeEnd;
            const std::pair<unsigned int, unsigned int> orderedIndicesOfBitrangeAccessOfLVar = determineBitrangeConstantIndexPairOrderedAscendingly(*evaluatedLVarBitRangeStart, *evaluatedLVarBitRangeEnd);
            if (orderedIndicesOfBitrangeAccessOfLVar.first <= rVarKnownBitIndexValue && rVarKnownBitIndexValue <= orderedIndicesOfBitrangeAccessOfLVar.second) {
                overlappingBit = rVarKnownBitIndexValue;
            }
        } else if (evaluatedRVarBitRangeStart.has_value()) {
            // The indices of the accessed bitrange in both operands have a constant value
            const std::pair<unsigned int, unsigned int> orderedIndicesOfBitrangeAccessOfRVar = determineBitrangeConstantIndexPairOrderedAscendingly(*evaluatedRVarBitRangeStart, *evaluatedRVarBitRangeEnd);
            const std::pair<unsigned int, unsigned int> orderedIndicesOfBitrangeAccessOfLVar = determineBitrangeConstantIndexPairOrderedAscendingly(*evaluatedLVarBitRangeStart, *evaluatedLVarBitRangeEnd);

            if ((orderedIndicesOfBitrangeAccessOfLVar.first < orderedIndicesOfBitrangeAccessOfRVar.first && orderedIndicesOfBitrangeAccessOfLVar.second < orderedIndicesOfBitrangeAccessOfRVar.first) || (orderedIndicesOfBitrangeAccessOfLVar.first > orderedIndicesOfBitrangeAccessOfRVar.first && orderedIndicesOfBitrangeAccessOfLVar.first > orderedIndicesOfBitrangeAccessOfRVar.second)) {
                return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::NotOverlapping);
            }

            const std::pair<unsigned int, unsigned int> accessedBitRangeOfLVar = std::make_pair(*evaluatedLVarBitRangeStart, *evaluatedLVarBitRangeEnd);
            if (accessedBitRangeOfLVar.first < accessedBitRangeOfLVar.second) {
                overlappingBit = accessedBitRangeOfLVar.first > orderedIndicesOfBitrangeAccessOfRVar.first ? accessedBitRangeOfLVar.first : orderedIndicesOfBitrangeAccessOfRVar.first;
            } else if (accessedBitRangeOfLVar.first > accessedBitRangeOfLVar.second) {
                overlappingBit = accessedBitRangeOfLVar.first < orderedIndicesOfBitrangeAccessOfRVar.second ? accessedBitRangeOfLVar.first : orderedIndicesOfBitrangeAccessOfRVar.second;
            } else {
                overlappingBit = accessedBitRangeOfLVar.first;
            }
        }
    }

    if (!overlappingBit.has_value()) {
        return VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::MaybeOverlapping);
    }

    auto overlapCheckResultContainer                          = VariableAccessOverlapCheckResult(VariableAccessOverlapCheckResult::OverlapState::Overlapping);
    overlapCheckResultContainer.overlappingIndicesInformation = VariableAccessOverlapCheckResult::OverlappingIndicesContainer({constantIndicesOfAccessedValuesPerDimension, *overlappingBit});
    return overlapCheckResultContainer;
}
