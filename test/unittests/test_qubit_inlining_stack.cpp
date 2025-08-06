/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/qubit_inlining_stack.hpp"
#include "core/syrec/module.hpp"

#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace syrec;

namespace {
    void assertInlineStackEntriesMatch(const QubitInliningStack::QubitInliningStackEntry& expected, const QubitInliningStack::QubitInliningStackEntry& actual) {
        if (expected.lineNumberOfCallOfTargetModule.has_value()) {
            ASSERT_TRUE(actual.lineNumberOfCallOfTargetModule.has_value()) << "Expected source code line number of called target module to not have a value";
            ASSERT_EQ(*expected.lineNumberOfCallOfTargetModule, *actual.lineNumberOfCallOfTargetModule) << "Source code line number of called target module mismatch";
        } else {
            ASSERT_FALSE(actual.lineNumberOfCallOfTargetModule.has_value()) << "Expected source code line number of called target module to not have a value";
        }

        if (expected.isTargetModuleAccessedViaCallStmt.has_value()) {
            ASSERT_TRUE(expected.isTargetModuleAccessedViaCallStmt.has_value()) << "Expected call type of target module to be specified";
            ASSERT_EQ(*expected.isTargetModuleAccessedViaCallStmt, actual.isTargetModuleAccessedViaCallStmt) << "Call type of target module mismatch";
        } else {
            ASSERT_FALSE(actual.isTargetModuleAccessedViaCallStmt.has_value()) << "Expected call type of target module not to be specified";
        }

        if (expected.targetModule != nullptr) {
            ASSERT_THAT(actual.targetModule, testing::NotNull()) << "Expected target module to be set";
            ASSERT_EQ(actual.targetModule, expected.targetModule) << "Target module reference mismatch";
        } else {
            ASSERT_THAT(actual.targetModule, testing::IsNull()) << "Expected target module to not be set";
        }
    }
    void assertInlineStackEntriesAre(QubitInliningStack& inlineStackToCheck, const std::vector<QubitInliningStack::QubitInliningStackEntry>& expectedInlineStackEntries) {
        ASSERT_EQ(expectedInlineStackEntries.size(), inlineStackToCheck.size()) << "Expected inline stack to have " << std::to_string(expectedInlineStackEntries.size()) << " entries but actually had only " << std::to_string(inlineStackToCheck.size());
        for (std::size_t i = 0; i < expectedInlineStackEntries.size(); ++i) {
            const auto* inlineStackEntryAtIdx = inlineStackToCheck.getStackEntryAt(i);
            ASSERT_THAT(inlineStackEntryAtIdx, testing::NotNull());
            ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(*inlineStackEntryAtIdx, expectedInlineStackEntries.at(i)));
        }
    }
} // namespace

// BEGIN pop tests
TEST(QubitInliningStackTests, PopFromEmptyStack) {
    auto inlineStack = QubitInliningStack();
    ASSERT_FALSE(inlineStack.pop());
    ASSERT_EQ(0, inlineStack.size());
}

TEST(QubitInliningStackTests, PopFromNonEmptyStack) {
    auto inlineStack = QubitInliningStack();

    auto targetModule = std::make_shared<Module>("targetModule");

    auto firstInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    firstInlineStackEntry.lineNumberOfCallOfTargetModule    = 1;
    firstInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    firstInlineStackEntry.targetModule                      = targetModule;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));

    auto secondInlineStackEntry         = QubitInliningStack::QubitInliningStackEntry();
    secondInlineStackEntry.targetModule = targetModule;
    ASSERT_TRUE(inlineStack.push(secondInlineStackEntry));

    const std::vector expectedInlineStackEntries = {firstInlineStackEntry, secondInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}
// END pop tests

// BEGIN push tests
TEST(QubitInliningStackTests, PushToEmptyStack) {
    auto inlineStack = QubitInliningStack();

    auto targetModule                               = std::make_shared<Module>("targetModule");
    auto pushedStackEntry                           = QubitInliningStack::QubitInliningStackEntry();
    pushedStackEntry.targetModule                   = targetModule;
    pushedStackEntry.lineNumberOfCallOfTargetModule = 1;
    ASSERT_TRUE(inlineStack.push(pushedStackEntry));

    const std::vector expectedInlineStackEntries = {pushedStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}

TEST(QubitInliningStackTests, PushEntryWithInvalidTargetModuleNotPossible) {
    auto inlineStack = QubitInliningStack();

    auto targetModule                              = std::make_shared<Module>("targetModule");
    auto validStackEntry                           = QubitInliningStack::QubitInliningStackEntry();
    validStackEntry.targetModule                   = targetModule;
    validStackEntry.lineNumberOfCallOfTargetModule = 1;
    ASSERT_TRUE(inlineStack.push(validStackEntry));

    const std::vector expectedInlineStackEntries = {validStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    auto invalidStackEntry         = QubitInliningStack::QubitInliningStackEntry();
    invalidStackEntry.targetModule = nullptr;
    ASSERT_FALSE(inlineStack.push(invalidStackEntry));
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}

TEST(QubitInliningStackTests, PushEntryWithEmptyCallTypeIdentifierPossible) {
    auto inlineStack = QubitInliningStack();

    auto targetModule                                                   = std::make_shared<Module>("targetModule");
    auto firstFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    firstFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    firstFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    firstFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(inlineStack.push(firstFullyDefinedInlineStackEntry));

    auto secondFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    secondFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = false;
    secondFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 2;
    ASSERT_TRUE(inlineStack.push(secondFullyDefinedInlineStackEntry));

    std::vector expectedInlineStackEntries = {firstFullyDefinedInlineStackEntry, secondFullyDefinedInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    auto inlineStackEntryMissingCallTypeIdentifier                           = QubitInliningStack::QubitInliningStackEntry();
    inlineStackEntryMissingCallTypeIdentifier.targetModule                   = targetModule;
    inlineStackEntryMissingCallTypeIdentifier.lineNumberOfCallOfTargetModule = 3;
    ASSERT_TRUE(inlineStack.push(inlineStackEntryMissingCallTypeIdentifier));

    expectedInlineStackEntries.emplace_back(inlineStackEntryMissingCallTypeIdentifier);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}

TEST(QubitInliningStackTests, PushEntryWithEmptyTargetModuleSourceCodeLineNumberPossible) {
    auto inlineStack = QubitInliningStack();

    auto targetModule                                                   = std::make_shared<Module>("targetModule");
    auto firstFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    firstFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    firstFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    firstFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(inlineStack.push(firstFullyDefinedInlineStackEntry));

    auto secondFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    secondFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = false;
    secondFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 2;
    ASSERT_TRUE(inlineStack.push(secondFullyDefinedInlineStackEntry));

    std::vector expectedInlineStackEntries = {firstFullyDefinedInlineStackEntry, secondFullyDefinedInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    auto inlineStackEntryMissingSourceCodeLineNumber                              = QubitInliningStack::QubitInliningStackEntry();
    inlineStackEntryMissingSourceCodeLineNumber.targetModule                      = targetModule;
    inlineStackEntryMissingSourceCodeLineNumber.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(inlineStackEntryMissingSourceCodeLineNumber));

    expectedInlineStackEntries.emplace_back(inlineStackEntryMissingSourceCodeLineNumber);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}

TEST(QubitInliningStackTests, PushToNonEmptyStack) {
    auto inlineStack = QubitInliningStack();

    std::vector<QubitInliningStack::QubitInliningStackEntry> expectedInlineStackEntries;

    auto targetModule                                                   = std::make_shared<Module>("targetModule");
    auto firstFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    firstFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    firstFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    firstFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(inlineStack.push(firstFullyDefinedInlineStackEntry));

    expectedInlineStackEntries.emplace_back(firstFullyDefinedInlineStackEntry);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    auto secondFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    secondFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = false;
    secondFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 2;
    ASSERT_TRUE(inlineStack.push(secondFullyDefinedInlineStackEntry));

    expectedInlineStackEntries.emplace_back(secondFullyDefinedInlineStackEntry);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}

TEST(QubitInliningStackTests, PushAndPopOperationSequence) {
    auto inlineStack = QubitInliningStack();

    std::vector<QubitInliningStack::QubitInliningStackEntry> expectedInlineStackEntries;

    auto targetModule                                                   = std::make_shared<Module>("targetModule");
    auto firstFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    firstFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    firstFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    firstFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 1;
    ASSERT_TRUE(inlineStack.push(firstFullyDefinedInlineStackEntry));

    expectedInlineStackEntries.emplace_back(firstFullyDefinedInlineStackEntry);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    ASSERT_TRUE(inlineStack.pop());
    expectedInlineStackEntries.clear();
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    auto secondFullyDefinedInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondFullyDefinedInlineStackEntry.targetModule                      = targetModule;
    secondFullyDefinedInlineStackEntry.isTargetModuleAccessedViaCallStmt = false;
    secondFullyDefinedInlineStackEntry.lineNumberOfCallOfTargetModule    = 2;
    ASSERT_TRUE(inlineStack.push(secondFullyDefinedInlineStackEntry));

    expectedInlineStackEntries.emplace_back(secondFullyDefinedInlineStackEntry);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    ASSERT_TRUE(inlineStack.pop());
    expectedInlineStackEntries.clear();
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
    ASSERT_FALSE(inlineStack.pop());
}
// END push tests

// BEGIN size tests
TEST(QubitInliningStackTests, GetSizeOfEmptyStack) {
    ASSERT_EQ(0, QubitInliningStack().size());
}

TEST(QubitInliningStackTests, GetSizeOfNonEmptyStack) {
    auto       inlineStack                               = QubitInliningStack();
    const auto firstTargetModule                         = std::make_shared<Module>("targetModule_1");
    auto       firstInlineStackEntry                     = QubitInliningStack::QubitInliningStackEntry();
    firstInlineStackEntry.targetModule                   = firstTargetModule;
    firstInlineStackEntry.lineNumberOfCallOfTargetModule = 1;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));
    ASSERT_EQ(1, inlineStack.size());

    const auto secondTargetModule                            = std::make_shared<Module>("targetModule_2");
    auto       secondInlineStackEntry                        = QubitInliningStack::QubitInliningStackEntry();
    secondInlineStackEntry.targetModule                      = secondTargetModule;
    secondInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(secondInlineStackEntry));
    ASSERT_EQ(2, inlineStack.size());

    const std::vector expectedInlineStackEntries = {firstInlineStackEntry, secondInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));
}
// END size tests

// BEGIN get entry at idx tests
TEST(QubitInliningStackTests, GetElementAtIndexOutOfRangeInNonEmptyStack) {
    auto       inlineStack                               = QubitInliningStack();
    const auto firstTargetModule                         = std::make_shared<Module>("targetModule_1");
    auto       firstInlineStackEntry                     = QubitInliningStack::QubitInliningStackEntry();
    firstInlineStackEntry.targetModule                   = firstTargetModule;
    firstInlineStackEntry.lineNumberOfCallOfTargetModule = 1;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));

    const auto secondTargetModule                            = std::make_shared<Module>("targetModule_2");
    auto       secondInlineStackEntry                        = QubitInliningStack::QubitInliningStackEntry();
    secondInlineStackEntry.targetModule                      = secondTargetModule;
    secondInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(secondInlineStackEntry));

    const std::vector expectedInlineStackEntries = {firstInlineStackEntry, secondInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    QubitInliningStack::QubitInliningStackEntry* fetchedStackEntry = nullptr;
    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(2));
    ASSERT_THAT(fetchedStackEntry, testing::IsNull());

    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(inlineStack.size() * 2));
    ASSERT_THAT(fetchedStackEntry, testing::IsNull());
}

TEST(QubitInliningStackTests, GetElementInEmptyStack) {
    auto                                         inlineStack       = QubitInliningStack();
    QubitInliningStack::QubitInliningStackEntry* fetchedStackEntry = nullptr;
    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(0));
    ASSERT_THAT(fetchedStackEntry, testing::IsNull());
}

TEST(QubitInliningStackTests, GetElementAtPoppedIndexFromStack) {
    auto inlineStack = QubitInliningStack();

    const auto targetModule                                 = std::make_shared<Module>("targetModule");
    auto       firstInlineStackEntry                        = QubitInliningStack::QubitInliningStackEntry();
    firstInlineStackEntry.targetModule                      = targetModule;
    firstInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));

    auto secondInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondInlineStackEntry.targetModule                      = targetModule;
    secondInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));

    const std::vector expectedInlineStackEntries = {firstInlineStackEntry, secondInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    QubitInliningStack::QubitInliningStackEntry* fetchedStackEntry      = nullptr;
    constexpr std::size_t                        toBeFetchStackEntryIdx = 1;
    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(toBeFetchStackEntryIdx));
    ASSERT_THAT(fetchedStackEntry, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(secondInlineStackEntry, *fetchedStackEntry));

    ASSERT_TRUE(inlineStack.pop());
    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(toBeFetchStackEntryIdx));
    ASSERT_THAT(fetchedStackEntry, testing::IsNull());
}

TEST(QubitInliningStackTests, GetElementAtNewlyPushedIndexFromStack) {
    auto inlineStack = QubitInliningStack();

    const auto targetModule                                 = std::make_shared<Module>("targetModule");
    auto       firstInlineStackEntry                        = QubitInliningStack::QubitInliningStackEntry();
    firstInlineStackEntry.targetModule                      = targetModule;
    firstInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));

    std::vector expectedInlineStackEntries = {firstInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    QubitInliningStack::QubitInliningStackEntry* fetchedStackEntry        = nullptr;
    constexpr std::size_t                        toBeFetchedStackEntryIdx = 1;

    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(toBeFetchedStackEntryIdx));
    ASSERT_THAT(fetchedStackEntry, testing::IsNull());

    auto secondInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondInlineStackEntry.targetModule                      = targetModule;
    secondInlineStackEntry.isTargetModuleAccessedViaCallStmt = false;
    ASSERT_TRUE(inlineStack.push(secondInlineStackEntry));

    expectedInlineStackEntries.emplace_back(secondInlineStackEntry);
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(toBeFetchedStackEntryIdx));
    ASSERT_THAT(fetchedStackEntry, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(secondInlineStackEntry, *fetchedStackEntry));
}

TEST(QubitInliningStackTests, GetElementAtVariousIndicesOfStack) {
    auto inlineStack = QubitInliningStack();

    const auto targetModule                                 = std::make_shared<Module>("targetModule");
    auto       firstInlineStackEntry                        = QubitInliningStack::QubitInliningStackEntry();
    firstInlineStackEntry.targetModule                      = targetModule;
    firstInlineStackEntry.isTargetModuleAccessedViaCallStmt = true;
    ASSERT_TRUE(inlineStack.push(firstInlineStackEntry));

    auto secondInlineStackEntry                              = QubitInliningStack::QubitInliningStackEntry();
    secondInlineStackEntry.targetModule                      = targetModule;
    secondInlineStackEntry.isTargetModuleAccessedViaCallStmt = false;
    ASSERT_TRUE(inlineStack.push(secondInlineStackEntry));

    auto thirdInlineStackEntry                           = QubitInliningStack::QubitInliningStackEntry();
    thirdInlineStackEntry.targetModule                   = targetModule;
    thirdInlineStackEntry.lineNumberOfCallOfTargetModule = 1;
    ASSERT_TRUE(inlineStack.push(thirdInlineStackEntry));

    const std::vector expectedInlineStackEntries = {firstInlineStackEntry, secondInlineStackEntry, thirdInlineStackEntry};
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesAre(inlineStack, expectedInlineStackEntries));

    QubitInliningStack::QubitInliningStackEntry* fetchedStackEntry = nullptr;
    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(0));
    ASSERT_THAT(fetchedStackEntry, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(firstInlineStackEntry, *fetchedStackEntry));

    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(1));
    ASSERT_THAT(fetchedStackEntry, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(secondInlineStackEntry, *fetchedStackEntry));

    ASSERT_NO_FATAL_FAILURE(fetchedStackEntry = inlineStack.getStackEntryAt(2));
    ASSERT_THAT(fetchedStackEntry, testing::NotNull());
    ASSERT_NO_FATAL_FAILURE(assertInlineStackEntriesMatch(thirdInlineStackEntry, *fetchedStackEntry));
}
// END get entry at idx tests
