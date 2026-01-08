/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/first_variable_qubit_offset_lookup.hpp"
#include "ir/Definitions.hpp"

#include <gtest/gtest.h>
#include <optional>
#include <string_view>

using namespace syrec;

namespace {
    void assertFetchedQubitOffsetMatchesExpectedValue(const FirstVariableQubitOffsetLookup& qubitOffsetLookup, const std::string_view& variableIdentifier, const std::optional<qc::Qubit>& expectedQubitOffset) {
        const std::optional<qc::Qubit> actualQubitOffset = qubitOffsetLookup.getOffsetToFirstQubitOfVariableInCurrentScope(variableIdentifier);
        if (expectedQubitOffset.has_value()) {
            ASSERT_TRUE(actualQubitOffset.has_value());
            ASSERT_EQ(expectedQubitOffset.value(), actualQubitOffset.value()) << " Expected qubit offset mismatch for variable " << variableIdentifier;
        } else {
            ASSERT_FALSE(actualQubitOffset.has_value()) << "Expected to not be able to determine qubit offset for variable " << variableIdentifier;
        }
    }
} // namespace

TEST(FirstVariableQubitOffsetLookupTests, OpenNewScopeInEmptyLookup) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, "a", std::nullopt));
}

TEST(FirstVariableQubitOffsetLookupTests, OpenNewScopeInLookupAlreadyContainingEntries) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());

    constexpr std::string_view firstVariableOfFirstScopeIdentifier        = "a";
    constexpr qc::Qubit        expectedOffsetForFirstVariableOfFirstScope = 1U;

    constexpr std::string_view secondVariableOfFirstScopeIdentifier        = "b";
    constexpr qc::Qubit        expectedOffsetForSecondVariableOfFirstScope = 2U;

    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));

    constexpr std::string_view firstVariableOfSecondScopeIdentifier        = "c";
    constexpr qc::Qubit        expectedOffsetForFirstVariableOfSecondScope = 3U;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfSecondScopeIdentifier, expectedOffsetForFirstVariableOfSecondScope));

    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, std::nullopt));
}

TEST(FirstVariableQubitOffsetLookupTests, CloseScopeInEmptyLookup) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_FALSE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, "a", std::nullopt));
}

TEST(FirstVariableQubitOffsetLookupTests, CloseScopeInLookupAlreadyContainingEntries) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());

    constexpr std::string_view firstVariableOfFirstScopeIdentifier        = "a";
    constexpr qc::Qubit        expectedOffsetForFirstVariableOfFirstScope = 1U;

    constexpr std::string_view secondVariableOfFirstScopeIdentifier        = "b";
    constexpr qc::Qubit        expectedOffsetForSecondVariableOfFirstScope = 2U;

    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));

    constexpr std::string_view firstVariableOfSecondScopeIdentifier        = "c";
    constexpr qc::Qubit        expectedOffsetForFirstVariableOfSecondScope = 3U;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfSecondScopeIdentifier, expectedOffsetForFirstVariableOfSecondScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfSecondScopeIdentifier, expectedOffsetForFirstVariableOfSecondScope));

    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, std::nullopt));

    ASSERT_TRUE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfSecondScopeIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));

    ASSERT_TRUE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfSecondScopeIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, std::nullopt));
}

TEST(FirstVariableQubitOffsetLookupTests, RegisterAndUpdateAlreadyExistingVariableQubitOffset) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());

    constexpr std::string_view firstVariableOfFirstScopeIdentifier               = "a";
    constexpr qc::Qubit        expectedInitialOffsetForFirstVariableOfFirstScope = 1U;
    constexpr qc::Qubit        expectedUpdatedOffsetForFirstVariableOfFirstScope = 3U;

    constexpr std::string_view secondVariableOfFirstScopeIdentifier        = "b";
    constexpr qc::Qubit        expectedOffsetForSecondVariableOfFirstScope = 2U;

    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedInitialOffsetForFirstVariableOfFirstScope));
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedInitialOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));

    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedUpdatedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedUpdatedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
}

TEST(FirstVariableQubitOffsetLookupTests, RegisterAndUpdateAlreadyExistingVariableQubitOffsetWithMatchingEntryInCurrentAndParentScopeOnlyUpdatingEntryInCurrentScope) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());

    constexpr std::string_view firstVariableOfFirstScopeIdentifier               = "a";
    constexpr qc::Qubit        expectedInitialOffsetForFirstVariableOfFirstScope = 1U;
    constexpr qc::Qubit        expectedUpdatedOffsetForFirstVariableOfFirstScope = 3U;

    constexpr std::string_view secondVariableOfFirstScopeIdentifier        = "b";
    constexpr qc::Qubit        expectedOffsetForSecondVariableOfFirstScope = 2U;

    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedInitialOffsetForFirstVariableOfFirstScope));
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedInitialOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));

    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedUpdatedOffsetForFirstVariableOfFirstScope));

    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedUpdatedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, std::nullopt));

    ASSERT_TRUE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedInitialOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
}

TEST(FirstVariableQubitOffsetLookupTests, RegisteringQubitOffsetForEmptyVariableIdentifierNotPossible) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;
    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());

    constexpr std::string_view firstVariableOfFirstScopeIdentifier        = "a";
    constexpr qc::Qubit        expectedOffsetForFirstVariableOfFirstScope = 1U;

    constexpr std::string_view secondVariableOfFirstScopeIdentifier        = "b";
    constexpr qc::Qubit        expectedOffsetForSecondVariableOfFirstScope = 2U;

    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
    ASSERT_FALSE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope("", expectedOffsetForSecondVariableOfFirstScope));

    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableOfFirstScopeIdentifier, expectedOffsetForFirstVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableOfFirstScopeIdentifier, expectedOffsetForSecondVariableOfFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, "", std::nullopt));
}

TEST(FirstVariableQubitOffsetLookupTests, RegisteringQubitOffsetInEmptyLookupNotPossible) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;

    constexpr std::string_view variableIdentifier        = "a";
    constexpr qc::Qubit        expectedOffsetForVariable = 1U;
    ASSERT_FALSE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(variableIdentifier, expectedOffsetForVariable));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, variableIdentifier, std::nullopt));
}

TEST(FirstVariableQubitOffsetLookupTests, GetQubitOffsetForVariablesWithMatchingEntriesInMultipleScopes) {
    FirstVariableQubitOffsetLookup qubitOffsetLookup;

    constexpr std::string_view firstVariableIdentifier                    = "a";
    constexpr qc::Qubit        expectedOffsetForFirstVariableInFirstScope = 1U;
    constexpr qc::Qubit        expectedOffsetForFirstVariableInThirdScope = 3U;

    constexpr std::string_view secondVariableIdentifier                     = "b";
    constexpr qc::Qubit        expectedOffsetForSecondVariableInSecondScope = 2U;
    constexpr qc::Qubit        expectedOffsetForSecondVariableInFourthScope = 4U;

    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableIdentifier, expectedOffsetForFirstVariableInFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableIdentifier, expectedOffsetForFirstVariableInFirstScope));

    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableIdentifier, expectedOffsetForSecondVariableInSecondScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableIdentifier, expectedOffsetForSecondVariableInSecondScope));

    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(firstVariableIdentifier, expectedOffsetForFirstVariableInThirdScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableIdentifier, expectedOffsetForFirstVariableInThirdScope));

    ASSERT_NO_FATAL_FAILURE(qubitOffsetLookup.openNewVariableQubitOffsetScope());
    ASSERT_TRUE(qubitOffsetLookup.registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(secondVariableIdentifier, expectedOffsetForSecondVariableInFourthScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableIdentifier, expectedOffsetForSecondVariableInFourthScope));

    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableIdentifier, expectedOffsetForSecondVariableInFourthScope));

    ASSERT_TRUE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableIdentifier, expectedOffsetForFirstVariableInThirdScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableIdentifier, std::nullopt));

    ASSERT_TRUE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableIdentifier, std::nullopt));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableIdentifier, expectedOffsetForSecondVariableInSecondScope));

    ASSERT_TRUE(qubitOffsetLookup.closeVariableQubitOffsetScope());
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, firstVariableIdentifier, expectedOffsetForFirstVariableInFirstScope));
    ASSERT_NO_FATAL_FAILURE(assertFetchedQubitOffsetMatchesExpectedValue(qubitOffsetLookup, secondVariableIdentifier, std::nullopt));
}
