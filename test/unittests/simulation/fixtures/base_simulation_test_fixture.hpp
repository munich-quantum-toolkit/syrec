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
#include "algorithms/simulation/simple_simulation.hpp"
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/configurable_options.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/statistics.hpp"
#include "core/syrec/program.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

// The .clang-tidy warning about the missing header file seems to be a false positive since the include of the required <nlohmann/json.hpp> is defined in this file.
// Maybe this warning is reported because the nlohmann library is implicitly added by one of the external dependencies?
using json = nlohmann::json; // NOLINT(misc-include-cleaner) Warning reported here seems to be a false positive since <nlohmann/json.hpp> is included

/**
 * A templated test fixture usable to validate the correct synthesis of an input circuit using a set of simulation runs.
 * @tparam T The type of the synthesizer under test
 * @details If this fixture is used to define a templated test (i.e. TYPED_TEST_P) with name "TestName" then the test case data needs to be defined in a .json file in the following format: \n
 * {
 *   "TestName": {
 *      "inputCircuit": <STRING>,
 *      "simulationRuns": [
 *          { "in": <STRING>, "out": <STRING> }
 *      ]
 *   }
 * }
 *
 * Note that the input and output state needs to be defined as a string containing only binary values. Additionally, only the non-ancillary qubit values
 * need to be defined in the input and output state.
 */
template<typename T>
class BaseSimulationTestFixture: public ::testing::Test {
public:
    void SetUp() override {
        static_assert(std::is_same_v<T, syrec::CostAwareSynthesis> || std::is_same_v<T, syrec::LineAwareSynthesis>);
    }

    syrec::AnnotatableQuantumComputation annotatableQuantumComputation;
    syrec::Program                       syrecProgramInstance;

    [[nodiscard]] static constexpr bool isTestingLineAwareSynthesis() noexcept {
        return std::is_same_v<T, syrec::LineAwareSynthesis>;
    }

    void performTestExecutionExpectingSynthesisFailureForCircuitLoadedFromString(const std::string_view& circuitToParseAndSynthesis, const std::optional<syrec::ConfigurableOptions>& optionalSynthesisSettings = std::nullopt) {
        ASSERT_NO_FATAL_FAILURE(parseInputCircuitFromString(circuitToParseAndSynthesis, syrecProgramInstance));
        ASSERT_FALSE(performProgramSynthesis(syrecProgramInstance, annotatableQuantumComputation, optionalSynthesisSettings)) << "Expected synthesis of input circuit to fail";
    }

    void performTestExecutionForCircuitLoadedFromJson(const std::string& pathToTestCaseDataJsonFile, const std::string& testcaseJsonKey, const std::optional<syrec::ConfigurableOptions>& optionalSynthesisSettings = std::nullopt, syrec::Statistics* optionalRecordedStatistics = nullptr) {
        json jsonDataOfTestCase;
        ASSERT_NO_FATAL_FAILURE(loadAndParseTestCaseDataFromJson(pathToTestCaseDataJsonFile, testcaseJsonKey, jsonDataOfTestCase));
        ASSERT_NO_FATAL_FAILURE(validateJsonStructure(jsonDataOfTestCase));

        // Since this is a member function of a templated class, the templated function of the nlohmann namespace needs to use this specific syntax to
        // correctly provide the template parameter for the latter (for further information read either the matching issue in the nlohman project
        // https://github.com/nlohmann/json/issues/3827 or https://en.cppreference.com/w/cpp/language/dependent_name.html#template_disambiguator).
        const std::string& stringifiedInputCircuit = jsonDataOfTestCase[jsonKeyForInputCircuit].template get<std::string>();
        ASSERT_NO_FATAL_FAILURE(parseInputCircuitFromString(stringifiedInputCircuit, syrecProgramInstance));
        ASSERT_TRUE(performProgramSynthesis(syrecProgramInstance, annotatableQuantumComputation, optionalSynthesisSettings, optionalRecordedStatistics)) << "Synthesis of input circuit was not successful";

        const json& jsonDataOfSimulationRuns = jsonDataOfTestCase[jsonKeyForSimulationRuns];
        for (const auto& jsonDataOfSimulationRun: jsonDataOfSimulationRuns) {
            const std::size_t numQubitsToCheck = jsonDataOfSimulationRun[jsonKeyForStringifiedBinaryInputState].template get<std::string>().size();
            ASSERT_LE(numQubitsToCheck, annotatableQuantumComputation.getNqubits()) << "Expected state values cannot contain more qubits than the quantum computation itself";

            syrec::NBitValuesContainer inputState(annotatableQuantumComputation.getNqubits());
            ASSERT_NO_FATAL_FAILURE(loadNBitValuesContainerFromString(inputState, jsonDataOfSimulationRun[jsonKeyForStringifiedBinaryInputState]));

            syrec::NBitValuesContainer outputState(inputState.size());
            ASSERT_NO_FATAL_FAILURE(loadNBitValuesContainerFromString(outputState, jsonDataOfSimulationRun[jsonKeyForStringifiedBinaryOutputState]));
            ASSERT_NO_FATAL_FAILURE(assertSimulationResultForStateMatchesExpectedOne(annotatableQuantumComputation, inputState, outputState, numQubitsToCheck));
        }
    }

    [[nodiscard]] static std::string getNameOfCurrentlyExecutedTest() {
        return testing::UnitTest::GetInstance()->current_test_info()->name();
    }

protected:
    static void loadAndParseTestCaseDataFromJson(const std::string& pathToTestCaseDataJsonFile, const std::string& testcaseJsonKey, json& containerForJsonDataOfTestCase) {
        std::ifstream inputFileStream(pathToTestCaseDataJsonFile, std::ifstream::in | std::ifstream::binary);
        ASSERT_TRUE(inputFileStream.good()) << "Input file @" << pathToTestCaseDataJsonFile << " is not in a usable state (e.g. does not exist)";

        try {
            const json parsedJsonDataOfFile = json::parse(inputFileStream);
            ASSERT_TRUE(parsedJsonDataOfFile.contains(testcaseJsonKey)) << "Matching entry for test case was not found in json loaded from " << pathToTestCaseDataJsonFile << " when using '" << testcaseJsonKey << "' as key";
            containerForJsonDataOfTestCase = parsedJsonDataOfFile[testcaseJsonKey];
        } catch (const std::exception& ex) {
            FAIL() << "Failed to parse JSON '" << pathToTestCaseDataJsonFile << "': " << ex.what();
        }
    }

    void validateJsonStructure(const json& jsonToValidate) const {
        ASSERT_TRUE(jsonToValidate.is_structured()) << "Expected test case data to be a JSON object";

        ASSERT_TRUE(jsonToValidate.contains(jsonKeyForInputCircuit)) << "Entry for input circuit using key '" << jsonKeyForInputCircuit << "' was not found in the json";
        ASSERT_TRUE(jsonToValidate[jsonKeyForInputCircuit].is_string()) << "Input circuit must be defined as a string in the json";

        ASSERT_TRUE(jsonToValidate.contains(jsonKeyForSimulationRuns)) << "Entry for data of simulation runs using key '" << jsonKeyForSimulationRuns << "' was not found in the json";
        ASSERT_TRUE(jsonToValidate[jsonKeyForSimulationRuns].is_array()) << "Data for simulation runs must be defined as an array in the json";

        const json& jsonDataForSimulationRuns = jsonToValidate[jsonKeyForSimulationRuns];
        for (const auto& jsonDataOfSimulationRun: jsonDataForSimulationRuns) {
            ASSERT_TRUE(jsonDataOfSimulationRun.is_structured()) << "Data per simulation run must be defined as an object in the json";

            ASSERT_TRUE(jsonDataOfSimulationRun.contains(jsonKeyForStringifiedBinaryInputState)) << "Entry for input state using key '" << jsonKeyForStringifiedBinaryInputState << "' was not found in the json";
            ASSERT_TRUE(jsonDataOfSimulationRun[jsonKeyForStringifiedBinaryInputState].is_string()) << "Input state must be defined as a string of binary values in the json";

            ASSERT_TRUE(jsonDataOfSimulationRun.contains(jsonKeyForStringifiedBinaryOutputState)) << "Entry for output state using key '" << jsonKeyForStringifiedBinaryInputState << "' was not found in the json";
            ASSERT_TRUE(jsonDataOfSimulationRun[jsonKeyForStringifiedBinaryOutputState].is_string()) << "Output state must be defined as a string of binary values in the json";
        }
    }

    [[nodiscard]] static bool performProgramSynthesis(const syrec::Program& program, syrec::AnnotatableQuantumComputation& annotatableQuantumComputation, const std::optional<syrec::ConfigurableOptions>& optionalSynthesisSettings = std::nullopt, syrec::Statistics* optionalRecordedStatistics = nullptr) {
        const auto synthesisSettings = optionalSynthesisSettings.value_or(syrec::ConfigurableOptions());
        if constexpr (std::is_same_v<T, syrec::CostAwareSynthesis>) {
            return syrec::CostAwareSynthesis::synthesize(annotatableQuantumComputation, program, synthesisSettings, optionalRecordedStatistics);
        } else {
            return syrec::LineAwareSynthesis::synthesize(annotatableQuantumComputation, program, synthesisSettings, optionalRecordedStatistics);
        }
    }

    static void assertSimulationResultForStateMatchesExpectedOne(const syrec::AnnotatableQuantumComputation& annotatableQuantumComputation, const syrec::NBitValuesContainer& inputState, const syrec::NBitValuesContainer& expectedOutputState, const std::size_t userDefinedNumQubitsToCheck) {
        ASSERT_EQ(inputState.size(), expectedOutputState.size());

        syrec::NBitValuesContainer actualOutputState(inputState.size());
        ASSERT_NO_FATAL_FAILURE(syrec::simpleSimulation(actualOutputState, annotatableQuantumComputation, inputState, nullptr));
        ASSERT_EQ(actualOutputState.size(), expectedOutputState.size());

        // We are assuming that the indices of the ancilla qubits are larger than the one of the inputs/output qubits and that the user is not interested in the value of the ancillary qubits.
        // Since we cannot determine which garbage qubits refer to parameters of type 'out' or local variables of type 'wire', the user must define the number of qubits to check in input/output states
        // to either include/exclude the value of the qubits of local variables/ancillary qubits from the checks.
        std::size_t numQubitsToCheck = annotatableQuantumComputation.getNqubitsWithoutAncillae();
        if (userDefinedNumQubitsToCheck != numQubitsToCheck) {
            ASSERT_LE(userDefinedNumQubitsToCheck, inputState.size()) << "User defined number of qubits must be smaller or equal to the size of the input state";
            numQubitsToCheck = userDefinedNumQubitsToCheck;
        }

        for (std::size_t i = 0; i < numQubitsToCheck; ++i) {
            ASSERT_EQ(expectedOutputState[i], actualOutputState[i]) << "Value mismatch during simulation at qubit " << std::to_string(i) << ", expected: " << std::to_string(static_cast<int>(expectedOutputState[i])) << " but was " << std::to_string(static_cast<int>(actualOutputState[i]))
                                                                    << "!\nInput state: " << inputState.stringify() << " | Expected output state: " << expectedOutputState.stringify() << " | Actual output state: " << actualOutputState.stringify();
        }
    }

    static void loadNBitValuesContainerFromString(syrec::NBitValuesContainer& container, const std::string& stringifiedBinaryState) {
        ASSERT_GT(container.size(), 0) << "To be able to verify the contents of the stringified binary state we need to know how many values are to be expected using the NBitValuesContainer";
        ASSERT_GE(container.size(), stringifiedBinaryState.size()) << "Expected size of NBitValues container must be equal to larger than stringified binary state size";

        for (std::size_t i = 0; i < stringifiedBinaryState.size(); ++i) {
            if (stringifiedBinaryState[i] == '1') {
                ASSERT_TRUE(container.flip(i)) << "Failed to flip value for output bit " << std::to_string(i);
            } else {
                ASSERT_EQ(stringifiedBinaryState[i], '0') << "Only the characters '0' and '1' are allowed when defining the state of an output";
            }
        }
    }

    static void parseInputCircuitFromString(const std::string_view& stringifiedSyrecProgram, syrec::Program& parserInstance, const std::optional<syrec::ConfigurableOptions>& optionalParserConfiguration = std::nullopt) {
        std::string errorsOfReadInputCircuit;
        ASSERT_NO_FATAL_FAILURE(errorsOfReadInputCircuit = parserInstance.readFromString(stringifiedSyrecProgram, optionalParserConfiguration.value_or(syrec::ConfigurableOptions())));
        ASSERT_TRUE(errorsOfReadInputCircuit.empty()) << "Expected no errors in input circuits but actually found the following: " << errorsOfReadInputCircuit;
    }

    std::string jsonKeyForInputCircuit                 = "inputCircuit";
    std::string jsonKeyForSimulationRuns               = "simulationRuns";
    std::string jsonKeyForStringifiedBinaryInputState  = "in";
    std::string jsonKeyForStringifiedBinaryOutputState = "out";
};
