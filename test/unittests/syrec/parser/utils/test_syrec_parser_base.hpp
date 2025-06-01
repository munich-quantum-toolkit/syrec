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

#include "core/syrec/parser/utils/base_syrec_ir_entity_stringifier.hpp"
#include "core/syrec/parser/utils/syrec_operation_utils.hpp"
#include "core/syrec/program.hpp"

#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

// The .clang-tidy warning about the missing header file seems to be a false positive since the include of the required <nlohmann/json.hpp> is defined in this file.
// Maybe this warning is reported because the nlohmann library is implicitly added by one of the external dependencies?
using json = nlohmann::json; // NOLINT(misc-include-cleaner) Warning reported here seems to be a false positive since <nlohmann/json.hpp> is included

struct TestFromJsonConfig {
    std::string nameOfJsonFile;
    std::string keyOfTestInJsonFile;
    std::string testCaseName;

    explicit TestFromJsonConfig(std::string nameOfJsonFile, std::string keyOfTestInJsonFile, std::string testCaseName):
        nameOfJsonFile(std::move(nameOfJsonFile)), keyOfTestInJsonFile(std::move(keyOfTestInJsonFile)), testCaseName(std::move(testCaseName)) {}
};

class SyrecParserBaseTestsFixture: public testing::TestWithParam<TestFromJsonConfig> {
protected:
    struct TestFromJson {
        std::string stringifiedSyrecProgramToProcess;
        std::string stringifiedExpectedSyrecProgramContent;
    };

    std::string jsonKeyInTestCaseDataForCircuit                                                                           = "inputCircuit";
    std::string jsonKeyInTestCaseDataForExpectedCircuit                                                                   = "expectedCircuit";
    std::string jsonKeyInTestCaseDataForParserConfiguration                                                               = "parserConfig";
    std::string jsonKeyInTestCaseDataForDefaultSignalBitwidthInParserConfig                                               = "defaultBitwidth";
    std::string jsonKeyInTestCaseDataForConstantValueTruncationOperation                                                  = "constValTruncationOp";
    std::string jsonKeyInTestCaseDataForAllowingOverlappingAccessOnAssignedToVariablePartsInAnyVariableAccessOfAssignment = "allowOverlappingAccessOnAssignedToSignalParts";

    TestFromJson                              loadedTestCaseData;
    syrec::Program                            parserInstance;
    std::optional<syrec::ReadProgramSettings> userDefinedParserConfiguration;

    void SetUp() override {
        const TestFromJsonConfig& testParameterData     = GetParam();
        const std::string&        testCaseJsonKeyInFile = testParameterData.keyOfTestInJsonFile;
        std::ifstream             inputFileStream(testParameterData.nameOfJsonFile, std::ios_base::in);
        ASSERT_TRUE(inputFileStream.good()) << "Input file @" << testParameterData.nameOfJsonFile << " is not in a usable state (e.g. does not exist)";

        const json parsedJsonDataOfFile = json::parse(inputFileStream);
        ASSERT_TRUE(parsedJsonDataOfFile.contains(testCaseJsonKeyInFile)) << "No matching entry with key '" << testCaseJsonKeyInFile << "' was found in the JSON test case data";

        const json& testCaseDataJson = parsedJsonDataOfFile[testCaseJsonKeyInFile];
        ASSERT_TRUE(testCaseDataJson.is_object()) << "Test case data with key '" << testCaseJsonKeyInFile << "' must be defined as a JSON object";
        ASSERT_TRUE(testCaseDataJson.contains(jsonKeyInTestCaseDataForCircuit)) << "Test case data did not contain expected key '" << jsonKeyInTestCaseDataForCircuit << "' for circuit to process";
        ASSERT_TRUE(testCaseDataJson.at(jsonKeyInTestCaseDataForCircuit).is_string()) << "Circuit to process must be defined as a string";
        loadedTestCaseData.stringifiedSyrecProgramToProcess = testCaseDataJson.at(jsonKeyInTestCaseDataForCircuit).get<std::string>();

        if (testCaseDataJson.contains(jsonKeyInTestCaseDataForExpectedCircuit)) {
            ASSERT_TRUE(testCaseDataJson.at(jsonKeyInTestCaseDataForExpectedCircuit).is_string()) << "Expected circuit processing result must be defined as a string";
            loadedTestCaseData.stringifiedExpectedSyrecProgramContent = testCaseDataJson.at(jsonKeyInTestCaseDataForExpectedCircuit).get<std::string>();
        } else {
            loadedTestCaseData.stringifiedExpectedSyrecProgramContent = loadedTestCaseData.stringifiedSyrecProgramToProcess;
        }

        if (testCaseDataJson.contains(jsonKeyInTestCaseDataForParserConfiguration)) {
            ASSERT_NO_FATAL_FAILURE(loadUserDefinedParserConfigurationFromJson(testCaseDataJson.at(jsonKeyInTestCaseDataForParserConfiguration)));
        }
    }

    static void assertKeyInJsonObjectExists(const json& jsonObject, const std::string& key) {
        ASSERT_TRUE(jsonObject.is_object());
        ASSERT_TRUE(jsonObject.contains(key)) << "Required key '" << key << "' was not found in the JSON object";
    }

    static void assertStringificationOfParsedSyrecProgramIsSuccessful(const syrec::Program& syrecProgramToStringifiy, std::ostream& containerForStringifiedProgram) {
        utils::BaseSyrecIrEntityStringifier::AdditionalFormattingOptions customFormattingOptions;
        customFormattingOptions.optionalCustomIndentationCharacterSequence = "";
        customFormattingOptions.optionalCustomNewlineCharacterSequence     = " ";

        utils::BaseSyrecIrEntityStringifier syrecProgramStringifier(customFormattingOptions);
        bool                                wasStringificationSuccessful = false;
        ASSERT_NO_FATAL_FAILURE(wasStringificationSuccessful = syrecProgramStringifier.stringify(containerForStringifiedProgram, syrecProgramToStringifiy)) << "Error during stringification of SyReC program";
        ASSERT_TRUE(wasStringificationSuccessful) << "Failed to stringify SyReC program";
    }

    void loadUserDefinedParserConfigurationFromJson(const json& jsonObject) {
        userDefinedParserConfiguration = syrec::ReadProgramSettings();
        ASSERT_TRUE(jsonObject.is_object()) << "User defined parser configuration needs to be defined as a json object";
        if (jsonObject.contains(jsonKeyInTestCaseDataForDefaultSignalBitwidthInParserConfig)) {
            ASSERT_TRUE(jsonObject.at(jsonKeyInTestCaseDataForDefaultSignalBitwidthInParserConfig).is_number_unsigned()) << "User defined default variable bitwidth needs to be defined as an unsigned integer";
            userDefinedParserConfiguration->defaultBitwidth = jsonObject.at(jsonKeyInTestCaseDataForDefaultSignalBitwidthInParserConfig).get<unsigned int>();
        }
        if (jsonObject.contains(jsonKeyInTestCaseDataForConstantValueTruncationOperation)) {
            ASSERT_TRUE(jsonObject.at(jsonKeyInTestCaseDataForConstantValueTruncationOperation).is_string());
            const auto& stringifiedTruncationOperation = jsonObject.at(jsonKeyInTestCaseDataForConstantValueTruncationOperation).get<std::string>();
            if (stringifiedTruncationOperation == "modulo") {
                userDefinedParserConfiguration->integerConstantTruncationOperation = utils::IntegerConstantTruncationOperation::Modulo;
            } else if (stringifiedTruncationOperation == "bitwiseAnd") {
                userDefinedParserConfiguration->integerConstantTruncationOperation = utils::IntegerConstantTruncationOperation::BitwiseAnd;
            } else {
                FAIL() << "No mapping for user defined integer constant value truncation operation '" << stringifiedTruncationOperation << "' defined";
            }
        }

        if (jsonObject.contains(jsonKeyInTestCaseDataForAllowingOverlappingAccessOnAssignedToVariablePartsInAnyVariableAccessOfAssignment)) {
            ASSERT_TRUE(jsonObject.at(jsonKeyInTestCaseDataForAllowingOverlappingAccessOnAssignedToVariablePartsInAnyVariableAccessOfAssignment).is_boolean());
            userDefinedParserConfiguration->allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess = jsonObject.at(jsonKeyInTestCaseDataForAllowingOverlappingAccessOnAssignedToVariablePartsInAnyVariableAccessOfAssignment).get<bool>();
        }
    }

    virtual void performTestExecution() {
        const syrec::ReadProgramSettings& parserConfiguration = userDefinedParserConfiguration.value_or(syrec::ReadProgramSettings());
        std::string                       aggregateOfDetectedErrorsDuringProcessingOfUserProvidedSyrecProgram;
        ASSERT_NO_FATAL_FAILURE(aggregateOfDetectedErrorsDuringProcessingOfUserProvidedSyrecProgram = parserInstance.readFromString(loadedTestCaseData.stringifiedSyrecProgramToProcess, parserConfiguration));
        ASSERT_TRUE(aggregateOfDetectedErrorsDuringProcessingOfUserProvidedSyrecProgram.empty()) << "Expected no errors to be reported when parsing the given SyReC program but actual found errors where: " << aggregateOfDetectedErrorsDuringProcessingOfUserProvidedSyrecProgram;

        std::string aggregateOfDetectedErrorsDuringProcessingOfExpectedOutputOfStringificationOfUserProvidedSyrecProgram;
        ASSERT_NO_FATAL_FAILURE(aggregateOfDetectedErrorsDuringProcessingOfExpectedOutputOfStringificationOfUserProvidedSyrecProgram = parserInstance.readFromString(loadedTestCaseData.stringifiedExpectedSyrecProgramContent, parserConfiguration));
        ASSERT_TRUE(aggregateOfDetectedErrorsDuringProcessingOfExpectedOutputOfStringificationOfUserProvidedSyrecProgram.empty()) << "Expected no errors to be reported when parsing the stringified version of the given SyReC program expected to be generated when stringifying the IR of the parser but actual found errors where: " << aggregateOfDetectedErrorsDuringProcessingOfExpectedOutputOfStringificationOfUserProvidedSyrecProgram;

        std::ostringstream containerForStringifiedProgram;
        ASSERT_NO_FATAL_FAILURE(assertStringificationOfParsedSyrecProgramIsSuccessful(parserInstance, containerForStringifiedProgram));
        ASSERT_EQ(containerForStringifiedProgram.str(), loadedTestCaseData.stringifiedExpectedSyrecProgramContent) << "SyReC program processed by the parser needs to match the user defined input circuit";
    }
};
