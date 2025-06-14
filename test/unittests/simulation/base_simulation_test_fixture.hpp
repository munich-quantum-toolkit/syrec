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
#include "core/n_bit_values_container.hpp"
#include "core/syrec/program.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

template<typename T>
class BaseSimulationTestFixture: public ::testing::Test {
public:
    void SetUp() override {
        static_assert(std::is_same_v<T, syrec::CostAwareSynthesis> || std::is_same_v<T, syrec::LineAwareSynthesis>);
    }

    void assertSimulationResultForStateMatchesExpectedOne(const syrec::NBitValuesContainer& inputState, const syrec::NBitValuesContainer& expectedOutputState) const {
        ASSERT_EQ(inputState.size(), expectedOutputState.size());

        syrec::NBitValuesContainer actualOutputState(inputState.size());
        ASSERT_NO_FATAL_FAILURE(syrec::simpleSimulation(actualOutputState, annotatableQuantumComputation, inputState));
        ASSERT_EQ(actualOutputState.size(), expectedOutputState.size());

        // We are assuming that the indices of the ancilla qubits are larger than the one of the inputs/output qubits.
        const std::size_t numQubitsToCheck = annotatableQuantumComputation.getNqubitsWithoutAncillae();
        for (std::size_t i = 0; i < numQubitsToCheck; ++i) {
            ASSERT_EQ(expectedOutputState[i], actualOutputState[i]) << "Value mismatch during simulation at qubit " << std::to_string(i) << ", expected: " << std::to_string(static_cast<int>(expectedOutputState[i])) << " but was " << std::to_string(static_cast<int>(actualOutputState[i]))
                                                                    << "!\nInput state: " << inputState.stringify() << " | Expected output state: " << expectedOutputState.stringify() << " | Actual output state: " << actualOutputState.stringify();
        }
    }

    [[nodiscard]] bool performProgramSynthesis(const syrec::Program& program) {
        if (std::is_same_v<T, syrec::CostAwareSynthesis>) {
            return syrec::CostAwareSynthesis::synthesize(annotatableQuantumComputation, program);
        }
        return syrec::LineAwareSynthesis::synthesize(annotatableQuantumComputation, program);
    }

    syrec::AnnotatableQuantumComputation annotatableQuantumComputation;
    syrec::Program                       syrecProgramInstance;
};
