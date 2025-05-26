/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/quantum_computation_simulation_for_state.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "ir/Definitions.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace syrec;

static const std::string EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY = "runtime";

TEST(SimulationInterfaceErrorCasesTests, EmptyInputStateInQuantumComputationWithMoreThanOneDataQubitNotAllowed) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    const Properties::ptr         statistics = std::make_shared<Properties>();

    const std::optional<qc::Qubit> addedQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(addedQubitIndex.has_value());
    ASSERT_EQ(0, *addedQubitIndex);

    const std::vector<bool>          quantumComputationInputQubitValues;
    std::optional<std::vector<bool>> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(quantumComputationOutputQubitValues = simulateQuantumComputationExecutionForState(annotatableQuantumComputation, quantumComputationInputQubitValues, statistics));
    ASSERT_FALSE(quantumComputationOutputQubitValues.has_value());
    // With the current syrec::Properties interface our only option to check that a property does not exist is by using the .get(...) call without providing a fall-back default value.
    // This assertion will however not catch the expected exception in debug builds since an internal assert fails which is not caught by the gtest assertion. Thus we omit the non-existance
    // check of the runtime property
    //ASSERT_ANY_THROW(statistics->get<double>(EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY));
}

TEST(SimulationInterfaceErrorCasesTests, EmptyInputStateInQuantumComputationWithNoDataQubitsAllowed) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    const Properties::ptr         statistics = std::make_shared<Properties>();

    const std::optional<qc::Qubit> addedQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit("q0", false);
    ASSERT_TRUE(addedQubitIndex.has_value());
    ASSERT_EQ(0, *addedQubitIndex);

    annotatableQuantumComputation.setLogicalQubitAncillary(*addedQubitIndex);

    const std::vector<bool>          quantumComputationInputQubitValues;
    std::optional<std::vector<bool>> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(quantumComputationOutputQubitValues = simulateQuantumComputationExecutionForState(annotatableQuantumComputation, quantumComputationInputQubitValues, statistics));
    ASSERT_FALSE(quantumComputationOutputQubitValues.has_value());
    ASSERT_NO_FATAL_FAILURE(statistics->get<double>(EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY));
}

TEST(SimulationInterfaceErrorCasesTests, ProvidingLessQubitValuesThanDataQubitsAsInputStateNotAllowed) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    const Properties::ptr         statistics = std::make_shared<Properties>();

    const std::optional<qc::Qubit> firstNonAncillaryQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(firstNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(0, *firstNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> secondNonAncillaryQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q1", false);
    ASSERT_TRUE(secondNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *secondNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> thirdNonAncillaryQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q3", true);
    ASSERT_TRUE(thirdNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(2, *thirdNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> ancillaryQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit("q2", true);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(3, *ancillaryQubitIndex);
    annotatableQuantumComputation.setLogicalQubitAncillary(*ancillaryQubitIndex);

    ASSERT_EQ(3, annotatableQuantumComputation.getNqubitsWithoutAncillae());
    ASSERT_EQ(1, annotatableQuantumComputation.getNancillae());

    const std::vector                quantumComputationInputQubitValues = {true};
    std::optional<std::vector<bool>> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(quantumComputationOutputQubitValues = simulateQuantumComputationExecutionForState(annotatableQuantumComputation, quantumComputationInputQubitValues, statistics));
    ASSERT_FALSE(quantumComputationOutputQubitValues.has_value());
    // With the current syrec::Properties interface our only option to check that a property does not exist is by using the .get(...) call without providing a fall-back default value.
    // This assertion will however not catch the expected exception in debug builds since an internal assert fails which is not caught by the gtest assertion. Thus we omit the non-existance
    // check of the runtime property
    //ASSERT_ANY_THROW(statistics->get<double>(EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY));
}

TEST(SimulationInterfaceErrorCasesTests, ProvidingMoreQubitValuesThanDataQubitsAsInputStateNotAllowed) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    const Properties::ptr         statistics = std::make_shared<Properties>();

    const std::optional<qc::Qubit> firstNonAncillaryQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(firstNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(0, *firstNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> secondNonAncillaryQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q1", false);
    ASSERT_TRUE(secondNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *secondNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> thirdNonAncillaryQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q3", true);
    ASSERT_TRUE(thirdNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(2, *thirdNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> ancillaryQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit("q2", true);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(3, *ancillaryQubitIndex);
    annotatableQuantumComputation.setLogicalQubitAncillary(*ancillaryQubitIndex);

    ASSERT_EQ(3, annotatableQuantumComputation.getNqubitsWithoutAncillae());
    ASSERT_EQ(1, annotatableQuantumComputation.getNancillae());

    const std::vector                quantumComputationInputQubitValues = {true, false, true, false};
    std::optional<std::vector<bool>> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(quantumComputationOutputQubitValues = simulateQuantumComputationExecutionForState(annotatableQuantumComputation, quantumComputationInputQubitValues, statistics));
    ASSERT_FALSE(quantumComputationOutputQubitValues.has_value());
    // With the current syrec::Properties interface our only option to check that a property does not exist is by using the .get(...) call without providing a fall-back default value.
    // This assertion will however not catch the expected exception in debug builds since an internal assert fails which is not caught by the gtest assertion. Thus we omit the non-existance
    // check of the runtime property
    //ASSERT_ANY_THROW(statistics->get<double>(EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY));
}

TEST(SimulationInterfaceErrorCasesTests, CheckRuntimePropertyIsSet) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    const Properties::ptr         statistics = std::make_shared<Properties>();

    const std::optional<qc::Qubit> addedQubitIndex = annotatableQuantumComputation.addPreliminaryAncillaryQubit("q0", false);
    ASSERT_TRUE(addedQubitIndex.has_value());
    ASSERT_EQ(0, *addedQubitIndex);

    annotatableQuantumComputation.setLogicalQubitAncillary(*addedQubitIndex);

    const std::vector<bool>          quantumComputationInputQubitValues;
    std::optional<std::vector<bool>> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(quantumComputationOutputQubitValues = simulateQuantumComputationExecutionForState(annotatableQuantumComputation, quantumComputationInputQubitValues, statistics));
    ASSERT_FALSE(quantumComputationOutputQubitValues.has_value());
    ASSERT_NO_FATAL_FAILURE(statistics->get<double>(EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY));
}

TEST(SimulationInterfaceErrorCasesTests, CheckFetchingRuntimePropertyWhenStatisticsContainerIsNotSet) {
    AnnotatableQuantumComputation annotatableQuantumComputation;
    const Properties::ptr         statistics = nullptr;

    const std::optional<qc::Qubit> addedQubitIndex = annotatableQuantumComputation.addNonAncillaryQubit("q0", false);
    ASSERT_TRUE(addedQubitIndex.has_value());
    ASSERT_EQ(0, *addedQubitIndex);

    annotatableQuantumComputation.x(*addedQubitIndex);

    const std::vector                quantumComputationInputQubitValues = {true};
    std::optional<std::vector<bool>> quantumComputationOutputQubitValues;
    ASSERT_NO_FATAL_FAILURE(quantumComputationOutputQubitValues = simulateQuantumComputationExecutionForState(annotatableQuantumComputation, quantumComputationInputQubitValues, statistics));
    ASSERT_TRUE(quantumComputationOutputQubitValues.has_value());
    ASSERT_EQ(1, quantumComputationOutputQubitValues->size());
    // With the current syrec::Properties interface our only option to check that a property does not exist is by using the .get(...) call without providing a fall-back default value.
    // This assertion will however not catch the expected exception in debug builds since an internal assert fails which is not caught by the gtest assertion. Thus we omit the non-existance
    // check of the runtime property
    //ASSERT_ANY_THROW(statistics->get<double>(EXPECTED_SIMULATION_RUNTIME_PROPERTY_KEY));
}