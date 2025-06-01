/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/variable_access_index_check.hpp"

#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/variable.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

using namespace utils;

namespace {
    VariableAccessIndicesValidity::IndexValidationResult::IndexValidity isIndexInRange(unsigned int indexValue, unsigned int maxAllowedValue) {
        return indexValue <= maxAllowedValue ? VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Ok : VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange;
    }
} // namespace

bool VariableAccessIndicesValidity::isValid() const {
    return std::all_of(
                   accessedValuePerDimensionValidity.cbegin(),
                   accessedValuePerDimensionValidity.cend(),
                   [](const IndexValidationResult& validityOfAccessedValueOfDimension) {
                       return validityOfAccessedValueOfDimension.indexValidity == IndexValidationResult::IndexValidity::Ok;
                   }) &&
           (!bitRangeAccessValidity.has_value() || (bitRangeAccessValidity->bitRangeStartValidity.indexValidity == IndexValidationResult::IndexValidity::Ok && bitRangeAccessValidity->bitRangeEndValidity.indexValidity == IndexValidationResult::IndexValidity::Ok));
}

std::optional<VariableAccessIndicesValidity> utils::validateVariableAccessIndices(const syrec::VariableAccess& variableAccess) {
    if (variableAccess.var == nullptr) {
        return std::nullopt;
    }

    VariableAccessIndicesValidity validityOfVariableAccessIndices;
    validityOfVariableAccessIndices.accessedValuePerDimensionValidity = std::vector(variableAccess.indexes.size(), VariableAccessIndicesValidity::IndexValidationResult(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Unknown, std::nullopt));

    const std::size_t numDimensionsOfVariable = variableAccess.getVar()->dimensions.size();
    for (std::size_t dimensionIdx = 0; dimensionIdx < variableAccess.indexes.size(); ++dimensionIdx) {
        const syrec::Expression::ptr& accessedValueOfDimension = variableAccess.indexes.at(dimensionIdx);
        if (accessedValueOfDimension == nullptr) {
            continue;
        }

        const auto& accessedValueOfDimensionExprCasted = std::dynamic_pointer_cast<syrec::NumericExpression>(accessedValueOfDimension);
        if (accessedValueOfDimensionExprCasted == nullptr || accessedValueOfDimensionExprCasted->value == nullptr || !accessedValueOfDimensionExprCasted->value->isConstant()) {
            continue;
        }

        const std::optional<unsigned int> evaluatedAccessedValueOfDimension = accessedValueOfDimensionExprCasted->value->tryEvaluate({});
        if (dimensionIdx < numDimensionsOfVariable && evaluatedAccessedValueOfDimension.has_value()) {
            if (variableAccess.getVar()->dimensions.at(dimensionIdx) == 0) {
                validityOfVariableAccessIndices.accessedValuePerDimensionValidity[dimensionIdx].indexValidity = VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange;
            } else {
                // We are assuming zero-based indexing
                validityOfVariableAccessIndices.accessedValuePerDimensionValidity[dimensionIdx].indexValidity = isIndexInRange(*evaluatedAccessedValueOfDimension, variableAccess.getVar()->dimensions.at(dimensionIdx) - 1);
            }
        } else {
            validityOfVariableAccessIndices.accessedValuePerDimensionValidity[dimensionIdx].indexValidity = VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Unknown;
        }
        validityOfVariableAccessIndices.accessedValuePerDimensionValidity[dimensionIdx].indexValue = evaluatedAccessedValueOfDimension;
    }

    if (!variableAccess.range.has_value()) {
        return validityOfVariableAccessIndices;
    }

    auto bitRangeStartValidity = VariableAccessIndicesValidity::IndexValidationResult(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Unknown, std::nullopt);
    auto bitRangeEndValidity   = VariableAccessIndicesValidity::IndexValidationResult(VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::Unknown, std::nullopt);

    const syrec::Number::ptr& bitRangeStart = variableAccess.range->first;
    const syrec::Number::ptr& bitRangeEnd   = variableAccess.range->second;
    if (const std::optional<unsigned int> evaluatedBitRangeStart = bitRangeStart != nullptr && bitRangeStart->isConstant() ? bitRangeStart->tryEvaluate({}) : std::nullopt; evaluatedBitRangeStart.has_value()) {
        if (variableAccess.getVar()->bitwidth == 0) {
            bitRangeStartValidity.indexValidity = VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange;
        } else {
            // We are assuming zero-based indexing
            bitRangeStartValidity.indexValidity = isIndexInRange(*evaluatedBitRangeStart, variableAccess.getVar()->bitwidth - 1);
        }
        bitRangeStartValidity.indexValue = evaluatedBitRangeStart;
    }

    if (const std::optional<unsigned int> evaluatedBitRangeEnd = bitRangeEnd != nullptr && bitRangeEnd->isConstant() ? bitRangeEnd->tryEvaluate({}) : std::nullopt; evaluatedBitRangeEnd.has_value()) {
        if (variableAccess.getVar()->bitwidth == 0) {
            bitRangeEndValidity.indexValidity = VariableAccessIndicesValidity::IndexValidationResult::IndexValidity::OutOfRange;
        } else {
            // We are assuming zero-based indexing
            bitRangeEndValidity.indexValidity = isIndexInRange(*evaluatedBitRangeEnd, variableAccess.getVar()->bitwidth - 1);
        }
        bitRangeEndValidity.indexValue = evaluatedBitRangeEnd;
    }
    validityOfVariableAccessIndices.bitRangeAccessValidity = VariableAccessIndicesValidity::BitRangeValidityResult({bitRangeStartValidity, bitRangeEndValidity});
    return validityOfVariableAccessIndices;
}
