/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "base_simulation_test_fixture.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/syrec/program.hpp"

#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// .clang-tidy reports a false positive here since are including the required nlohman json header file
using json = nlohmann::json; // NOLINT(misc-include-cleaner)

using namespace syrec;

namespace {
    template<typename T>
    class FullCircuitSimulationTestsFixture: public BaseSimulationTestFixture<T> {
    public:
        void performTestExecutionForCircuit(const std::string& inputCircuitFilename, const std::string& inputCircuitJsonKeyInConfig) {
            const std::string pathToInputCircuit = getPathToCircuitsDirectory() + inputCircuitFilename;

            std::string errorsOfReadInputCircuit;
            ASSERT_NO_FATAL_FAILURE(errorsOfReadInputCircuit = this->syrecProgramInstance.read(pathToInputCircuit, syrec::ReadProgramSettings()));
            ASSERT_TRUE(errorsOfReadInputCircuit.empty()) << "Expected no errors in input circuits but actually found the following: " << errorsOfReadInputCircuit;

            ASSERT_TRUE(this->performProgramSynthesis(this->syrecProgramInstance)) << "Synthesis of input circuit was not successful";
            NBitValuesContainer inputState(this->annotatableQuantumComputation.getNqubits());
            expectedOutputState.resize(inputState.size());
            ASSERT_NO_FATAL_FAILURE(this->loadTestCaseDataFromJson(inputCircuitJsonKeyInConfig));

            ASSERT_NO_FATAL_FAILURE(this->prepareInputState(inputState));
            this->assertSimulationResultForStateMatchesExpectedOne(inputState, this->expectedOutputState);
        }

    protected:
        [[nodiscard]] static std::string getPathToCircuitsDirectory() {
            // The path to the directory containing the circuit to process is relative to the WORKING_DIRECTORY set in the CMake script that is used to generate the test executable.
            return "./circuits/";
        }

        [[nodiscard]] static std::optional<std::string> getPathToTestCaseConfig() {
            // The path to the config for the given synthesizer type is relative to the WORKING_DIRECTORY set in the CMake script that is used to generate the test executable.
            if (std::is_same_v<T, LineAwareSynthesis>) {
                return "./configs/circuits_line_aware_simulation.json";
            }
            if (std::is_same_v<T, CostAwareSynthesis>) {
                return "./configs/circuits_cost_aware_simulation.json";
            }
            return std::nullopt;
        }

        static void loadNBitValuesContainerFromString(NBitValuesContainer& resultContainer, const std::string& stringifiedContainerContent) {
            for (std::size_t i = 0; i < stringifiedContainerContent.size(); ++i) {
                if (stringifiedContainerContent[i] == '1') {
                    ASSERT_TRUE(resultContainer.flip(i)) << "Failed to flip value for output bit " << std::to_string(i);
                } else {
                    ASSERT_EQ(stringifiedContainerContent[i], '0') << "Only the characters '0' and '1' are allowed when defining the state of an output";
                }
            }
        }

        void loadTestCaseDataFromJson(const std::string& jsonKeyForCircuitInConfig) {
            const std::optional<std::string> pathToTestCaseConfig = getPathToTestCaseConfig();
            ASSERT_TRUE(pathToTestCaseConfig.has_value()) << "Path to test case configuration was not defined for given synthesizer type";

            std::ifstream testCaseConfigInputFilestream(*pathToTestCaseConfig);
            ASSERT_TRUE(testCaseConfigInputFilestream.good()) << "Failed to open test case config file @ " << *pathToTestCaseConfig;
            json fullTestCaseConfigJson = json::parse(testCaseConfigInputFilestream);
            ASSERT_TRUE(fullTestCaseConfigJson.contains(jsonKeyForCircuitInConfig)) << "Did not find entry with key '" << jsonKeyForCircuitInConfig << "' in test case config file @ " << *pathToTestCaseConfig;

            const auto& currTestCaseConfigJson = fullTestCaseConfigJson[jsonKeyForCircuitInConfig];
            ASSERT_TRUE(currTestCaseConfigJson.is_structured()) << "Configuration entry of circuit '" << jsonKeyForCircuitInConfig << "' must be a structured entry!";

            const std::string setInputIndicesJsonKey = "set_lines";
            ASSERT_TRUE(currTestCaseConfigJson.contains(setInputIndicesJsonKey)) << "Configuration did not contain expected entry '" << setInputIndicesJsonKey << "'";
            ASSERT_TRUE(currTestCaseConfigJson[setInputIndicesJsonKey].is_array()) << "Set input lines must be defined in the json as an integer array";
            setInputIndices = currTestCaseConfigJson[setInputIndicesJsonKey].get<std::vector<int>>();

            const std::string expectedOutputStateJsonKey = "sim_out";
            ASSERT_TRUE(currTestCaseConfigJson.contains(expectedOutputStateJsonKey)) << "Configuration did not contain expected entry '" << expectedOutputStateJsonKey << "'";
            ASSERT_TRUE(currTestCaseConfigJson[expectedOutputStateJsonKey].is_string()) << "Expected output state must be defined as a binary string";
            ASSERT_NO_FATAL_FAILURE(loadNBitValuesContainerFromString(expectedOutputState, currTestCaseConfigJson[expectedOutputStateJsonKey].get<std::string>()));
        }

        void prepareInputState(NBitValuesContainer& inputState) const {
            for (const auto& setInputIndex: setInputIndices) {
                ASSERT_GE(setInputIndex, 0) << "Index of input bit must be non-negative integer but was actually " << std::to_string(setInputIndex);
                ASSERT_TRUE(inputState.set(static_cast<std::size_t>(setInputIndex))) << "Could not set value of input " << std::to_string(setInputIndex) << " in the input state";
            }
        }

        std::vector<int>    setInputIndices;
        NBitValuesContainer expectedOutputState;
    };
} // namespace

TYPED_TEST_SUITE_P(FullCircuitSimulationTestsFixture);

TYPED_TEST_P(FullCircuitSimulationTestsFixture, TestOfCircuitAlu2) {
    this->performTestExecutionForCircuit("alu_2.src", "alu_2");
}

TYPED_TEST_P(FullCircuitSimulationTestsFixture, TestOfCircuitSwap2) {
    this->performTestExecutionForCircuit("swap_2.src", "swap_2");
}

TYPED_TEST_P(FullCircuitSimulationTestsFixture, TestOfCircuitSimpleAdd2) {
    this->performTestExecutionForCircuit("simple_add_2.src", "simple_add_2");
}

TYPED_TEST_P(FullCircuitSimulationTestsFixture, TestOfCircuitMultiply2) {
    this->performTestExecutionForCircuit("multiply_2.src", "multiply_2");
}

TYPED_TEST_P(FullCircuitSimulationTestsFixture, TestOfCircuitModulo2) {
    this->performTestExecutionForCircuit("modulo_2.src", "modulo_2");
}

TYPED_TEST_P(FullCircuitSimulationTestsFixture, TestOfCircuitNegate8) {
    this->performTestExecutionForCircuit("negate_8.src", "negate_8");
}

REGISTER_TYPED_TEST_SUITE_P(FullCircuitSimulationTestsFixture,
                            TestOfCircuitAlu2,
                            TestOfCircuitSwap2,
                            TestOfCircuitSimpleAdd2,
                            TestOfCircuitMultiply2,
                            TestOfCircuitModulo2,
                            TestOfCircuitNegate8);

using SynthesizerTypes = testing::Types<CostAwareSynthesis, LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, FullCircuitSimulationTestsFixture, SynthesizerTypes, );
