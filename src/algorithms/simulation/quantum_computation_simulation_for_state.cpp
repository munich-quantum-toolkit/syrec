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

#include "core/properties.hpp"
#include "dd/Package.hpp"
#include "dd/Simulation.hpp"
#include "ir/QuantumComputation.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {
    /*
     * Prefer the usage of std::chrono::steady_clock instead of std::chrono::system_clock since the former cannot decrease (due to time zone changes, etc.) and is most suitable for measuring intervals according to (https://en.cppreference.com/w/cpp/chrono/steady_clock)
     */
    using TimeStamp = std::chrono::time_point<std::chrono::steady_clock>;
} // namespace

namespace syrec {

    std::optional<std::vector<bool>> simulateQuantumComputationExecutionForState(const qc::QuantumComputation& quantumComputation, const std::vector<bool>& quantumComputationInputQubitValues,
                                                                                 const Properties::ptr& statistics) {
        if (quantumComputationInputQubitValues.size() != quantumComputation.getNqubitsWithoutAncillae()) {
            std::cerr << "Input state should only define the value of the " << std::to_string(quantumComputation.getNqubitsWithoutAncillae()) << " non-ancillae input qubits but user specified values for " << std::to_string(quantumComputationInputQubitValues.size()) << " qubits!";
            return std::nullopt;
        }

        if (quantumComputationInputQubitValues.empty()) {
            if (quantumComputation.getNqubitsWithoutAncillae() > 0) {
                std::cerr << "Input state must have at least one input qubit";
                return std::nullopt;
            }
            if (statistics != nullptr) {
                statistics->set("runtime", static_cast<double>(0));
            }
            return std::nullopt;
        }

        const TimeStamp   simulationStartTime = std::chrono::steady_clock::now();
        const std::size_t nQubits             = quantumComputation.getNqubits();
        // The user should only need to provide as many input values as their are input qubits defined in the quantum computation
        // while being able to ignore the initial values of ancillary qubits which are assumed to be initialized to zero.
        std::vector fullInitialState(nQubits, false);
        std::copy(quantumComputationInputQubitValues.cbegin(), quantumComputationInputQubitValues.cend(), fullInitialState.begin());

        auto dd = std::make_unique<dd::Package>(nQubits);
        // Instead of modifying the quantum computation with additional operations, the initial values of the input qubits are set
        // by modifying the initial state in the decision diagram. This also allows for the reuse of the quantum computation for future
        // simulation runs
        const dd::VectorDD decisionDiagramInitialState = dd->makeBasisState(nQubits, fullInitialState);
        auto               outputState                 = dd::simulate(quantumComputation, decisionDiagramInitialState, *dd);

        std::mt19937_64 rng{};

        // Instead of measure the whole output state, one could also measure the qubits of interest via dd->measureOneCollapsing(outputState, qubitIndex, rng)
        const std::string& stringifiedMeasurementsOfOutputState = dd->measureAll(outputState, false, rng);
        if (statistics != nullptr) {
            const TimeStamp simulationEndTime = std::chrono::steady_clock::now();
            const auto      simulationRunTime = std::chrono::duration_cast<std::chrono::milliseconds>(simulationEndTime - simulationStartTime);
            statistics->set("runtime", static_cast<double>(simulationRunTime.count()));
        }

        std::vector quantumComputationOutputQubitValues(quantumComputationInputQubitValues.size(), false);
        // According to the MQT::DD documentation, the most significant qubit (i.e. the one with the highest qubit index) is the left most qubit in the output measurement while
        // the least significant qubit is the rightmost entry in the output measurement. Note that ancillary and garbage qubits are included in the measured output state, thus the
        // range of qubit indices of interest is equal to [NQubits - 1, NAncillaries] with the index 'NQubits - 1' referring to the least significant qubit in the input state while
        // the most significant qubit in the output state is located at index 'NAncillaries'
        const std::size_t indexOfLeastSignificantQubit = quantumComputation.getNqubits() - 1;
        for (std::size_t i = 0; i < quantumComputationOutputQubitValues.size(); ++i) {
            quantumComputationOutputQubitValues[i] = stringifiedMeasurementsOfOutputState[indexOfLeastSignificantQubit - i] == '1';
        }
        return quantumComputationOutputQubitValues;
    }
} // namespace syrec
