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

#include "core/syrec/expression.hpp"

#include <climits>
#include <cstdint>
#include <optional>

namespace utils {
    /**
     * @brief Defines the available operations for the truncation of constant integer values.
     */
    enum class IntegerConstantTruncationOperation : std::uint8_t {
        /**
         * @brief Truncate using the module operation (e.g. 5 MOD 3 = 2)
         */
        Modulo,
        /**
         * @brief Truncate using the bitwise AND operation and a 'one' bitmask of the expected bitwidth (e.g. truncate value 13 to bitwidth of 3 => 1101 AND 0111 = 0101  [13 AND 3 = 4])
         */
        BitwiseAnd
    };

    /**
     * @brief Truncate a constant value to an expected bitwidth using the user-defined truncation operation.
     * @param valueToTruncate The value to truncate
     * @param expectedResultBitwidth The expected bitwidth of the result (defines the rhs operand of integer constant truncation operation)
     * @param integerConstantTruncationOperation The truncation operation to use
     * @return The truncated constant value
     */
    [[nodiscard]] inline unsigned int truncateConstantValueToExpectedBitwidth(unsigned int valueToTruncate, unsigned int expectedResultBitwidth, IntegerConstantTruncationOperation integerConstantTruncationOperation) {
        if (expectedResultBitwidth == 0) {
            return 0;
        }

        if (expectedResultBitwidth <= 32) {
            if (const unsigned int maxValueStorableInExpectedResultBitwidth = expectedResultBitwidth == 32 ? UINT_MAX : (1U << expectedResultBitwidth) - 1U; valueToTruncate >= maxValueStorableInExpectedResultBitwidth) {
                if (integerConstantTruncationOperation == IntegerConstantTruncationOperation::BitwiseAnd) {
                    // Create suitable bitmask to extract relevant bits from value to truncate as: 2^e_bitwidth - 1
                    return valueToTruncate & maxValueStorableInExpectedResultBitwidth;
                }
                if (integerConstantTruncationOperation == IntegerConstantTruncationOperation::Modulo) {
                    return valueToTruncate % maxValueStorableInExpectedResultBitwidth;
                }
            }
        }
        return valueToTruncate;
    }

    /**
     * @brief Try to evaluate the given binary operation using the provided operand values.
     * @param lOperand The optional value of the left hand side operand of the binary operation
     * @param binaryOperation The binary operation used for the evaluation of the binary expression
     * @param rOperand The optional value of the right hand side operand of the binary operation
     * @return Returns the result of the binary operation if both operands had a constant value.
     */
    [[nodiscard]] inline std::optional<unsigned int> tryEvaluate(const std::optional<unsigned int> lOperand, syrec::BinaryExpression::BinaryOperation binaryOperation, const std::optional<unsigned int> rOperand) {
        std::optional<unsigned int> evaluationResult;
        if (lOperand.has_value() && rOperand.has_value()) {
            const unsigned int constantValueOfLOperand = *lOperand;
            const unsigned int constantValueOfROperand = *rOperand;
            switch (binaryOperation) {
                case syrec::BinaryExpression::BinaryOperation::Add:
                    evaluationResult = constantValueOfLOperand + constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::Subtract:
                    evaluationResult = constantValueOfLOperand - constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::Multiply:
                    evaluationResult = constantValueOfLOperand * constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::Divide:
                    if (constantValueOfROperand == 0) {
                        return std::nullopt;
                    }

                    evaluationResult = constantValueOfLOperand / constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::FracDivide:
                    if (constantValueOfROperand == 0) {
                        return std::nullopt;
                    }

                    // The sizeof the unsigned int type does not necessarily need to be equal to 32 bits (https://en.cppreference.com/w/cpp/language/types), thus a case distinction is needed to calculate the correct value
                    // based on the actual size of the data type.
                    if constexpr (sizeof(unsigned int) == 2) {
                        evaluationResult = (static_cast<uint32_t>(constantValueOfLOperand) * static_cast<uint32_t>(constantValueOfROperand)) >> 16;
                    } else {
                        evaluationResult = (static_cast<uint64_t>(constantValueOfLOperand) * static_cast<uint64_t>(constantValueOfROperand)) >> 32;
                    }
                    break;
                case syrec::BinaryExpression::BinaryOperation::Modulo:
                    if (constantValueOfROperand == 0) {
                        return std::nullopt;
                    }

                    evaluationResult = constantValueOfLOperand % constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::Equals:
                    evaluationResult = constantValueOfLOperand == constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::NotEquals:
                    evaluationResult = constantValueOfLOperand != constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::GreaterEquals:
                    evaluationResult = constantValueOfLOperand >= constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::GreaterThan:
                    evaluationResult = constantValueOfLOperand > constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::LessEquals:
                    evaluationResult = constantValueOfLOperand <= constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::LessThan:
                    evaluationResult = constantValueOfLOperand < constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::Exor:
                    evaluationResult = constantValueOfLOperand ^ constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::BitwiseAnd:
                    evaluationResult = constantValueOfLOperand & constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::BitwiseOr:
                    evaluationResult = constantValueOfLOperand | constantValueOfROperand;
                    break;
                case syrec::BinaryExpression::BinaryOperation::LogicalAnd:
                    evaluationResult = constantValueOfLOperand > 0 && constantValueOfROperand > 0;
                    break;
                case syrec::BinaryExpression::BinaryOperation::LogicalOr:
                    evaluationResult = constantValueOfLOperand > 0 || constantValueOfROperand > 0;
                    break;
                default:
                    break;
            }
        }
        return evaluationResult;
    }

    /**
     * @brief Try to evaluate the result of the shift operation is both operands have a constant value.
     * @param toBeShiftedValue The left hand side value of the shift operation
     * @param shiftOperation The shift operation to use for the evaluation of the shift expression
     * @param shiftAmount The right hand side value of the shift operation defining the number of positions each bit of the left hand side operand is shifted using the shift operation.
     * @return The result of the shift operation if both operands had a constant value, 0 if the \p toBeShiftedValue has a value of zero, otherwise std::nullopt.
     */
    [[nodiscard]] inline std::optional<unsigned int> tryEvaluate(const std::optional<unsigned int> toBeShiftedValue, syrec::ShiftExpression::ShiftOperation shiftOperation, const std::optional<unsigned int> shiftAmount) {
        if (shiftAmount.has_value() && *shiftAmount == 0) {
            return toBeShiftedValue;
        }
        if (toBeShiftedValue.has_value()) {
            if (*toBeShiftedValue == 0) {
                return 0;
            }

            if (shiftAmount.has_value()) {
                switch (shiftOperation) {
                    case syrec::ShiftExpression::ShiftOperation::Left:
                        return *toBeShiftedValue << *shiftAmount;
                    case syrec::ShiftExpression::ShiftOperation::Right:
                        return *toBeShiftedValue >> *shiftAmount;
                }
            }
        }
        return std::nullopt;
    }
} // namespace utils
