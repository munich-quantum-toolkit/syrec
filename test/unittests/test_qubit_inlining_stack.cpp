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
#include "core/syrec/expression.hpp"
#include "core/syrec/module.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

    void assertStringifiedModuleSignaturesMatch(const QubitInliningStack::QubitInliningStackEntry& stackEntryToStringify, const std::string_view& expected) {
        const std::optional<std::string>& stringifiedTargetModuleSignature = stackEntryToStringify.stringifySignatureOfCalledModule();
        ASSERT_TRUE(stringifiedTargetModuleSignature.has_value()) << "Expected to be able to stringify the signature of the target module";
        ASSERT_EQ(expected, *stringifiedTargetModuleSignature) << "Stringified module signatures did not match";
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

// BEGIN stringification of target module signature tests
TEST(QubitInliningStackTests, StringificationModuleSignatureWithTargetModuleNotSetIsNotPossible) {
    const auto stackEntry = QubitInliningStack::QubitInliningStackEntry();
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());
}

TEST(QubitInliningStackTests, StringificationModuleSignatureWithEmptyTargetModuleIdentifierIsNotPossible) {
    const auto targetModule = std::make_shared<Module>("");
    auto       stackEntry   = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithInvalidParameterNotPossible) {
    const auto targetModule               = std::make_shared<Module>("main");
    auto       validParameterDefinition   = std::make_shared<Variable>(Variable::Type::In, "a", std::vector({1U}), 4U);
    auto       invalidParameterDefinition = nullptr;
    targetModule->addParameter(validParameterDefinition);
    targetModule->addParameter(invalidParameterDefinition);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithParameterOfNonParameterTypeNotPossible) {
    const auto targetModuleWithParameterOfTypeWire = std::make_shared<Module>("param_type_wire_module");
    auto       wireTypeParameter                   = std::make_shared<Variable>(Variable::Type::Wire, "a", std::vector({1U}), 4U);
    targetModuleWithParameterOfTypeWire->addParameter(wireTypeParameter);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModuleWithParameterOfTypeWire;
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());

    const auto targetModuleWithParameterOfTypeState = std::make_shared<Module>("param_type_state_module");
    auto       stateTypeParameter                   = std::make_shared<Variable>(Variable::Type::State, "a", std::vector({1U}), 4U);
    targetModuleWithParameterOfTypeState->addParameter(stateTypeParameter);

    stackEntry.targetModule = targetModuleWithParameterOfTypeState;
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithEmptyParameterIdentifierNotPossible) {
    const auto targetModule               = std::make_shared<Module>("main");
    auto       validParameterDefinition   = std::make_shared<Variable>(Variable::Type::In, "a", std::vector({1U}), 4U);
    auto       invalidParameterDefinition = std::make_shared<Variable>(Variable::Type::Inout, "", std::vector({2U}), 4U);
    targetModule->addParameter(validParameterDefinition);
    targetModule->addParameter(invalidParameterDefinition);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithParameterWithEmptyDimensionDeclarationNotPossible) {
    const auto targetModule               = std::make_shared<Module>("main");
    auto       validParameterDefinition   = std::make_shared<Variable>(Variable::Type::In, "a", std::vector({1U}), 4U);
    auto       invalidParameterDefinition = std::make_shared<Variable>(Variable::Type::Inout, "b", std::vector<unsigned int>(), 4U);
    targetModule->addParameter(validParameterDefinition);
    targetModule->addParameter(invalidParameterDefinition);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;
    ASSERT_FALSE(stackEntry.stringifySignatureOfCalledModule().has_value());
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithNoParameters) {
    const auto targetModule = std::make_shared<Module>("main");
    auto       stackEntry   = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main()"));
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithParameterOfTypeIn) {
    const auto targetModule      = std::make_shared<Module>("main");
    auto       parameterOfTypeIn = std::make_shared<Variable>(Variable::Type::In, "a", std::vector({1U}), 4U);
    targetModule->addParameter(parameterOfTypeIn);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main(in a[1](4))"));
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithParameterOfTypeOut) {
    const auto targetModule       = std::make_shared<Module>("main");
    auto       parameterOfTypeOut = std::make_shared<Variable>(Variable::Type::Out, "a", std::vector({1U}), 4U);
    targetModule->addParameter(parameterOfTypeOut);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main(out a[1](4))"));
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithParameterOfTypeInout) {
    const auto targetModule         = std::make_shared<Module>("main");
    auto       parameterOfTypeInout = std::make_shared<Variable>(Variable::Type::Inout, "a", std::vector({1U}), 4U);
    targetModule->addParameter(parameterOfTypeInout);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main(inout a[1](4))"));
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithNDimensionalParameter) {
    const auto targetModule         = std::make_shared<Module>("main");
    auto       parameterOfTypeInout = std::make_shared<Variable>(Variable::Type::Inout, "a", std::vector({2U, 3U, 1U}), 4U);
    targetModule->addParameter(parameterOfTypeInout);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main(inout a[2][3][1](4))"));
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureWithMultipleParameters) {
    const auto targetModule            = std::make_shared<Module>("main");
    auto       parameterOfTypeInout    = std::make_shared<Variable>(Variable::Type::Inout, "a", std::vector({2U, 1U}), 2U);
    auto       firstParameterOfTypeIn  = std::make_shared<Variable>(Variable::Type::In, "b", std::vector({3U}), 3U);
    auto       secondParameterOfTypeIn = std::make_shared<Variable>(Variable::Type::In, "c", std::vector({1U}), 4U);
    targetModule->addParameter(parameterOfTypeInout);
    targetModule->addParameter(firstParameterOfTypeIn);
    targetModule->addParameter(secondParameterOfTypeIn);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main(inout a[2][1](2), in b[3](3), in c[1](4))"));
}

TEST(QubitInliningStackTests, StringificationOfModuleSignatureDoesNotStringifiyStatementsOfModuleBody) {
    const auto targetModule        = std::make_shared<Module>("main");
    auto       assignableParameter = std::make_shared<Variable>(Variable::Type::Inout, "a", std::vector({2U}), 3U);
    auto       readonlyParameter   = std::make_shared<Variable>(Variable::Type::In, "b", std::vector({1U}), 3U);
    targetModule->addParameter(assignableParameter);
    targetModule->addParameter(readonlyParameter);

    const auto exprDefiningAccessedValueOfDimension = std::make_shared<NumericExpression>(std::make_shared<Number>(0U), 1U);

    auto assignmentLhsOperand = std::make_shared<VariableAccess>();
    assignmentLhsOperand->var = assignableParameter;
    assignmentLhsOperand->indexes.emplace_back(exprDefiningAccessedValueOfDimension);

    auto assignmentRhsOperand = std::make_shared<VariableAccess>();
    assignmentRhsOperand->var = readonlyParameter;
    assignmentRhsOperand->indexes.emplace_back(exprDefiningAccessedValueOfDimension);

    const auto assignmentRhsExpr   = std::make_shared<VariableExpression>(assignmentRhsOperand);
    const auto assignmentStmt      = std::make_shared<AssignStatement>(assignmentLhsOperand, AssignStatement::AssignOperation::Add, assignmentRhsExpr);
    const auto unaryAssignmentStmt = std::make_shared<UnaryStatement>(UnaryStatement::UnaryOperation::Increment, assignmentLhsOperand);
    targetModule->addStatement(assignmentStmt);
    targetModule->addStatement(unaryAssignmentStmt);

    auto stackEntry         = QubitInliningStack::QubitInliningStackEntry();
    stackEntry.targetModule = targetModule;

    ASSERT_NO_FATAL_FAILURE(assertStringifiedModuleSignaturesMatch(stackEntry, "module main(inout a[2](3), in b[1](3))"));
}
// END stringification of target module signature tests
