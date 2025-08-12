/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/statement_execution_order_stack.hpp"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>

using namespace syrec;

namespace {
    void assertExecutionOrderStatesMatch(const std::optional<StatementExecutionOrderStack::StatementExecutionOrder>& expected, const std::optional<StatementExecutionOrderStack::StatementExecutionOrder>& actual) {
        if (expected.has_value()) {
            ASSERT_TRUE(actual.has_value()) << "Expected statement execution order state to have a value";
            ASSERT_EQ(*expected, *actual) << "Statement execution order state mismatch";
        } else {
            ASSERT_FALSE(actual.has_value()) << "Expected statement execution order state to not have a value";
        }
    }
} // namespace

TEST(StatementExecutionOrderStackTests, InitializationSetsAggregateStateToSequentialExecution) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(StatementExecutionOrderStack::StatementExecutionOrder::Sequential, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, GetAggregateExecutionOrderStateOfEmptyStack) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(std::nullopt, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, AddExecutionStateToEmptyStack) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(std::nullopt, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(StatementExecutionOrderStack::StatementExecutionOrder::Sequential, executionOrderStack->addStatementExecutionOrderToAggregateState(StatementExecutionOrderStack::StatementExecutionOrder::Sequential)));
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse, executionOrderStack->addStatementExecutionOrderToAggregateState(StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse)));
}

TEST(StatementExecutionOrderStackTests, AddSequentialExecutionStateToAggregateStateEqualToSequentialExecution) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(StatementExecutionOrderStack::StatementExecutionOrder::Sequential, executionOrderStack->addStatementExecutionOrderToAggregateState(StatementExecutionOrderStack::StatementExecutionOrder::Sequential)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(StatementExecutionOrderStack::StatementExecutionOrder::Sequential, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, AddSequentialExecutionStateToAggregateStateEqualToInverseExecution) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());

    constexpr auto expectedAggregateStmtExecutionOrderState = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->addStatementExecutionOrderToAggregateState(expectedAggregateStmtExecutionOrderState)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    constexpr auto stmtExecutionOrderStateToAdd = StatementExecutionOrderStack::StatementExecutionOrder::Sequential;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->addStatementExecutionOrderToAggregateState(stmtExecutionOrderStateToAdd)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, AddInverseExecutionStateToAggregateStateEqualToSequentialExecution) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(StatementExecutionOrderStack::StatementExecutionOrder::Sequential, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    constexpr auto stmtExecutionOrderStateToAdd             = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    constexpr auto expectedAggregateStmtExecutionOrderState = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->addStatementExecutionOrderToAggregateState(stmtExecutionOrderStateToAdd)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, AddInverseExecutionStateToAggregateStateEqualToInverseExecution) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());

    constexpr auto initialAggregateStmtExecutionOrderState = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(initialAggregateStmtExecutionOrderState, executionOrderStack->addStatementExecutionOrderToAggregateState(initialAggregateStmtExecutionOrderState)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(initialAggregateStmtExecutionOrderState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    constexpr auto stmtExecutionOrderStateToAdd             = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    constexpr auto expectedAggregateStmtExecutionOrderState = StatementExecutionOrderStack::StatementExecutionOrder::Sequential;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->addStatementExecutionOrderToAggregateState(stmtExecutionOrderStateToAdd)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedAggregateStmtExecutionOrderState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, RemoveExecutionOrderStateFromEmptyStack) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_FALSE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
}

TEST(StatementExecutionOrderStackTests, RemoveExecutionOrderStateFromStackContainingSingleEntry) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());

    constexpr auto sequentialStmtExecutionOrder = StatementExecutionOrderStack::StatementExecutionOrder::Sequential;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(sequentialStmtExecutionOrder, executionOrderStack->addStatementExecutionOrderToAggregateState(sequentialStmtExecutionOrder)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(sequentialStmtExecutionOrder, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());

    constexpr auto invertedStmtExecutionOrder = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(invertedStmtExecutionOrder, executionOrderStack->addStatementExecutionOrderToAggregateState(invertedStmtExecutionOrder)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(invertedStmtExecutionOrder, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
}

TEST(StatementExecutionOrderStackTests, RemoveExecutionOrderStateFromStackCausingInversionOfAggregateState) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    constexpr auto expectedInitialAggregateState = StatementExecutionOrderStack::StatementExecutionOrder::Sequential;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    constexpr auto firstStmtExecutionOrderCausesAggregateStateFlip = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    constexpr auto firstExpectedAggregateState                     = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(firstExpectedAggregateState, executionOrderStack->addStatementExecutionOrderToAggregateState(firstStmtExecutionOrderCausesAggregateStateFlip)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(firstExpectedAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    constexpr auto secondStmtExecutionOrderCausesAggregateStateFlip = StatementExecutionOrderStack::StatementExecutionOrder::InvertedAndInReverse;
    constexpr auto secondExpectedAggregateState                     = StatementExecutionOrderStack::StatementExecutionOrder::Sequential;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(secondExpectedAggregateState, executionOrderStack->addStatementExecutionOrderToAggregateState(secondStmtExecutionOrderCausesAggregateStateFlip)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(secondExpectedAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(firstExpectedAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}

TEST(StatementExecutionOrderStackTests, RemoveExecutionOrderStateFromStackNotCausingInversionOfAggregateState) {
    auto executionOrderStack = std::make_unique<StatementExecutionOrderStack>();
    ASSERT_THAT(executionOrderStack, testing::NotNull());
    constexpr auto expectedInitialAggregateState = StatementExecutionOrderStack::StatementExecutionOrder::Sequential;
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->addStatementExecutionOrderToAggregateState(expectedInitialAggregateState)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->addStatementExecutionOrderToAggregateState(expectedInitialAggregateState)));
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));

    ASSERT_TRUE(executionOrderStack->removeLastAddedStatementExecutionOrderFromAggregateState());
    ASSERT_NO_FATAL_FAILURE(assertExecutionOrderStatesMatch(expectedInitialAggregateState, executionOrderStack->getCurrentAggregateStatementExecutionOrderState()));
}
