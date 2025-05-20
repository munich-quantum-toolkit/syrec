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

#include "core/properties.hpp"
#include "ir/QuantumComputation.hpp"

#include <vector>

namespace syrec {
    /**
     * Simulate a series of quantum operations on a given input quantum state using the MQT::Core decision diagram functionality.
     *
     * Note that the value of the garbage qubits in the output state can probably be ignored.
     * @param quantumComputation The quantum computation containing the quantum operations to simulate
     * @param quantumComputationInputQubitValues The initial values of the non-ancillary input qubits of the \p quantumComputation. Ancillary qubits are initialized to 0. The value of the least significant qubit starts at index 0 while the value of the most significant qubit is defined at the end of the container.
     * @param quantumComputationOutputQubitValues The output values of the non-ancillary output qubits in the output state of the \p quantumComputation (i.e. after the simulation was completed). The value of the least significant qubit starts at index 0 while the value of the most significant qubit is defined at the end of the container.
     * @param statistics Container to fetch settings from and store statistics to. Will store the measured allocated CPU time (unit: milliseconds) required for the simulation.
     */
    void simulateQuantumComputationExecutionForState(const qc::QuantumComputation& quantumComputation, const std::vector<bool>& quantumComputationInputQubitValues, std::vector<bool>& quantumComputationOutputQubitValues, const Properties::ptr& statistics);
} // namespace syrec
