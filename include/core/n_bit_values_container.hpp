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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syrec {
    /**
     * Provides a rudimentary reimplementation of the boost dynamic_bitset container
     */
    class NBitValuesContainer {
    public:
        /**
         * Constructs an empty container
         */
        NBitValuesContainer() = default;

        /**
         * Construct a zero initialized container storing n bits.
         * @param n The number of bits to be stored in the container
         */
        explicit NBitValuesContainer(std::size_t n):
            lineValues(std::vector(n, false)) {}

        /**
         * Construct and initialize a container storing @p n bits using an integer
         *
         * The first [0, min(@p n, 64)) bits are initialized as (@p initialLineValues >> i) & 1.
         * All remaining bits are initialized with the boolean value FALSE.
         *
         * @param n The number of bits to be stored in the container
         * @param initialLineValues An integer defining the initial values of the bits in the container
         */
        explicit NBitValuesContainer(std::size_t n, std::uint64_t initialLineValues):
            NBitValuesContainer(n) {
            for (std::size_t i = 0; i < lineValues.size() && i < 64U; ++i) {
                lineValues[i] = static_cast<bool>((initialLineValues >> i) & 1);
            }
        }

        /**
         * Resize the container.
         * If the new size N_new is larger than N_old than (N_new - N_old) elements initialized with FALSE are appended to the existing ones.
         * A new smaller size causes a truncation of the existing values to the range [0, N_new).
         * @param n Resize the container to hold n elements
         */
        void resize(std::size_t n) {
            lineValues.resize(n, false);
        }

        /**
         * @brief Get the value of a specific bit
         * @param bitPosition The zero-based index of the accessed bit
         * @return The value of the accessed bit if provided index was in the range [0, size()), otherwise an exception.
         */
        [[maybe_unused]] bool operator[](const std::size_t bitPosition) const {
            return test(bitPosition).value();
        }

        /**
         * @brief Flip the value of a specific bit
         * @param bitPosition The zero-based index of the accessed bit
         * @return Whether the provided index was in the range [0, size())
         */
        [[maybe_unused]] bool flip(std::size_t bitPosition) {
            const auto currentBitValue = test(bitPosition);
            if (!currentBitValue.has_value()) {
                return false;
            }
            set(bitPosition, !*currentBitValue);
            return true;
        }

        /**
         * @brief Set the value of a specific bit
         * @param bitPosition The zero-based index of the accessed bit
         * @param value The future value of the bit
         * @return Whether the provided index was in the range [0, size())
         */
        [[maybe_unused]] bool set(std::size_t bitPosition, bool value) {
            if (bitPosition >= size()) {
                return false;
            }
            lineValues[bitPosition] = value;
            return true;
        }

        /**
         * @brief Set the value of a specific bit to TRUE
         * @param bitPosition The zero-based index of the accessed bit
         * @return Whether the provided index was in the range [0, size())
         */
        [[maybe_unused]] bool set(std::size_t bitPosition) {
            return set(bitPosition, true);
        }

        /**
         * @brief Set the value of a specific bit to FALSE
         * @param bitPosition The zero-based index of the accessed bit
         * @return Whether the provided index was in the range [0, size())
         */
        [[maybe_unused]] bool reset(std::size_t bitPosition) {
            return set(bitPosition, false);
        }

        /**
         * @brief Get the value of a specific bit
         * @param bitPosition The zero-based index of the accessed bit
         * @return Whether the provided index was in the range [0, size())
         */
        [[nodiscard]] std::optional<bool> test(std::size_t bitPosition) const {
            if (bitPosition >= size()) {
                return std::nullopt;
            }
            return lineValues[bitPosition];
        }

        /**
         * @return The number of bits stored in the container
         */
        [[nodiscard]] std::size_t size() const noexcept {
            return lineValues.size();
        }

        /**
         * @return Returns whether any bit in the container is set to the boolean value TRUE. A empty container is considered as having no bits set to TRUE.
         */
        [[nodiscard]] bool none() const {
            return std::none_of(lineValues.begin(), lineValues.end(), [](const bool lineValue) { return lineValue; });
        }

        /**
         * Stringify the value of the size() bits of the container starting with the 0th bit.
         * The boolean values are converted to their numeric counterparts (FALSE -> 0, TRUE -> 1).
         * @return The stringified container content
         */
        [[nodiscard]] std::string stringify() const {
            std::string stringifiedContainerContent(size(), '0');
            for (std::size_t i = 0; i < size(); ++i) {
                stringifiedContainerContent[i] = operator[](i) ? '1' : '0';
            }
            return stringifiedContainerContent;
        }

    protected:
        std::vector<bool> lineValues;
    };

    /**
     * @brief Combine two containers storing the same number of bits using the bitwise AND operation.
     * @param lOperand The left-hand operand of the bitwise AND operation (A & B)
     * @param rOperand The right-hand operand of the bitwise AND operation (A & B)
     * @return The result of the bitwise AND operation if both operands had the same size, otherwise and exception
     */
    inline NBitValuesContainer operator&(const NBitValuesContainer& lOperand, const NBitValuesContainer& rOperand) {
        auto bitwiseAndResult = NBitValuesContainer(lOperand.size());
        for (std::size_t i = 0; i < lOperand.size(); ++i) {
            bitwiseAndResult.set(i, lOperand[i] && rOperand[i]);
        }
        return bitwiseAndResult;
    }

    /**
     * @brief Perform a bitwise comparison between two containers container.
     * @param lOperand The left operand of the equality operation
     * @param rOperand The right operand of the equality operation
     * @return Whether the two objects store the same number of bits and are bitwise equal
     */
    inline bool operator==(const NBitValuesContainer& lOperand, const NBitValuesContainer& rOperand) {
        if (lOperand.size() != rOperand.size()) {
            return false;
        }

        bool equal = true;
        for (std::size_t i = 0; i < lOperand.size() && equal; ++i) {
            equal = lOperand[i] == rOperand[i];
        }
        return equal;
    }
}; // namespace syrec
