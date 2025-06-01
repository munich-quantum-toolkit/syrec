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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace utils {
    class IfStatementExpressionComponentsRecorder {
    public:
        using ptr = std::shared_ptr<IfStatementExpressionComponentsRecorder>;
        enum class ExpressionBracketKind : std::uint8_t {
            Opening,
            Closing
        };

        enum class VariableAccessComponent : std::uint8_t {
            BitrangeStart,
            BitrangeEnd,
            DimensionAccessExpressionStart,
            DimensionAccessExpressionEnd
        };

        enum class OperationMode : std::uint8_t {
            Recording,
            Comparing
        };
        using ExpressionComponent = std::variant<std::string, unsigned int, syrec::BinaryExpression::BinaryOperation, syrec::ShiftExpression::ShiftOperation, ExpressionBracketKind, VariableAccessComponent>;

        void                              recordExpressionComponent(const ExpressionComponent& expressionComponent);
        void                              switchMode(OperationMode newOperationMode);
        [[nodiscard]] std::optional<bool> recordedMatchingExpressionComponents() const;

    protected:
        OperationMode                    operationMode = OperationMode::Recording;
        std::vector<ExpressionComponent> expressionComponents;
        std::size_t                      indexForComparisonOfExpressionComponents = 0;
        std::optional<bool>              aggregateOfComparisonResultsOfExpressionComponents;
    };
} // namespace utils
