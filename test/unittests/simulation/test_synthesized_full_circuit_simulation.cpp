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

#include <gtest/gtest.h>
#include <string>

namespace {
    template<typename T>
    [[nodiscard]] std::string getPathToTestCaseDataJsonFile() {
        // The path to the config for the given synthesizer type is relative to the WORKING_DIRECTORY set in the CMake script that is used to generate the test executable.
        if constexpr (std::is_same_v<T, syrec::LineAwareSynthesis>) {
            return "./unittests/simulation/data/test_line_aware_synthesis_of_full_circuits.json";
        }
        return "./unittests/simulation/data/test_cost_aware_synthesis_of_full_circuits.json";
    }
} // namespace

TYPED_TEST_SUITE_P(BaseSimulationTestFixture);

TYPED_TEST_P(BaseSimulationTestFixture, TestOfCircuitAlu2) {
    this->performTestExecutionForCircuitLoadedFromJson(getPathToTestCaseDataJsonFile<TypeParam>(), this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TestOfCircuitSwap2) {
    this->performTestExecutionForCircuitLoadedFromJson(getPathToTestCaseDataJsonFile<TypeParam>(), this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TestOfCircuitSimpleAdd2) {
    this->performTestExecutionForCircuitLoadedFromJson(getPathToTestCaseDataJsonFile<TypeParam>(), this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TestOfCircuitMultiply2) {
    this->performTestExecutionForCircuitLoadedFromJson(getPathToTestCaseDataJsonFile<TypeParam>(), this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TestOfCircuitModulo2) {
    this->performTestExecutionForCircuitLoadedFromJson(getPathToTestCaseDataJsonFile<TypeParam>(), this->getNameOfCurrentlyExecutedTest());
}

TYPED_TEST_P(BaseSimulationTestFixture, TestOfCircuitNegate8) {
    this->performTestExecutionForCircuitLoadedFromJson(getPathToTestCaseDataJsonFile<TypeParam>(), this->getNameOfCurrentlyExecutedTest());
}

REGISTER_TYPED_TEST_SUITE_P(BaseSimulationTestFixture,
                            TestOfCircuitAlu2,
                            TestOfCircuitSwap2,
                            TestOfCircuitSimpleAdd2,
                            TestOfCircuitMultiply2,
                            TestOfCircuitModulo2,
                            TestOfCircuitNegate8);

using SynthesizerTypes = testing::Types<syrec::CostAwareSynthesis, syrec::LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(SyrecSynthesisTest, BaseSimulationTestFixture, SynthesizerTypes, );
