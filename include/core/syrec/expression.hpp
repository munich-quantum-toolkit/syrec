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
#include <memory>

namespace syrec {

    /**
     * @brief SyReC expression
     *
     * This abstract class represents the base of an SyReC expression.
     * Eeach expression is derived from this class and has to
     * implement the bitwidth() and the print() methods.
     */
    struct Expression {
        /**
       * @brief Shared Pointer interface to the class
       */
        using ptr = std::shared_ptr<Expression>;

        using vec = std::vector<ptr>;

        /**
       * @brief Standard constructor
       */
        Expression() = default;

        /**
       * @brief Standard destructor
       */
        virtual ~Expression() = default;

        /**
       * @brief Bit-width of the expression
       *
       * This method returns the bit-width of an expression.
       *
       * @return Bit-width of the expression
       */
        [[nodiscard]] virtual unsigned bitwidth() const = 0;
    };

    /**
     * @brief Numeric Expression
     */
    struct NumericExpression: Expression {
        /**
       * @brief Operation to perform in case of binary numeric expression
       */
        enum : std::uint8_t {
            /**
         * @brief Addition
         */
            Add,

            /**
         * @brief Subtraction
         */
            Subtract,

            /**
         * @brief Multiplication
         */
            Multiply,

            /**
         * @brief Division
         */
            Divide,

            Modulo,

            LogicalAnd,

            LogicalOr,

            BitwiseAnd,

            BitwiseOr,

            LessThan,

            GreaterThan,

            LessEquals,

            GreaterEquals,

            Equals,

            NotEquals

        };

        /**
       * @brief Creates a numeric expression with a value and a bit-width
       *
       * @param value Value
       * @param bitwidth Bit-width of the value
       */
        NumericExpression(Number::ptr value, unsigned bitwidth):
            value(std::move(value)), bwidth(bitwidth) {}

        [[nodiscard]] unsigned bitwidth() const override {
            return bwidth;
        }

        Number::ptr value = nullptr;
        unsigned    bwidth{};
    };

    /**
     * @brief Variable expression
     *
     * This class represents a variable expression and
     * capsulates a variable access pointer var().
     */
    struct VariableExpression: Expression {
        /**
       * @brief Constructor with variable
       *
       * @param var Variable access
       */
        explicit VariableExpression(VariableAccess::ptr var):
            var(std::move(var)) {}

        [[nodiscard]] unsigned bitwidth() const override {
            return var->bitwidth();
        }

        VariableAccess::ptr var = nullptr;
    };

    /**
     * @brief Binary expression
     *
     * This class represents a binary expression between two sub
     * expressions lhs() and rhs() by an operation op().
     */
    struct BinaryExpression: Expression {
        /**
       * @brief Operation to perform
       */
        enum : std::uint8_t {
            /**
         * @brief Addition
         */
            Add,

            /**
         * @brief Subtraction
         */
            Subtract,

            /**
         * @brief Bit-wise EXOR
         */
            Exor,

            /**
         * @brief Multiplication
         *
         * Returns n least significant bits, where n is the bit-width of lhs() and rhs()
         */
            Multiply,

            /**
         * @brief Division
         *
         * Returns n least significant bits, where n is the bit-width of lhs() and rhs()
         */
            Divide,

            /**
         * @brief Modulo Operation
         */
            Modulo,

            /**
         * @brief Multiplication (most significant bits)
         *
         * Performs multiplication and returns the n most significant bits, where n is the bit-width of lhs() and rhs()
         */
            FracDivide,

            /**
         * @brief Logical AND
         */
            LogicalAnd,

            /**
         * @brief Logical OR
         */
            LogicalOr,

            /**
         * @brief Bitwise AND
         */
            BitwiseAnd,

            /**
         * @brief Bitwise OR
         */
            BitwiseOr,

            /**
         * @brief Less than
         */
            LessThan,

            /**
         * @brief Greater than
         */
            GreaterThan,

            /**
         * @brief Equals
         */
            Equals,

            /**
         * @brief Not equals
         */
            NotEquals,

            /**
         * @brief Less equals
         */
            LessEquals,

            /**
         * @brief Greater equals
         */
            GreaterEquals
        };

        /**
       * @brief Constructor which initializes a operation
       *
       * @param lhs Expression on left hand side
       * @param op Operation to be performed
       * @param rhs Expression on right hand side
       */
        BinaryExpression(Expression::ptr lhs,
                         unsigned        op,
                         Expression::ptr rhs):
            lhs(std::move(lhs)),
            op(op), rhs(std::move(rhs)) {}

        /**
       * @brief Bit-width of the expression
       *
       * It is assumed that the bit-width of the left
       * hand side and the right hand side are the same.
       * Thus, the bit-width of the lhs() expression is
       * returned. If a logical operation is performed,
       * however, the returned bit-width is always 1.
       *
       * @return Bit-width of the expression
       */
        [[nodiscard]] unsigned bitwidth() const override {
            switch (op) {
                case LogicalAnd:
                case LogicalOr:
                case LessThan:
                case GreaterThan:
                case Equals:
                case NotEquals:
                case LessEquals:
                case GreaterEquals:
                    return 1;

                default:
                    // lhs and rhs are assumed to have the same bit-width
                    return lhs->bitwidth();
            }
        }

        Expression::ptr lhs = nullptr;
        unsigned        op{};
        Expression::ptr rhs = nullptr;
    };

    /**
     * @brief Shift expression
     *
     * This class represents a binary expression with a
     * sub-expression lhs() and a number rhs() by a shift operation op().
     */
    struct ShiftExpression: Expression {
        /**
       * @brief Shift Operation
       */
        enum : std::uint8_t {
            /**
         * @brief Left-Shift
         */
            Left,

            /**
         * @brief Right-Shift
         */
            Right
        };

        /**
       * @brief Constructor
       *
       * This constructs a shift expression with a sub-expression
       * as the \p lhs, a shift operation \p os, and a number
       * \p rhs.
       *
       * @param lhs Expression to be shifted
       * @param op Shift operation
       * @param rhs Number of bits to shift
       */
        ShiftExpression(Expression::ptr lhs,
                        unsigned        op,
                        Number::ptr     rhs):
            lhs(std::move(lhs)),
            op(op), rhs(std::move(rhs)) {}

        /**
       * @brief Returns the bit-width of the expression
       *
       * The bit-width of a shift expression is the bit-width
       * of the lhs sub-expression.
       *
       * @return Bit-width of the expression
       */
        [[nodiscard]] unsigned bitwidth() const override {
            return lhs->bitwidth();
        }

        Expression::ptr lhs = nullptr;
        unsigned        op{};
        Number::ptr     rhs = nullptr;
    };

    struct UnaryExpression: Expression {
        enum : std::uint8_t {
            LogicalNegation,
            BitwiseNegation
        };

        UnaryExpression(unsigned op, Expression::ptr expr):
            op(op), expr(std::move(expr)) {}

        [[nodiscard]] unsigned bitwidth() const override {
            return op == UnaryExpression::LogicalNegation ? 1 : expr->bitwidth();
        }

        unsigned        op;
        Expression::ptr expr;
    };
} // namespace syrec
