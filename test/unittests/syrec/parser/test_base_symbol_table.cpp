/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/module.hpp"
#include "core/syrec/parser/utils/symbolTable/base_symbol_table.hpp"
#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <algorithm>
#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace utils;

namespace {
    constexpr unsigned int          DEFAULT_BITWIDTH          = 16;
    const std::vector<unsigned int> DEFAULT_SIGNAL_DIMENSIONS = {2, 3};

    class VariableTypeAmbiguityDuringModuleInsertionTestFixture: public ::testing::TestWithParam<std::pair<syrec::Variable::Type, syrec::Variable::Type>> {
    protected:
        BaseSymbolTable           symbolTable;
        syrec::Variable::Type     firstModuleParameterType;
        syrec::Variable::Type     secondModuleParameterType;
        unsigned int              defaultSignalBitwidth;
        std::vector<unsigned int> defaultSignalDimensions;

        VariableTypeAmbiguityDuringModuleInsertionTestFixture():
            firstModuleParameterType(std::get<0>(GetParam())), secondModuleParameterType(std::get<1>(GetParam())),
            defaultSignalBitwidth(DEFAULT_BITWIDTH), defaultSignalDimensions(DEFAULT_SIGNAL_DIMENSIONS) {}

        explicit VariableTypeAmbiguityDuringModuleInsertionTestFixture(const std::pair<syrec::Variable::Type, syrec::Variable::Type>& moduleParameterData):
            firstModuleParameterType(moduleParameterData.first), secondModuleParameterType(moduleParameterData.second),
            defaultSignalBitwidth(DEFAULT_BITWIDTH), defaultSignalDimensions(DEFAULT_SIGNAL_DIMENSIONS) {}
    };

    class NoVariableTypeAmbiguityDuringModuleInsertionTestFixture: public VariableTypeAmbiguityDuringModuleInsertionTestFixture {
    protected:
        NoVariableTypeAmbiguityDuringModuleInsertionTestFixture():
            VariableTypeAmbiguityDuringModuleInsertionTestFixture(GetParam()) {}
    };

    struct VariableTypeAssignabilityLookup {
        syrec::Variable::Type              variableType;
        std::vector<syrec::Variable::Type> assignableVariableTypes;
        std::vector<syrec::Variable::Type> notAssignableVariableTypes;

        VariableTypeAssignabilityLookup(syrec::Variable::Type variableType, const std::vector<syrec::Variable::Type>& assignableVariableTypes, const std::vector<syrec::Variable::Type>& notAssignableVariableTypes):
            variableType(variableType), assignableVariableTypes(assignableVariableTypes), notAssignableVariableTypes(notAssignableVariableTypes) {}
    };

    class VariableTypeAssignabilityDuringOverloadResolution: public ::testing::TestWithParam<VariableTypeAssignabilityLookup> {
    protected:
        BaseSymbolTable                 symbolTable;
        VariableTypeAssignabilityLookup variableTypeAssignabilityLookup;

        VariableTypeAssignabilityDuringOverloadResolution():
            variableTypeAssignabilityLookup(GetParam()) {}
    };

    syrec::Statement::vec createStatementBodyContainingSingleSkipStmt() {
        return {std::make_shared<syrec::SkipStatement>()};
    }

    void assertScopePointersMatch(const std::optional<TemporaryVariableScope::ptr>& expected, const std::optional<TemporaryVariableScope::ptr>& actual) {
        if (!expected.has_value()) {
            ASSERT_FALSE(actual.has_value());
            return;
        }
        ASSERT_TRUE(*expected);
        ASSERT_TRUE(actual.has_value());
        ASSERT_EQ(*expected, *actual);
    }

    void openNewScopeAndAssertSuccessfulCreation(BaseSymbolTable& symbolTable, TemporaryVariableScope::ptr& activeSymbolTableScope) {
        ASSERT_NO_FATAL_FAILURE(activeSymbolTableScope = symbolTable.openTemporaryScope());
        ASSERT_THAT(activeSymbolTableScope, testing::NotNull());

        std::optional<TemporaryVariableScope::ptr> currentlyActiveSymbolTableScope;
        ASSERT_NO_FATAL_FAILURE(currentlyActiveSymbolTableScope = symbolTable.getActiveTemporaryScope());
        ASSERT_NO_FATAL_FAILURE(assertScopePointersMatch(activeSymbolTableScope, currentlyActiveSymbolTableScope));
    }

    void assertModuleInsertionCompletesSuccessfully(BaseSymbolTable& symbolTable, const syrec::Module::ptr& moduleToInsert) {
        bool wasModuleInsertionSuccessful = false;
        ASSERT_NO_FATAL_FAILURE(wasModuleInsertionSuccessful = symbolTable.insertModule(moduleToInsert));
        ASSERT_TRUE(wasModuleInsertionSuccessful);
    }

    void assertModuleInsertionDoesNotCompleteSuccessfully(BaseSymbolTable& symbolTable, const syrec::Module::ptr& moduleToInsert) {
        bool wasModuleInsertionSuccessful = false;
        ASSERT_NO_FATAL_FAILURE(wasModuleInsertionSuccessful = symbolTable.insertModule(moduleToInsert));
        ASSERT_FALSE(wasModuleInsertionSuccessful);
    }

    void assertVariableCollectionsMatchInOrder(const syrec::Variable::vec& expected, const syrec::Variable::vec& actual) {
        ASSERT_EQ(expected.size(), actual.size());
        ASSERT_TRUE(std::equal(
                expected.cbegin(),
                expected.cend(),
                actual.cbegin(),
                actual.cend(),
                [](const syrec::Variable::ptr& expectedVariable, const syrec::Variable::ptr& actualVariable) {
                    return expectedVariable->name == actualVariable->name && expectedVariable->bitwidth == actualVariable->bitwidth && std::equal(expectedVariable->dimensions.cbegin(), expectedVariable->dimensions.cend(), actualVariable->dimensions.cbegin(), actualVariable->dimensions.cend(), [](const unsigned int expectedValuesForDimension, const unsigned int actualValuesForDimension) {
                               return expectedValuesForDimension == actualValuesForDimension;
                           });
                }));
    }

    void assertModuleSignaturesMatch(const syrec::Module& expected, const syrec::Module& actual) {
        ASSERT_EQ(expected.name, actual.name);
        ASSERT_EQ(expected.parameters.size(), actual.parameters.size());
        ASSERT_NO_FATAL_FAILURE(assertVariableCollectionsMatchInOrder(expected.parameters, actual.parameters));
        ASSERT_NO_FATAL_FAILURE(assertVariableCollectionsMatchInOrder(expected.variables, actual.variables));
    }

    void assertModuleMatchingSignatureMatchesExpectedOne(const BaseSymbolTable::ModuleOverloadResolutionResult& expectedModuleMatchingSignature, const BaseSymbolTable::ModuleOverloadResolutionResult& actualModuleMatchingSignature) {
        ASSERT_EQ(expectedModuleMatchingSignature.resolutionResult, actualModuleMatchingSignature.resolutionResult);
        if (expectedModuleMatchingSignature.moduleMatchingSignature.has_value()) {
            ASSERT_EQ(expectedModuleMatchingSignature.moduleMatchingSignature.value(), actualModuleMatchingSignature.moduleMatchingSignature.value());
        } else {
            ASSERT_FALSE(actualModuleMatchingSignature.moduleMatchingSignature.has_value());
        }
    }

    void assertModuleCollectionsMatch(const syrec::Module::vec& expected, const syrec::Module::vec& actual) {
        ASSERT_EQ(expected.size(), actual.size());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const syrec::Module::ptr& expectedModule = expected.at(i);
            const syrec::Module::ptr& actualModule   = actual.at(i);
            ASSERT_TRUE(expectedModule);
            ASSERT_TRUE(actualModule);
            ASSERT_NO_FATAL_FAILURE(assertModuleSignaturesMatch(*expectedModule, *actualModule));
        }
    }

    [[nodiscard]] BaseSymbolTable::ModuleOverloadResolutionResult createModuleOverloadResolutionResultForSingleMatch(const syrec::Module::ptr& expectedSingleMatch) {
        return BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::SingleMatchFound, expectedSingleMatch);
    }

    [[nodiscard]] BaseSymbolTable::ModuleOverloadResolutionResult createModuleOverloadResolutionResultForNoMatch() {
        return BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    }

    [[nodiscard]] BaseSymbolTable::ModuleOverloadResolutionResult createModuleOverloadResolutionResultForInvalidCallerArguments() {
        return BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::CallerArgumentsInvalid, std::nullopt);
    }
} // namespace

TEST_P(VariableTypeAmbiguityDuringModuleInsertionTestFixture, VariableTypeAmbiguityPreventsModuleInsertion) {
    const auto firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "paramOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto firstModule          = std::make_shared<syrec::Module>("moduleOne");
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const auto callerArgument           = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArg", defaultSignalDimensions, defaultSignalBitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    // Variable type ambiguity using same variable identifier detected
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionDoesNotCompleteSuccessfully(symbolTable, firstModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    // Variable type ambiguity cannot be resolved by simply changing the variable identifier
    const auto secondModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramTwo", defaultSignalDimensions, defaultSignalBitwidth);
    const auto secondModule          = std::make_shared<syrec::Module>(firstModule->name);
    secondModule->parameters.emplace_back(secondModuleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionDoesNotCompleteSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));
}

TEST_P(VariableTypeAmbiguityDuringModuleInsertionTestFixture, VariableTypeAmbiguityIsNotTriggeredIfModuleIdentifiersDiffer) {
    const auto firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "paramOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto firstModule          = std::make_shared<syrec::Module>("moduleOne");
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const auto callerArgument           = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArg", defaultSignalDimensions, defaultSignalBitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto secondModule = std::make_shared<syrec::Module>("moduleTwo");
    secondModule->parameters.emplace_back(firstModuleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(secondModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(secondModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));
}

TEST_P(NoVariableTypeAmbiguityDuringModuleInsertionTestFixture, NoVariableTypeAmbiguityDuringModuleInsertion) {
    const auto firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "paramOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto firstModule          = std::make_shared<syrec::Module>("moduleOne");
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const auto callerArgument           = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArg", defaultSignalDimensions, defaultSignalBitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto secondModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramTwo", defaultSignalDimensions, defaultSignalBitwidth);
    const auto secondModule          = std::make_shared<syrec::Module>("moduleTwo");
    secondModule->parameters.emplace_back(secondModuleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(secondModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(secondModule->name, {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));
}

TEST_P(NoVariableTypeAmbiguityDuringModuleInsertionTestFixture, VariableTypeAmbiguityForSingleParameterDoesNotPreventModuleInsertion) {
    const auto firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "mOneParamOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto firstModule          = std::make_shared<syrec::Module>("moduleOne");
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    const auto firstCallerArgument      = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", defaultSignalDimensions, defaultSignalBitwidth);
    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto secondModuleParameterOne = std::make_shared<syrec::Variable>(firstModuleParameterType, "mTwoParamOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto secondModuleParameterTwo = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mTwoParamTwo", std::vector<unsigned>({3, 2}), defaultSignalBitwidth - 1);
    const auto secondModule             = std::make_shared<syrec::Module>(firstModule->name);
    secondModule->parameters.emplace_back(secondModuleParameterOne);
    secondModule->parameters.emplace_back(secondModuleParameterTwo);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    const auto secondCallerArgument = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", secondModuleParameterTwo->dimensions, secondModuleParameterTwo->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstCallerArgument, secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));
}

TEST_P(NoVariableTypeAmbiguityDuringModuleInsertionTestFixture, VariableTypeAmbiguityResolvedByDifferentSignalBitwidth) {
    const std::string moduleIdentifier     = "moduleOne";
    const auto        firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "paramOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto        firstModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    auto       modulesMatchingSignature          = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const auto callerArgumentWithDefaultBitwidth = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", firstModuleParameter->dimensions, firstModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto secondModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramTwo", defaultSignalDimensions, defaultSignalBitwidth - 1);
    const auto secondModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModule->parameters.emplace_back(secondModuleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto callerArgumentWithSmallerBitwidth = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", secondModuleParameter->dimensions, secondModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithSmallerBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));

    const auto thirdModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramThree", defaultSignalDimensions, defaultSignalBitwidth + 1);
    const auto thirdModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    thirdModule->parameters.emplace_back(thirdModuleParameter);
    thirdModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, thirdModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule, thirdModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithSmallerBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));

    const auto callerArgumentWithLargerBitwidth = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgThree", thirdModuleParameter->dimensions, thirdModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithLargerBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(thirdModule), modulesMatchingSignature));
}

TEST_P(NoVariableTypeAmbiguityDuringModuleInsertionTestFixture, VariableTypeAmbiguityResolvedByDifferentNumberOfValuesForDimensionOfSignal) {
    const std::string moduleIdentifier     = "moduleOne";
    const auto        firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "paramOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto        firstModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    auto       modulesMatchingSignature          = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const auto callerArgumentWithDefaultBitwidth = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", firstModuleParameter->dimensions, firstModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    std::vector<unsigned int> signalDimensionsWithLessValuesForDimension = defaultSignalDimensions;
    --signalDimensionsWithLessValuesForDimension[0];

    const auto secondModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramTwo", signalDimensionsWithLessValuesForDimension, defaultSignalBitwidth);
    const auto secondModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModule->parameters.emplace_back(secondModuleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto callerArgumentWithLessValuesForDimensions = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", secondModuleParameter->dimensions, secondModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithLessValuesForDimensions}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));

    std::vector<unsigned int> signalDimensionsWithMoreValuesForDimension = defaultSignalDimensions;
    --signalDimensionsWithMoreValuesForDimension[0];

    const auto thirdModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramThree", signalDimensionsWithMoreValuesForDimension, defaultSignalBitwidth + 1);
    const auto thirdModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    thirdModule->parameters.emplace_back(thirdModuleParameter);
    thirdModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, thirdModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule, thirdModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithLessValuesForDimensions}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));

    const auto callerArgumentWithMoreValuesForDimensions = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgThree", thirdModuleParameter->dimensions, thirdModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithMoreValuesForDimensions}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(thirdModule), modulesMatchingSignature));
}

TEST_P(NoVariableTypeAmbiguityDuringModuleInsertionTestFixture, VariableTypeAmbiguityResolvedByDifferentNumberOfDimensionOfSignal) {
    const std::string moduleIdentifier     = "moduleOne";
    const auto        firstModuleParameter = std::make_shared<syrec::Variable>(firstModuleParameterType, "paramOne", defaultSignalDimensions, defaultSignalBitwidth);
    const auto        firstModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModule->parameters.emplace_back(firstModuleParameter);
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    auto       modulesMatchingSignature          = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const auto callerArgumentWithDefaultBitwidth = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", firstModuleParameter->dimensions, firstModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    std::vector<unsigned int> signalDimensionsWithLessDimensions = defaultSignalDimensions;
    signalDimensionsWithLessDimensions.pop_back();

    const auto secondModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramTwo", signalDimensionsWithLessDimensions, defaultSignalBitwidth);
    const auto secondModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModule->parameters.emplace_back(secondModuleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto callerArgumentWithLessDimensions = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", secondModuleParameter->dimensions, secondModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithLessDimensions}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));

    std::vector<unsigned int> signalDimensionsWithMoreDimensions = defaultSignalDimensions;
    signalDimensionsWithMoreDimensions.emplace_back(1);

    const auto thirdModuleParameter = std::make_shared<syrec::Variable>(secondModuleParameterType, "paramThree", signalDimensionsWithMoreDimensions, defaultSignalBitwidth + 1);
    const auto thirdModule          = std::make_shared<syrec::Module>(moduleIdentifier);
    thirdModule->parameters.emplace_back(thirdModuleParameter);
    thirdModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, thirdModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule, thirdModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithDefaultBitwidth}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithLessDimensions}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));

    const auto callerArgumentWithMoreDimensions = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgThree", thirdModuleParameter->dimensions, thirdModuleParameter->bitwidth);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithMoreDimensions}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(thirdModule), modulesMatchingSignature));
}

INSTANTIATE_TEST_SUITE_P(
        VariableTypeAmbiguity,
        VariableTypeAmbiguityDuringModuleInsertionTestFixture,
        ::testing::Values(
                std::make_pair(syrec::Variable::Type::In, syrec::Variable::Type::In),
                std::make_pair(syrec::Variable::Type::Out, syrec::Variable::Type::Inout),
                std::make_pair(syrec::Variable::Type::Out, syrec::Variable::Type::Out),
                std::make_pair(syrec::Variable::Type::Inout, syrec::Variable::Type::Inout),
                std::make_pair(syrec::Variable::Type::Inout, syrec::Variable::Type::Out)));

INSTANTIATE_TEST_SUITE_P(
        NoVariableTypeAmbiguity,
        NoVariableTypeAmbiguityDuringModuleInsertionTestFixture,
        ::testing::Values(
                std::make_pair(syrec::Variable::Type::In, syrec::Variable::Type::Out),
                std::make_pair(syrec::Variable::Type::In, syrec::Variable::Type::Inout),
                std::make_pair(syrec::Variable::Type::Out, syrec::Variable::Type::In),
                std::make_pair(syrec::Variable::Type::Inout, syrec::Variable::Type::In)));

TEST_P(VariableTypeAssignabilityDuringOverloadResolution, VariableTypeAssignabilityDuringOverloadResolutionWhenSearchingForModulesUsingSignature) {
    const auto moduleParameter        = std::make_shared<syrec::Variable>(variableTypeAssignabilityLookup.variableType, "moduleParamOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto moduleWithOneParameter = std::make_shared<syrec::Module>("moduleOne");
    moduleWithOneParameter->parameters.emplace_back(moduleParameter);
    moduleWithOneParameter->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, moduleWithOneParameter));

    if (variableTypeAssignabilityLookup.notAssignableVariableTypes.empty()) {
        syrec::Module::vec modulesMatchingName;
        ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleWithOneParameter->name));
        ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({moduleWithOneParameter}, modulesMatchingName));

        auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::SingleMatchFound, std::nullopt);
        ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleWithOneParameter->name, {moduleParameter}));
        ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(moduleWithOneParameter), modulesMatchingSignature));

        auto firstCallerArg = std::make_shared<syrec::Variable>(moduleParameter->type, "callerArg", moduleParameter->dimensions, moduleParameter->bitwidth);
        for (const syrec::Variable::Type assignableVariableTypeForModuleParameter: variableTypeAssignabilityLookup.assignableVariableTypes) {
            firstCallerArg->type = assignableVariableTypeForModuleParameter;
            ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleWithOneParameter->name, {firstCallerArg}));
            ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(moduleWithOneParameter), modulesMatchingSignature));
        }
    } else {
        const auto moduleParameterNotAssignableForFirstModuleParameter = std::make_shared<syrec::Variable>(variableTypeAssignabilityLookup.notAssignableVariableTypes.front(), "moduleParamTwo", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
        const auto moduleWithACompatibleAndIncompatibleParameter       = std::make_shared<syrec::Module>(moduleWithOneParameter->name);
        moduleWithACompatibleAndIncompatibleParameter->parameters.emplace_back(moduleParameter);
        moduleWithACompatibleAndIncompatibleParameter->parameters.emplace_back(moduleParameterNotAssignableForFirstModuleParameter);
        moduleWithACompatibleAndIncompatibleParameter->statements = createStatementBodyContainingSingleSkipStmt();
        ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, moduleWithACompatibleAndIncompatibleParameter));

        syrec::Module::vec modulesMatchingName;
        ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleWithOneParameter->name));
        ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({moduleWithOneParameter, moduleWithACompatibleAndIncompatibleParameter}, modulesMatchingName));

        auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
        ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleWithOneParameter->name, {moduleParameter}));
        ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(moduleWithOneParameter), modulesMatchingSignature));

        auto firstCallerArg = std::make_shared<syrec::Variable>(moduleParameter->type, "callerArg", moduleParameter->dimensions, moduleParameter->bitwidth);
        for (const syrec::Variable::Type assignableVariableTypeForModuleParameter: variableTypeAssignabilityLookup.assignableVariableTypes) {
            firstCallerArg->type = assignableVariableTypeForModuleParameter;
            ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleWithOneParameter->name, {firstCallerArg}));
            ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(moduleWithOneParameter), modulesMatchingSignature));
        }

        for (const syrec::Variable::Type notAssignableVariableTypeForModuleParameter: variableTypeAssignabilityLookup.notAssignableVariableTypes) {
            firstCallerArg->type = notAssignableVariableTypeForModuleParameter;
            ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleWithOneParameter->name, {firstCallerArg}));
            ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
        VariableTypeAssignabilityCheckWhenLookingForModulesMatchingSignature,
        VariableTypeAssignabilityDuringOverloadResolution,
        ::testing::Values(
                VariableTypeAssignabilityLookup(syrec::Variable::Type::In, {syrec::Variable::Type::In, syrec::Variable::Type::Out, syrec::Variable::Type::Inout, syrec::Variable::Type::Wire, syrec::Variable::Type::State}, {}),
                VariableTypeAssignabilityLookup(syrec::Variable::Type::Out, {syrec::Variable::Type::Out, syrec::Variable::Type::Inout, syrec::Variable::Type::Wire}, {syrec::Variable::Type::In, syrec::Variable::Type::State}),
                VariableTypeAssignabilityLookup(syrec::Variable::Type::Inout, {syrec::Variable::Type::Out, syrec::Variable::Type::Inout, syrec::Variable::Type::Wire}, {syrec::Variable::Type::In, syrec::Variable::Type::State})));

TEST(BaseSymbolTableTests, OpenScope) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));
}

TEST(BaseSymbolTableTests, CloseEmptyScope) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    std::optional<TemporaryVariableScope::ptr> closedActiveSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(closedActiveSymbolTableScope = symbolTable.closeTemporaryScope());
    ASSERT_NO_FATAL_FAILURE(assertScopePointersMatch(std::nullopt, closedActiveSymbolTableScope));

    std::optional<TemporaryVariableScope::ptr> currentlyOpenSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(currentlyOpenSymbolTableScope = symbolTable.getActiveTemporaryScope());
    ASSERT_NO_FATAL_FAILURE(assertScopePointersMatch(std::nullopt, currentlyOpenSymbolTableScope));

    ASSERT_NO_FATAL_FAILURE(closedActiveSymbolTableScope = symbolTable.closeTemporaryScope());
    ASSERT_NO_FATAL_FAILURE(assertScopePointersMatch(std::nullopt, closedActiveSymbolTableScope));

    ASSERT_NO_FATAL_FAILURE(currentlyOpenSymbolTableScope = symbolTable.getActiveTemporaryScope());
    ASSERT_NO_FATAL_FAILURE(assertScopePointersMatch(std::nullopt, currentlyOpenSymbolTableScope));
}

TEST(BaseSymbolTableTests, InsertModuleSharingSignatureWithExistingOneHavingDifferentIdentifierCreatesNewSymTableEntry) {
    BaseSymbolTable symbolTable;

    const auto moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);

    const std::string firstModuleIdentifier = "moduleOne";
    const auto        firstModuleDefinition = std::make_shared<syrec::Module>(firstModuleIdentifier);
    firstModuleDefinition->parameters       = {moduleParameter};
    firstModuleDefinition->statements       = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModuleDefinition}, modulesMatchingName));

    const auto callerParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerParamOne", moduleParameter->dimensions, moduleParameter->bitwidth);

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleDefinition), modulesMatchingSignature));

    const std::string secondModuleIdentifier = "moduleTwo";
    const auto        secondModuleDefinition = std::make_shared<syrec::Module>(secondModuleIdentifier);
    secondModuleDefinition->parameters       = firstModuleDefinition->parameters;
    secondModuleDefinition->statements       = firstModuleDefinition->statements;
    ASSERT_TRUE(symbolTable.insertModule(secondModuleDefinition));

    // Check existing valid module definition was left unchanged
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModuleDefinition}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleDefinition), modulesMatchingSignature));

    // Check second module definition has matching symbol table entry
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(secondModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({secondModuleDefinition}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(secondModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModuleDefinition), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, InsertOverloadedModuleHavingAdditionalParameterThanExistingOne) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    const std::string moduleIdentifier        = "moduleOne";
    const auto        firstModuleParameterOne = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", std::vector<unsigned>(1, 2), DEFAULT_BITWIDTH);
    auto              firstModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModuleToInsert->statements           = createStatementBodyContainingSingleSkipStmt();
    firstModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleToInsert));

    const auto& secondModuleParameterOne = std::make_shared<syrec::Variable>(*firstModuleParameterOne);
    secondModuleParameterOne->name       = "mTwoParamOne";

    const auto secondModuleParameterTwo = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "mTwoParamTwo", std::vector<unsigned>(2, 2), DEFAULT_BITWIDTH);
    auto       secondModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModuleToInsert->statements    = createStatementBodyContainingSingleSkipStmt();
    secondModuleToInsert->parameters    = {secondModuleParameterOne, secondModuleParameterTwo};
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleToInsert));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch(modulesMatchingName, {firstModuleToInsert, secondModuleToInsert}));

    auto firstCallerArgument  = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", secondModuleParameterOne->dimensions, secondModuleParameterOne->bitwidth);
    auto secondCallerArgument = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", secondModuleParameterTwo->dimensions, secondModuleParameterTwo->bitwidth);

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleToInsert), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstCallerArgument, secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModuleToInsert), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, ExistsModuleForNameWithNoModulesExisting) {
    const BaseSymbolTable symbolTable;
    ASSERT_FALSE(symbolTable.existsModuleForName("moduleName"));
}

TEST(BaseSymbolTableTests, ExistsModuleForName) {
    BaseSymbolTable symbolTable;

    const auto firstModule  = std::make_shared<syrec::Module>("firstModule");
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    const auto moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondModule    = std::make_shared<syrec::Module>(firstModule->name);
    secondModule->parameters.emplace_back(moduleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_TRUE(symbolTable.existsModuleForName(firstModule->name));
}

TEST(BaseSymbolTableTests, ExistsModuleForNameUsingEmptyIdentifier) {
    BaseSymbolTable symbolTable;

    const auto firstModule  = std::make_shared<syrec::Module>("firstModule");
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    const auto moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondModule    = std::make_shared<syrec::Module>(firstModule->name);
    secondModule->parameters.emplace_back(moduleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_FALSE(symbolTable.existsModuleForName(""));
}

TEST(BaseSymbolTableTests, ExistsModuleForNameUsingNonExistingModuleIdentifier) {
    BaseSymbolTable symbolTable;

    const auto firstModule  = std::make_shared<syrec::Module>("firstModule");
    firstModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    const auto moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondModule    = std::make_shared<syrec::Module>(firstModule->name);
    secondModule->parameters.emplace_back(moduleParameter);
    secondModule->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_FALSE(symbolTable.existsModuleForName("otherModuleIdent"));
}

// Error cases tests
TEST(BaseSymbolTableTests, GetModulesByNameWithNoExistingModules) {
    const BaseSymbolTable symbolTable;

    const auto         callerArgument = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "callerArg", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName("moduleName"));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({}, modulesMatchingName));
}

TEST(BaseSymbolTableTests, GetModulesByNameUsingIdentifierHavingNoMatchesWithModuleIdentifiers) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    const auto        defaultVariableDimensions = std::vector<unsigned>({2});
    const std::string firstModuleIdentifier     = "moduleOne";
    const auto        firstModuleParameterOne   = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", std::vector<unsigned>({2}), DEFAULT_BITWIDTH);
    auto              firstModuleToInsert       = std::make_shared<syrec::Module>(firstModuleIdentifier);
    firstModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    firstModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleToInsert));

    const std::string secondModuleIdentifier   = "moduleTwo";
    const auto        secondModuleParameterOne = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "mOneParamTwo", defaultVariableDimensions, DEFAULT_BITWIDTH);
    auto              secondModuleToInsert     = std::make_shared<syrec::Module>(secondModuleIdentifier);
    secondModuleToInsert->parameters.emplace_back(secondModuleParameterOne);
    secondModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleToInsert));

    auto thirdModuleToInsert = std::make_shared<syrec::Module>(firstModuleIdentifier);
    thirdModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    thirdModuleToInsert->parameters.emplace_back(secondModuleParameterOne);
    thirdModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, thirdModuleToInsert));

    syrec::Module::vec modulesMatchingIdentifier;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingIdentifier = symbolTable.getModulesByName("moduleThree"));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({}, modulesMatchingIdentifier));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingIdentifier = symbolTable.getModulesByName(firstModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModuleToInsert, thirdModuleToInsert}, modulesMatchingIdentifier));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingIdentifier = symbolTable.getModulesByName(secondModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({secondModuleToInsert}, modulesMatchingIdentifier));
}

TEST(BaseSymbolTableTests, GetModulesMatchingSignatureWithCallerArgumentBitwidthDifferentThanModuleParameterBitwidth) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    const std::string moduleIdentifier        = "moduleOne";
    const auto        firstModuleParameterOne = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", std::vector<unsigned>({2}), DEFAULT_BITWIDTH);
    auto              firstModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    firstModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleToInsert));

    const auto secondModuleParameterTwo = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mTwoParamOne", firstModuleParameterOne->dimensions, DEFAULT_BITWIDTH + 2);
    auto       secondModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    secondModuleToInsert->parameters.emplace_back(secondModuleParameterTwo);
    secondModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleToInsert));

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    // Overload resolution for known module identifier and caller arguments not matching any module
    const auto callerArgumentWithABitwidthSmallerThanAnyModuleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "callerArgOne", firstModuleParameterOne->dimensions, firstModuleParameterOne->bitwidth - 2);
    const auto callerArgumentWithABitwidthLargerThanAnyModuleParameter  = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "callerArgTwo", secondModuleParameterTwo->dimensions, secondModuleParameterTwo->bitwidth + 2);

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithABitwidthSmallerThanAnyModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithABitwidthLargerThanAnyModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    const std::string unknownModuleIdentifier = "unknownModuleIdentifier";
    // Overload resolution for unknown module identifier
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {firstModuleParameterOne}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {firstModuleParameterOne, secondModuleParameterTwo}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    // Successfully overload resolution for any of the two inserted modules
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstModuleParameterOne}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleToInsert), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstModuleParameterOne, secondModuleParameterTwo}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne({createModuleOverloadResolutionResultForSingleMatch(secondModuleToInsert)}, modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, GetModulesMatchingSignatureWithCallerArgumentNumberOfDimensionsDifferentThanModuleParameterNumberOfDimensions) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    const std::string moduleIdentifier        = "moduleOne";
    const auto        firstModuleParameterOne = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", std::vector<unsigned>({2, 1}), DEFAULT_BITWIDTH);
    auto              firstModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    firstModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleToInsert));

    const auto secondModuleParameterTwo = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mTwoParamOne", std::vector<unsigned>({1, 2, 3}), DEFAULT_BITWIDTH);
    auto       secondModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    secondModuleToInsert->parameters.emplace_back(secondModuleParameterTwo);
    secondModuleToInsert->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleToInsert));

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    // Overload resolution for known module identifier and caller arguments not matching any module
    const auto callerArgumentWithLessDimensionsThanAnyModuleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "callerArgOne", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto callerArgumentWithMoreDimensionsThanAnyModuleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "callerArgTwo", std::vector<unsigned>({1, 2, 3, 4}), DEFAULT_BITWIDTH);

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithLessDimensionsThanAnyModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithMoreDimensionsThanAnyModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    const std::string unknownModuleIdentifier = "unknownModuleIdentifier";
    // Overload resolution for unknown module identifier
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {firstModuleParameterOne}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {firstModuleParameterOne, secondModuleParameterTwo}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    // Successfully overload resolution for any of the two inserted modules
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstModuleParameterOne}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleToInsert), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstModuleParameterOne, secondModuleParameterTwo}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModuleToInsert), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, GetModulesMatchingSignatureWithCallerArgumentNumberOfValuesOfDimensionDifferentThanModuleParameterValuesOfDimension) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    const std::string moduleIdentifier        = "moduleOne";
    const auto        firstModuleParameterOne = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", std::vector<unsigned>(1, 2), DEFAULT_BITWIDTH);
    auto              firstModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModuleToInsert->statements           = createStatementBodyContainingSingleSkipStmt();
    firstModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleToInsert));

    auto              modulesMatchingSignature                                              = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    const std::string callerArgumentIdentifier                                              = "callerArgumentOne";
    const auto        callerArgumentWithNumberOfValuesOfDimensionSmallerThanModuleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, callerArgumentIdentifier, std::vector(1, firstModuleParameterOne->dimensions.front() - 1), DEFAULT_BITWIDTH);

    const auto callerArgumentWithNumberOfValuesOfDimensionLargerThanModuleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, callerArgumentIdentifier, std::vector(1, firstModuleParameterOne->dimensions.front() + 1), DEFAULT_BITWIDTH);
    auto       secondModuleParameterOne                                             = std::make_shared<syrec::Variable>(*firstModuleParameterOne);
    secondModuleParameterOne->name                                                  = "mTwoParamOne";

    const auto secondModuleParameterTwo = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "mTwoParamTwo", callerArgumentWithNumberOfValuesOfDimensionLargerThanModuleParameter->dimensions, DEFAULT_BITWIDTH);
    auto       secondModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModuleToInsert->statements    = createStatementBodyContainingSingleSkipStmt();
    secondModuleToInsert->parameters    = {secondModuleParameterOne, secondModuleParameterTwo};
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleToInsert));

    const auto  callerArgumentWithNumberOfValuesOfDimensionLargerThanAnyModuleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, callerArgumentIdentifier, std::vector(1, firstModuleParameterOne->dimensions.front() + 2), DEFAULT_BITWIDTH);
    const auto  callerArgumentOne                                                       = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", secondModuleParameterOne->dimensions, secondModuleParameterOne->bitwidth);
    const auto& callerArgumentTwo                                                       = std::make_shared<syrec::Variable>(secondModuleParameterTwo->type, "callerArgTwo", secondModuleParameterTwo->dimensions, secondModuleParameterTwo->bitwidth);

    // Overload resolution for known module identifier and caller arguments not matching any module
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithNumberOfValuesOfDimensionSmallerThanModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentWithNumberOfValuesOfDimensionLargerThanAnyModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    // Overload resolution for unknown module identifier
    const std::string unknownModuleIdentifier = "moduleTwo";
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {callerArgumentWithNumberOfValuesOfDimensionSmallerThanModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {callerArgumentWithNumberOfValuesOfDimensionLargerThanAnyModuleParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {callerArgumentOne}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    // Successfully overload resolution for any of the two inserted modules
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentOne, callerArgumentTwo}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModuleToInsert), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {callerArgumentOne}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleToInsert), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, GetModulesMatchingSignatureWithNumberOfCallerArgumentsBeingDifferentThanTheNumberOfParametersOfAnyDeclaredModule) {
    BaseSymbolTable             symbolTable;
    TemporaryVariableScope::ptr activeSymbolTableScope;
    ASSERT_NO_FATAL_FAILURE(openNewScopeAndAssertSuccessfulCreation(symbolTable, activeSymbolTableScope));

    const std::string moduleIdentifier        = "moduleOne";
    const auto        firstModuleParameterOne = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", std::vector<unsigned>(1, 2), DEFAULT_BITWIDTH);
    auto              firstModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    firstModuleToInsert->statements           = createStatementBodyContainingSingleSkipStmt();
    firstModuleToInsert->parameters.emplace_back(firstModuleParameterOne);
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleToInsert));

    auto secondModuleParameterOne  = std::make_shared<syrec::Variable>(*firstModuleParameterOne);
    secondModuleParameterOne->name = "mTwoParamOne";

    const auto secondModuleParameterTwo = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "mTwoParamTwo", std::vector<unsigned>(2, 2), DEFAULT_BITWIDTH);
    auto       secondModuleToInsert     = std::make_shared<syrec::Module>(moduleIdentifier);
    secondModuleToInsert->statements    = createStatementBodyContainingSingleSkipStmt();
    secondModuleToInsert->parameters    = {secondModuleParameterOne, secondModuleParameterTwo};
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleToInsert));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(moduleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch(modulesMatchingName, {firstModuleToInsert, secondModuleToInsert}));

    auto firstCallerArgument  = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", secondModuleParameterOne->dimensions, secondModuleParameterOne->bitwidth);
    auto secondCallerArgument = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", secondModuleParameterTwo->dimensions, secondModuleParameterTwo->bitwidth);
    auto thirdCallerArgument  = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgThree", std::vector<unsigned>(1, 1), DEFAULT_BITWIDTH);

    // Overload resolution for known module identifier and caller arguments not matching any module
    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstCallerArgument, secondCallerArgument, thirdCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    // Overload resolution for unknown module identifier
    const std::string unknownModuleIdentifier = "moduleTwo";
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {firstCallerArgument, secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(unknownModuleIdentifier, {firstCallerArgument, secondCallerArgument, thirdCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));

    // Successfully overload resolution for any of the two inserted modules
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModuleToInsert), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(moduleIdentifier, {firstCallerArgument, secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModuleToInsert), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, InsertModuleWithEmptyIdentifierCreatesNoSymTableEntry) {
    BaseSymbolTable symbolTable;
    const auto      moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);

    const std::string validModuleIdentifier = "testModule";
    const auto        validModuleDefinition = std::make_shared<syrec::Module>(validModuleIdentifier);
    validModuleDefinition->parameters       = {moduleParameter};
    validModuleDefinition->statements       = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, validModuleDefinition));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(validModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({validModuleDefinition}, modulesMatchingName));

    const auto callerParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerParamOne", moduleParameter->dimensions, moduleParameter->bitwidth);

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(validModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(validModuleDefinition), modulesMatchingSignature));

    const auto invalidModuleDefinition  = std::make_shared<syrec::Module>("");
    invalidModuleDefinition->parameters = {moduleParameter};
    invalidModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_FALSE(symbolTable.insertModule(invalidModuleDefinition));

    // Check existing valid module definition was left unchanged
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(validModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({validModuleDefinition}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(validModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(validModuleDefinition), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, InsertModuleWhoseSignatureMatchesExistingOneDoesNotCreateDuplicateSymTableEntry) {
    BaseSymbolTable symbolTable;

    const auto moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);

    const std::string validModuleIdentifier = "testModule";
    const auto        validModuleDefinition = std::make_shared<syrec::Module>(validModuleIdentifier);
    validModuleDefinition->parameters       = {moduleParameter};
    validModuleDefinition->statements       = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, validModuleDefinition));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(validModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({validModuleDefinition}, modulesMatchingName));

    const auto callerParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerParamOne", moduleParameter->dimensions, moduleParameter->bitwidth);

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(validModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(validModuleDefinition), modulesMatchingSignature));

    const auto invalidModuleDefinition  = std::make_shared<syrec::Module>(validModuleIdentifier);
    invalidModuleDefinition->parameters = validModuleDefinition->parameters;
    invalidModuleDefinition->statements = validModuleDefinition->statements;
    ASSERT_FALSE(symbolTable.insertModule(invalidModuleDefinition));

    // Check existing valid module definition was left unchanged
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(validModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({validModuleDefinition}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(validModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(validModuleDefinition), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, InsertModuleUsingNullptrDoesNotSucceedButDoesNotCrash) {
    BaseSymbolTable symbolTable;
    const auto      moduleParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "paramOne", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);

    const std::string validModuleIdentifier = "testModule";
    const auto        validModuleDefinition = std::make_shared<syrec::Module>(validModuleIdentifier);
    validModuleDefinition->parameters       = {moduleParameter};
    validModuleDefinition->statements       = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, validModuleDefinition));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(validModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({validModuleDefinition}, modulesMatchingName));

    const auto callerParameter = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerParamOne", moduleParameter->dimensions, moduleParameter->bitwidth);

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(validModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(validModuleDefinition), modulesMatchingSignature));

    ASSERT_FALSE(symbolTable.insertModule(nullptr));

    // Check existing valid module definition was left unchanged
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(validModuleIdentifier));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({validModuleDefinition}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(validModuleIdentifier, {callerParameter}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(validModuleDefinition), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, FetchModulesUsingEmptyIdentifier) {
    BaseSymbolTable symbolTable;

    const auto moduleInParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "inParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto firstModuleDefinition = std::make_shared<syrec::Module>("moduleOne");
    firstModuleDefinition->parameters.emplace_back(moduleInParameter);
    firstModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    const auto moduleInoutParameter   = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "inoutParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto secondModuleDefinition = std::make_shared<syrec::Module>("moduleTwo");
    secondModuleDefinition->parameters.emplace_back(moduleInParameter);
    secondModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleDefinition));

    syrec::Module::vec modulesMatchingIdentifier;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingIdentifier = symbolTable.getModulesByName(""));
    ASSERT_TRUE(modulesMatchingIdentifier.empty());
}

TEST(BaseSymbolTableTests, FetchModulesUsingIdentifierNotMatchingAnyExistingDeclaration) {
    BaseSymbolTable symbolTable;

    const auto moduleInParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "inParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto firstModuleDefinition = std::make_shared<syrec::Module>("moduleOne");
    firstModuleDefinition->parameters.emplace_back(moduleInParameter);
    firstModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    const auto moduleInoutParameter   = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "inoutParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto secondModuleDefinition = std::make_shared<syrec::Module>("moduleTwo");
    secondModuleDefinition->parameters.emplace_back(moduleInParameter);
    secondModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleDefinition));

    syrec::Module::vec modulesMatchingIdentifier;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingIdentifier = symbolTable.getModulesByName("anotherModuleIdentifier"));
    ASSERT_TRUE(modulesMatchingIdentifier.empty());
}

TEST(BaseSymbolTableTests, FetchModulesUsingCallerSignatureUsingEmptyModuleIdentifier) {
    BaseSymbolTable symbolTable;

    const auto moduleInParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "inParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto firstModuleDefinition = std::make_shared<syrec::Module>("moduleOne");
    firstModuleDefinition->parameters.emplace_back(moduleInParameter);
    firstModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    const auto moduleInoutParameter   = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "inoutParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto secondModuleDefinition = std::make_shared<syrec::Module>("moduleTwo");
    secondModuleDefinition->parameters.emplace_back(moduleInParameter);
    secondModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleDefinition));

    const auto callerArgument           = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArg", moduleInParameter->dimensions, moduleInParameter->bitwidth);
    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature("", {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, FetchModulesUsingCallerSignatureUsingModuleIdentifierNotMatchingAnyExistingDeclaration) {
    BaseSymbolTable symbolTable;

    const auto moduleInParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "inParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto firstModuleDefinition = std::make_shared<syrec::Module>("moduleOne");
    firstModuleDefinition->parameters.emplace_back(moduleInParameter);
    firstModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    const auto moduleInoutParameter   = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "inoutParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto secondModuleDefinition = std::make_shared<syrec::Module>("moduleTwo");
    secondModuleDefinition->parameters.emplace_back(moduleInParameter);
    secondModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleDefinition));

    const auto callerArgument           = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArg", moduleInParameter->dimensions, moduleInParameter->bitwidth);
    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature("anotherModuleIdentifier", {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, FetchModulesUsingCallerSignatureUsingInvalidCallerArgument) {
    BaseSymbolTable symbolTable;

    const auto moduleInParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "inParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto firstModuleDefinition = std::make_shared<syrec::Module>("moduleOne");
    firstModuleDefinition->parameters.emplace_back(moduleInParameter);
    firstModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    const auto moduleInoutParameter   = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "inoutParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto secondModuleDefinition = std::make_shared<syrec::Module>("moduleTwo");
    secondModuleDefinition->parameters.emplace_back(moduleInParameter);
    secondModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleDefinition));

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature("anotherModuleIdentifier", {nullptr}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForInvalidCallerArguments(), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModuleDefinition->name, {nullptr}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForInvalidCallerArguments(), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, FetchModulesUsingCallerSignatureWithoutParametersDoesNotPerformLookupByName) {
    BaseSymbolTable symbolTable;

    const auto moduleInParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "inParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto firstModuleDefinition = std::make_shared<syrec::Module>("moduleOne");
    firstModuleDefinition->parameters.emplace_back(moduleInParameter);
    firstModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModuleDefinition));

    const auto moduleInoutParameter   = std::make_shared<syrec::Variable>(syrec::Variable::Type::Inout, "inoutParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto moduleOutParameter     = std::make_shared<syrec::Variable>(syrec::Variable::Type::Out, "outParam", std::vector<unsigned>({1}), DEFAULT_BITWIDTH);
    const auto secondModuleDefinition = std::make_shared<syrec::Module>("moduleTwo");
    secondModuleDefinition->parameters.emplace_back(moduleInParameter);
    secondModuleDefinition->parameters.emplace_back(moduleOutParameter);
    secondModuleDefinition->statements = createStatementBodyContainingSingleSkipStmt();
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModuleDefinition));

    auto modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModuleDefinition->name, {}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, FetchModulesUsingCallerSignatureMatchingMultipleModules) {
    BaseSymbolTable symbolTable;

    const auto firstParameterOfFirstModule  = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "mOneParamOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondParameterOfFirstModule = std::make_shared<syrec::Variable>(syrec::Variable::Type::Out, "mOneParamTwo", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto firstModule                  = std::make_shared<syrec::Module>("moduleOne");
    firstModule->parameters.emplace_back(firstParameterOfFirstModule);
    firstModule->parameters.emplace_back(secondParameterOfFirstModule);
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, firstModule));

    syrec::Module::vec modulesMatchingName;
    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule}, modulesMatchingName));

    const auto firstCallerArgument      = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondCallerArgument     = std::make_shared<syrec::Variable>(syrec::Variable::Type::Wire, "callerArgTwo", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstCallerArgument, secondCallerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    const auto firstParameterOfSecondModule  = std::make_shared<syrec::Variable>(secondParameterOfFirstModule->type, "mTwoParamOne", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondParameterOfSecondModule = std::make_shared<syrec::Variable>(firstParameterOfFirstModule->type, "mTwoParamTwo", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    const auto secondModule                  = std::make_shared<syrec::Module>(firstModule->name);
    secondModule->parameters.emplace_back(firstParameterOfSecondModule);
    secondModule->parameters.emplace_back(secondParameterOfSecondModule);
    ASSERT_NO_FATAL_FAILURE(assertModuleInsertionCompletesSuccessfully(symbolTable, secondModule));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingName = symbolTable.getModulesByName(firstModule->name));
    ASSERT_NO_FATAL_FAILURE(assertModuleCollectionsMatch({firstModule, secondModule}, modulesMatchingName));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstCallerArgument, secondCallerArgument}));

    const auto overloadResultMatchingMultipleModules = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::MultipleMatchesFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(overloadResultMatchingMultipleModules, modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstParameterOfFirstModule, secondParameterOfFirstModule}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(firstModule), modulesMatchingSignature));

    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature(firstModule->name, {firstParameterOfSecondModule, secondParameterOfSecondModule}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForSingleMatch(secondModule), modulesMatchingSignature));
}

TEST(BaseSymbolTableTests, FetchModulesUsingCallerSignatureWithNoExistingModules) {
    const BaseSymbolTable symbolTable;

    const auto callerArgument           = std::make_shared<syrec::Variable>(syrec::Variable::Type::In, "callerArg", DEFAULT_SIGNAL_DIMENSIONS, DEFAULT_BITWIDTH);
    auto       modulesMatchingSignature = BaseSymbolTable::ModuleOverloadResolutionResult(BaseSymbolTable::ModuleOverloadResolutionResult::NoMatchFound, std::nullopt);
    ASSERT_NO_FATAL_FAILURE(modulesMatchingSignature = symbolTable.getModulesMatchingSignature("moduleName", {callerArgument}));
    ASSERT_NO_FATAL_FAILURE(assertModuleMatchingSignatureMatchesExpectedOne(createModuleOverloadResolutionResultForNoMatch(), modulesMatchingSignature));
}
