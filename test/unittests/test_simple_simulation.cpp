/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/simple_simulation.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/properties.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>

using namespace syrec;

TEST(SimpleSimulationTests, SimulationOfXOperationWithNoControlQubits) {
    constexpr std::size_t   numQubits         = 3;
    constexpr std::uint64_t initialStateValue = 7; // 111
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto targetQubit    = static_cast<qc::Qubit>(1);
    const auto     xGateOperation = qc::StandardOperation(qc::Controls(), targetQubit, qc::OpType::X);
    ASSERT_TRUE(coreOperationSimulation(xGateOperation, inputState));

    ASSERT_TRUE(inputState[0]);
    ASSERT_FALSE(inputState[1]);
    ASSERT_TRUE(inputState[2]);
}

TEST(SimpleSimulationTests, SimulationOfXOperationWithNoControlQubitsSet) {
    constexpr std::size_t   numQubits         = 3;
    constexpr std::uint64_t initialStateValue = 0;
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto targetQubit    = static_cast<qc::Qubit>(1);
    const auto     xGateOperation = qc::StandardOperation(qc::Controls({0, 2}), targetQubit, qc::OpType::X);
    ASSERT_TRUE(coreOperationSimulation(xGateOperation, inputState));

    ASSERT_FALSE(inputState[0]);
    ASSERT_FALSE(inputState[1]);
    ASSERT_FALSE(inputState[2]);
}

TEST(SimpleSimulationTests, SimulationOfXOperationWithAllControlQubitsSet) {
    constexpr std::size_t   numQubits         = 3;
    constexpr std::uint64_t initialStateValue = 5; // 101
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto targetQubit    = static_cast<qc::Qubit>(1);
    const auto     xGateOperation = qc::StandardOperation(qc::Controls({0, 2}), targetQubit, qc::OpType::X);
    ASSERT_TRUE(coreOperationSimulation(xGateOperation, inputState));

    ASSERT_TRUE(inputState[0]);
    ASSERT_TRUE(inputState[1]);
    ASSERT_TRUE(inputState[2]);
}

TEST(SimpleSimulationTests, SimulationOfXOperationWithOnlySomeControlQubitsSet) {
    constexpr std::size_t   numQubits         = 3;
    constexpr std::uint64_t initialStateValue = 1; // 100
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto targetQubit    = static_cast<qc::Qubit>(1);
    const auto     xGateOperation = qc::StandardOperation(qc::Controls({0, 2}), targetQubit, qc::OpType::X);
    ASSERT_TRUE(coreOperationSimulation(xGateOperation, inputState));

    ASSERT_TRUE(inputState[0]);
    ASSERT_FALSE(inputState[1]);
    ASSERT_FALSE(inputState[2]);
}

TEST(SimpleSimulationTests, SimulationOfSwapOperationWithNoControlQubits) {
    constexpr std::size_t   numQubits         = 4;
    constexpr std::uint64_t initialStateValue = 12; // 0011
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto targetQubitOne    = static_cast<qc::Qubit>(0);
    constexpr auto targetQubitTwo    = static_cast<qc::Qubit>(3);
    const auto     swapGateOperation = qc::StandardOperation(qc::Controls(), qc::Targets({targetQubitOne, targetQubitTwo}), qc::OpType::SWAP);
    ASSERT_TRUE(coreOperationSimulation(swapGateOperation, inputState));

    ASSERT_TRUE(inputState[0]);
    ASSERT_FALSE(inputState[1]);
    ASSERT_TRUE(inputState[2]);
    ASSERT_FALSE(inputState[3]);
}

TEST(SimpleSimulationTests, SimulationOfSwapOperationWithNoControlQubitsSet) {
    constexpr std::size_t   numQubits         = 4;
    constexpr std::uint64_t initialStateValue = 8; // 0001
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto controlQubitOne = static_cast<qc::Qubit>(1);
    constexpr auto controlQubitTwo = static_cast<qc::Qubit>(2);

    constexpr auto targetQubitOne    = static_cast<qc::Qubit>(0);
    constexpr auto targetQubitTwo    = static_cast<qc::Qubit>(3);
    const auto     swapGateOperation = qc::StandardOperation(qc::Controls({controlQubitOne, controlQubitTwo}), qc::Targets({targetQubitOne, targetQubitTwo}), qc::OpType::SWAP);
    ASSERT_TRUE(coreOperationSimulation(swapGateOperation, inputState));

    ASSERT_FALSE(inputState[0]);
    ASSERT_FALSE(inputState[1]);
    ASSERT_FALSE(inputState[2]);
    ASSERT_TRUE(inputState[3]);
}

TEST(SimpleSimulationTests, SimulationOfSwapOperationWithAllControlQubitsSet) {
    constexpr std::size_t   numQubits         = 4;
    constexpr std::uint64_t initialStateValue = 14; // 0111
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto controlQubitOne = static_cast<qc::Qubit>(1);
    constexpr auto controlQubitTwo = static_cast<qc::Qubit>(2);

    constexpr auto targetQubitOne    = static_cast<qc::Qubit>(0);
    constexpr auto targetQubitTwo    = static_cast<qc::Qubit>(3);
    const auto     swapGateOperation = qc::StandardOperation(qc::Controls({controlQubitOne, controlQubitTwo}), qc::Targets({targetQubitOne, targetQubitTwo}), qc::OpType::SWAP);
    ASSERT_TRUE(coreOperationSimulation(swapGateOperation, inputState));

    ASSERT_TRUE(inputState[0]);
    ASSERT_TRUE(inputState[1]);
    ASSERT_TRUE(inputState[2]);
    ASSERT_FALSE(inputState[3]);
}

TEST(SimpleSimulationTests, SimulationOfSwapOperationWithOnlySomeControlQubitsSet) {
    constexpr std::size_t   numQubits         = 4;
    constexpr std::uint64_t initialStateValue = 10; // 0101
    NBitValuesContainer     inputState(numQubits, initialStateValue);

    constexpr auto controlQubitOne = static_cast<qc::Qubit>(1);
    constexpr auto controlQubitTwo = static_cast<qc::Qubit>(2);

    constexpr auto targetQubitOne    = static_cast<qc::Qubit>(0);
    constexpr auto targetQubitTwo    = static_cast<qc::Qubit>(3);
    const auto     swapGateOperation = qc::StandardOperation(qc::Controls({controlQubitOne, controlQubitTwo}), qc::Targets({targetQubitOne, targetQubitTwo}), qc::OpType::SWAP);
    ASSERT_TRUE(coreOperationSimulation(swapGateOperation, inputState));

    ASSERT_FALSE(inputState[0]);
    ASSERT_TRUE(inputState[1]);
    ASSERT_FALSE(inputState[2]);
    ASSERT_TRUE(inputState[3]);
}

TEST(SimpleSimulationTests, SimulationWithMoreInputValuesProvidedThanQubitsInQuantumComputation) {
    AnnotatableQuantumComputation  quantumComputation;
    const std::optional<qc::Qubit> qubitIndex = quantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    constexpr std::size_t     inputStateSize = 2;
    const NBitValuesContainer inputState(inputStateSize);
    NBitValuesContainer       outputState;
    ASSERT_NO_FATAL_FAILURE(simpleSimulation(outputState, quantumComputation, inputState));
    ASSERT_EQ(0, outputState.size());
}

TEST(SimpleSimulationTests, SimulationWithLessInputValuesProvidedThanQubitsInQuantumComputation) {
    AnnotatableQuantumComputation  quantumComputation;
    const std::optional<qc::Qubit> qubitOneIndex = quantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(qubitOneIndex.has_value());
    ASSERT_EQ(0, *qubitOneIndex);

    const std::optional<qc::Qubit> qubitTwoIndex = quantumComputation.addNonAncillaryQubit("q1", false);
    ASSERT_TRUE(qubitTwoIndex.has_value());
    ASSERT_EQ(1, *qubitTwoIndex);

    constexpr std::size_t     inputStateSize = 1;
    const NBitValuesContainer inputState(inputStateSize);
    NBitValuesContainer       outputState;
    ASSERT_NO_FATAL_FAILURE(simpleSimulation(outputState, quantumComputation, inputState));
    ASSERT_EQ(0, outputState.size());
}

TEST(SimpleSimulationTests, SimulationRuntimePropertySet) {
    AnnotatableQuantumComputation  quantumComputation;
    const std::optional<qc::Qubit> qubitOneIndex = quantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(qubitOneIndex.has_value());
    ASSERT_EQ(0, *qubitOneIndex);

    const std::optional<qc::Qubit> qubitTwoIndex = quantumComputation.addNonAncillaryQubit("q1", false);
    ASSERT_TRUE(qubitTwoIndex.has_value());
    ASSERT_EQ(1, *qubitTwoIndex);

    ASSERT_TRUE(quantumComputation.addOperationsImplementingCnotGate(*qubitOneIndex, *qubitTwoIndex));

    constexpr std::size_t     inputStateSize  = 2;
    constexpr std::uint64_t   inputStateValue = 1;
    const NBitValuesContainer inputState(inputStateSize, inputStateValue); // 10
    NBitValuesContainer       outputState;
    const Properties::ptr     statistics = std::make_shared<Properties>();

    ASSERT_NO_FATAL_FAILURE(simpleSimulation(outputState, quantumComputation, inputState, statistics));

    ASSERT_EQ(2, outputState.size());
    ASSERT_TRUE(outputState[0]);
    ASSERT_TRUE(outputState[1]);

    ASSERT_NO_FATAL_FAILURE(statistics->get<double>("runtime"));
}
