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
    /**
     * A utility class to get/modify/store the active statement execution order of a statement block for a call stack of SyReC Call-/UncallStatements.
     */
    class StatementExecutionOrderStack {
    public:
        /**
         * An scoped enumeration to define the execution order for a SyReC statement block
         */
        enum class StatementExecutionOrder : bool {
            /**
             * Perform a sequential execution of the statements in a statement block.
             */
            Sequential = false,
            /**
             * Execute the statements of a statement block starting at the last statement up to and including the first statement of the block with each entry of the block being inverted prior to its execution.
             */
            InvertedAndInReverse = true
        };

        /**
         * Initializes a new statement execution order stack with one sequential execution order state entry.
         */
        StatementExecutionOrderStack() {
            addStatementExecutionOrderToAggregateState(StatementExecutionOrder::Sequential);
        }

        /**
         * Get the current aggregate statement execution order state.
         * @return The current aggregate statement execution order state or std::nullopt if the stack is empty.
         */
        [[nodiscard]] std::optional<StatementExecutionOrder> getCurrentAggregateStatementExecutionOrderState() const noexcept {
            return !statementExecutionOrderStack.empty() ? std::make_optional(statementExecutionOrderAggregateState) : std::nullopt;
        }

        /**
         * Add a new statement execution order state to the aggregate state.
         *
         * The update of the aggregate state is done according to the following rules:
         * - Aggregate: SEQUENTIAL + new: SEQUENTIAL => Aggregate: SEQUENTIAL
         * - Aggregate: SEQUENTIAL + new: INVERTED => Aggregate: INVERTED
         * - Aggregate: INVERTED + new: SEQUENTIAL => Aggregate: INVERTED
         * - Aggregate: INVERTED + new: INVERTED => Aggregate: SEQUENTIAL
         * @param executionOrder The statement execution order state to add to the aggregate state
         * @return The new aggregate state.
         */
        [[maybe_unused]] StatementExecutionOrder addStatementExecutionOrderToAggregateState(StatementExecutionOrder executionOrder) {
            statementExecutionOrderStack.emplace_back(executionOrder);
            statementExecutionOrderAggregateState = combineStates(statementExecutionOrderAggregateState, executionOrder);
            return statementExecutionOrderAggregateState;
        }

        /**
         * Removes the last added statement execution order state from the internal stack and updates the aggregate state.
         *
         * The update of the aggregate state is done according to the following rules:
         * - Aggregate: SEQUENTIAL - removed entry: SEQUENTIAL => Aggregate: SEQUENTIAL
         * - Aggregate: SEQUENTIAL - removed entry: INVERTED => Aggregate: INVERTED
         * - Aggregate: INVERTED - removed entry: SEQUENTIAL => Aggregate: INVERTED
         * - Aggregate: INVERTED - removed entry: INVERTED => Aggregate: SEQUENTIAL
         *
         * The aggregate state is only updated if an entry on the internal stack exists.
         * @return Whether an entry was popped from the internal stack.
         */
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
        StatementExecutionOrder              statementExecutionOrderAggregateState = StatementExecutionOrder::Sequential;
        std::vector<StatementExecutionOrder> statementExecutionOrderStack;

        [[maybe_unused]] static StatementExecutionOrder combineStates(StatementExecutionOrder currentAggregateState, StatementExecutionOrder toBeAdded) noexcept {
            return static_cast<StatementExecutionOrder>(static_cast<bool>(currentAggregateState) ^ static_cast<bool>(toBeAdded));
        }

        friend constexpr StatementExecutionOrder operator!(StatementExecutionOrder executionOrder) noexcept {
            return executionOrder == StatementExecutionOrder::Sequential ? StatementExecutionOrder::InvertedAndInReverse : StatementExecutionOrder::Sequential;
        }
    };
} // namespace syrec
