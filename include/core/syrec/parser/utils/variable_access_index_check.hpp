/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "core/syrec/variable.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace utils {
    struct VariableAccessIndicesValidity {
        struct IndexValidationResult {
            enum class IndexValidity : std::uint8_t {
                Ok,
                OutOfRange,
                Unknown
            };
            IndexValidity               indexValidity;
            std::optional<unsigned int> indexValue;

            explicit IndexValidationResult(IndexValidity indexValidity, std::optional<unsigned int> indexValue):
                indexValidity(indexValidity), indexValue(indexValue) {}
        };

        struct BitRangeValidityResult {
            IndexValidationResult bitRangeStartValidity;
            IndexValidationResult bitRangeEndValidity;
        };
        std::vector<IndexValidationResult>    accessedValuePerDimensionValidity;
        std::optional<BitRangeValidityResult> bitRangeAccessValidity;

        [[nodiscard]] bool isValid() const;
    };

    /**
     * @brief   Determine whether the defined indices of the variable access are within range of the dimensions of the accessed variable.
     * @details Note that no expressions are evaluated and no loop variable value lookup is performed, both cases will lead to the validity of the index to be considered as unknown. <br>
     *          Additionally, the validity of index values in accessed dimensions at indices larger than the number of dimensions of the accessed variable is considered to be unknown.
     * @param variableAccess The variable access to validate
     * @return The validity of the indices defined in the accessed values per dimension and bitrange.
     */
    [[nodiscard]] std::optional<VariableAccessIndicesValidity> validateVariableAccessIndices(const syrec::VariableAccess& variableAccess);
} //namespace utils
