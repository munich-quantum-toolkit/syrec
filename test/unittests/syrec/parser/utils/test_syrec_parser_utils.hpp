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

#include "test_syrec_parser_base.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The .clang-tidy warning about the missing header file seems to be a false positive since the include of the required <nlohmann/json.hpp> is defined in this file.
// Maybe this warning is reported because the nlohmann library is implicitly added by one of the external dependencies?
using json = nlohmann::json; // NOLINT(misc-include-cleaner) Warning reported here seems to be a false positive since <nlohmann/json.hpp> is included

namespace syrec_parser_test_utils {
    constexpr auto TEST_NAME_NOT_ALLOWED_CHARACTER       = '-';
    constexpr auto TEST_NAME_COMPONENT_DELIMITER_SYMBOL  = '_';
    constexpr auto GTEST_IGNORE_PREFIX                   = "DISABLED";
    constexpr auto ERROR_REASON_JSON_DATA_NOT_OBJECT     = "DISABLED_TOPLEVEL_JSON_OBJECT_NOT_FOUND";
    constexpr auto ERROR_REASON_JSON_DATA_INVALID        = "DISABLED_JSON_DATA_INVALID";
    constexpr auto ERROR_REASON_FAILED_TO_LOAD_FROM_FILE = "DISABLED_FAILED_TO_LOAD_TEST_DATA_FROM_FILE";

    struct FilenameAndTestNamePrefix {
        std::string relativePathToTestDataFolder;
        std::string nameOfFileContainingTestData;
        std::string testnamePrefixForTestsLoadedFromFile;

        FilenameAndTestNamePrefix(std::string relativePathToTestDataFolder, std::string nameOfFileContainingTestData, std::string testnamePrefixForTestsLoadedFromFile):
            relativePathToTestDataFolder(std::move(relativePathToTestDataFolder)), nameOfFileContainingTestData(std::move(nameOfFileContainingTestData)), testnamePrefixForTestsLoadedFromFile(std::move(testnamePrefixForTestsLoadedFromFile)) {}

        size_t operator()(const FilenameAndTestNamePrefix& entityToHash) const {
            // For now we use an often referred to implementation: https://stackoverflow.com/a/2595226 that is sufficient for our use case
            constexpr std::hash<std::string> hasher{};
            std::size_t                      seed = 0;
            seed ^= hasher(entityToHash.relativePathToTestDataFolder) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hasher(entityToHash.nameOfFileContainingTestData) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }

        bool operator==(const FilenameAndTestNamePrefix& other) const {
            return relativePathToTestDataFolder == other.relativePathToTestDataFolder && nameOfFileContainingTestData == other.nameOfFileContainingTestData;
        }
    };

    /*
     * Only non-empty ASCII alphanumeric characters without underscores are supported as tests names
     * see https://google.github.io/googletest/advanced.html#specifying-names-for-value-parameterized-test-parameters
     */
    inline bool doesStringContainOnlyAsciiCharactersOrUnderscores(const std::string_view& stringToCheck) {
        return std::all_of(
                stringToCheck.cbegin(),
                stringToCheck.cend(),
                [](const char characterToCheck) {
                    return std::isalnum(characterToCheck) != 0 || characterToCheck == TEST_NAME_COMPONENT_DELIMITER_SYMBOL;
                });
    }

    inline std::string concatenateStrings(const std::string& lString, const char delimiter, const std::initializer_list<std::string>& otherComponents) {
        std::string resultContainer = lString;
        for (const auto& toBeConcatenatedComponents: otherComponents) {
            resultContainer += delimiter;
            resultContainer += toBeConcatenatedComponents;
        }
        return resultContainer;
    }

    inline std::string replaceInvalidCharactersInTestCaseName(const std::string& testCaseName) {
        if (testCaseName.find_first_of(TEST_NAME_NOT_ALLOWED_CHARACTER) == std::string::npos) {
            return testCaseName;
        }

        std::string transformedTestCaseName         = testCaseName;
        const auto  replacementForInvalidCharacters = std::to_string(TEST_NAME_COMPONENT_DELIMITER_SYMBOL);
        for (std::size_t indexOfNextInvalidCharacter = testCaseName.find_first_of(TEST_NAME_NOT_ALLOWED_CHARACTER, 0); indexOfNextInvalidCharacter != std::string::npos; indexOfNextInvalidCharacter += testCaseName.size()) {
            transformedTestCaseName.replace(indexOfNextInvalidCharacter, 1, replacementForInvalidCharacters);
            indexOfNextInvalidCharacter = testCaseName.find_first_of(TEST_NAME_NOT_ALLOWED_CHARACTER, indexOfNextInvalidCharacter);
        }
        return transformedTestCaseName;
    }

    inline std::string extractFilenameWithoutFileExtension(const std::string& filename) {
        if (const std::size_t fileExtensionPosition = filename.find_last_of('.'); fileExtensionPosition != std::string::npos) {
            return filename.substr(0, fileExtensionPosition);
        }
        return filename;
    }

    inline std::vector<std::string> loadTestCaseNamesFromFile(const std::string& relativePathToFile, const std::string& inputFileName, const std::string& testCaseNamePrefix) {
        std::ifstream inputFileStream(relativePathToFile + "/" + inputFileName, std::ios_base::in);
        if (!inputFileStream.good()) {
            return {concatenateStrings(ERROR_REASON_FAILED_TO_LOAD_FROM_FILE, TEST_NAME_COMPONENT_DELIMITER_SYMBOL, {extractFilenameWithoutFileExtension(inputFileName)})};
        }

        const auto parsedJson = json::parse(inputFileStream, nullptr, false);
        if (parsedJson.is_discarded()) {
            return {concatenateStrings(ERROR_REASON_JSON_DATA_INVALID, TEST_NAME_COMPONENT_DELIMITER_SYMBOL, {extractFilenameWithoutFileExtension(inputFileName)})};
        }
        if (!parsedJson.is_object()) {
            return {concatenateStrings(ERROR_REASON_JSON_DATA_NOT_OBJECT, TEST_NAME_COMPONENT_DELIMITER_SYMBOL, {extractFilenameWithoutFileExtension(inputFileName)})};
        }

        std::vector<std::string> foundTestCases;
        std::size_t              testCaseIndexInFile = 0;
        // According to the nlohman json documentation (https://json.nlohmann.me/features/iterators/#iteration-order-for-objects), iterating over values using the provided iterator will order the values according to std::less<> by default.
        // Thus no random access iterator is used => the std::distance call should be positive in all cases (thus we can take the absolute value of returned iter_diff_t)
        foundTestCases.reserve(static_cast<std::size_t>(std::distance(parsedJson.items().begin(), parsedJson.items().end())));

        for (const auto& topLevelJsonObjectKeysIterable: parsedJson.items()) {
            const std::string& testCaseName = topLevelJsonObjectKeysIterable.key();
            if (testCaseName.empty()) {
                continue;
            }

            if (doesStringContainOnlyAsciiCharactersOrUnderscores(testCaseName)) {
                foundTestCases.emplace_back(testCaseName);
            } else {
                foundTestCases.emplace_back(concatenateStrings(
                        ERROR_REASON_JSON_DATA_INVALID, TEST_NAME_COMPONENT_DELIMITER_SYMBOL, {extractFilenameWithoutFileExtension(inputFileName), std::string(GTEST_IGNORE_PREFIX), "Test", std::to_string(testCaseIndexInFile), "ContainedNonConformingCharactersInName", testCaseNamePrefix}));
            }

            ++testCaseIndexInFile;
        }
        return foundTestCases;
    }

    inline std::vector<TestFromJsonConfig> loadTestCaseNamesFromFiles(const std::vector<FilenameAndTestNamePrefix>& inputFileNamesData) {
        std::vector<TestFromJsonConfig> loadedTestCases;
        for (const auto& inputFileNameData: inputFileNamesData) {
            if (inputFileNameData.nameOfFileContainingTestData.empty()) {
                continue;
            }

            const std::vector<std::string> loadedTestCaseNamesFromFile = !inputFileNameData.nameOfFileContainingTestData.empty() ? loadTestCaseNamesFromFile(inputFileNameData.relativePathToTestDataFolder, inputFileNameData.nameOfFileContainingTestData, inputFileNameData.testnamePrefixForTestsLoadedFromFile) : std::vector<std::string>();

            if (loadedTestCaseNamesFromFile.empty()) {
                continue;
            }

            if (loadedTestCaseNamesFromFile.size() == 1 && loadedTestCaseNamesFromFile.front().find_first_of(GTEST_IGNORE_PREFIX) == 0) {
                loadedTestCases.emplace_back(loadedTestCaseNamesFromFile.front(), "", replaceInvalidCharactersInTestCaseName(loadedTestCaseNamesFromFile.front()));
                continue;
            }

            std::transform(
                    loadedTestCaseNamesFromFile.cbegin(),
                    loadedTestCaseNamesFromFile.cend(),
                    std::back_inserter(loadedTestCases),
                    [&inputFileNameData](const std::string& keyOfTestCaseInJsonFile) {
                        return TestFromJsonConfig(
                                inputFileNameData.relativePathToTestDataFolder + "/" + inputFileNameData.nameOfFileContainingTestData,
                                keyOfTestCaseInJsonFile,
                                replaceInvalidCharactersInTestCaseName(!inputFileNameData.testnamePrefixForTestsLoadedFromFile.empty() ? concatenateStrings(inputFileNameData.testnamePrefixForTestsLoadedFromFile, TEST_NAME_COMPONENT_DELIMITER_SYMBOL, {keyOfTestCaseInJsonFile}) : keyOfTestCaseInJsonFile));
                    });
        }
        return loadedTestCases;
    }
} //namespace syrec_parser_test_utils
