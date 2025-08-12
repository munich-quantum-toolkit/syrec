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

#include <optional>
#include <vector>

namespace syrec {
    class StatementExecutionOrderStack {
    public:
        enum class StatementExecutionOrder : bool {
            Sequential           = false,
            InvertedAndInReverse = true
        };

        friend constexpr StatementExecutionOrder operator!(StatementExecutionOrder executionOrder) noexcept {
            return executionOrder == StatementExecutionOrder::Sequential ? StatementExecutionOrder::InvertedAndInReverse : StatementExecutionOrder::Sequential;
        }

        StatementExecutionOrderStack() {
            statementExecutionOrderAggregateState = StatementExecutionOrder::Sequential;
            addStatementExecutionOrderToAggregateState(StatementExecutionOrder::Sequential);
        }

        [[nodiscard]] std::optional<StatementExecutionOrder> getCurrentAggregateStatementExecutionOrderState() const noexcept {
            return !statementExecutionOrderStack.empty() ? std::make_optional(statementExecutionOrderAggregateState) : std::nullopt;
        }

        [[maybe_unused]] StatementExecutionOrder addStatementExecutionOrderToAggregateState(StatementExecutionOrder executionOrder) {
            statementExecutionOrderStack.emplace_back(executionOrder);
            statementExecutionOrderAggregateState = combineStates(statementExecutionOrderAggregateState, executionOrder);
            return statementExecutionOrderAggregateState;
        }

        [[maybe_unused]] bool removeLastAddedStatementExecutionOrderFromAggregateState() {
            if (statementExecutionOrderStack.empty()) {
                return false;
            }

            const StatementExecutionOrder lastAddedStatementExecutionOrder = statementExecutionOrderStack.back();
            statementExecutionOrderStack.pop_back();
            statementExecutionOrderAggregateState = combineStates(statementExecutionOrderAggregateState, lastAddedStatementExecutionOrder);
            return true;
        }

    protected:
        StatementExecutionOrder              statementExecutionOrderAggregateState;
        std::vector<StatementExecutionOrder> statementExecutionOrderStack;

        [[maybe_unused]] static StatementExecutionOrder combineStates(StatementExecutionOrder curr, StatementExecutionOrder toBeAdded) noexcept {
            return static_cast<StatementExecutionOrder>(static_cast<bool>(curr) ^ static_cast<bool>(toBeAdded));
        }
    };
} // namespace syrec
