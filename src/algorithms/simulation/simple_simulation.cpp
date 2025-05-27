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

#include "core/n_bit_values_container.hpp"
#include "core/properties.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>

using namespace syrec;

namespace {
    //Prefer the usage of std::chrono::steady_clock instead of std::chrono::system_clock since the former cannot decrease (due to time zone changes, etc.) and is most suitable for measuring intervals according to (https://en.cppreference.com/w/cpp/chrono/steady_clock)
    using TimeStamp = std::chrono::time_point<std::chrono::steady_clock>;

    bool areAllControlQubitsSetInState(const qc::Controls& controlQubits, const NBitValuesContainer& state) {
        return controlQubits.empty() || std::all_of(controlQubits.cbegin(), controlQubits.cend(), [&state](const qc::Control& controlQubit) { return state.test(controlQubit.qubit).value_or(false); });
    }
} // namespace

bool syrec::coreOperationSimulation(const qc::Operation& op, NBitValuesContainer& input) {
    const auto gateType = op.getType();
    if (gateType == qc::OpType::X) {
        // TODO: Handle control or target qubits being out of range, targets being empty
        if (areAllControlQubitsSetInState(op.getControls(), input)) {
            input.flip(op.getTargets().front());
        }
        return true;
    }
    if (gateType == qc::OpType::SWAP) {
        // TODO: Handle control or target qubits being out of range, targets being empty
        if (areAllControlQubitsSetInState(op.getControls(), input)) {
            const qc::Qubit targetQubitOne = op.getTargets()[0];
            const qc::Qubit targetQubitTwo = op.getTargets()[1];

            const bool valueOfTargetQubitOne = input[targetQubitOne];
            if (input[targetQubitOne] != input[targetQubitTwo]) {
                input.set(targetQubitOne, input[targetQubitTwo]);
                input.set(targetQubitTwo, valueOfTargetQubitOne);
            }
        }
        return true;
    }
    std::cerr << "Cannot simulate gate of type " << std::to_string(gateType) << "\n";
    return false;
}

void syrec::simpleSimulation(NBitValuesContainer& output, const qc::QuantumComputation& quantumComputation, const NBitValuesContainer& input, const Properties::ptr& statistics) {
    if (input.size() != quantumComputation.getNqubits()) {
        std::cerr << "Input state size (" << input.size() << ") must match number of qubits in the quantum computation (" << quantumComputation.getNqubits() << ")\n";
        return;
    }

    const TimeStamp simulationStartTime = std::chrono::steady_clock::now();

    output = input;
    for (std::size_t i = 0; i < quantumComputation.getNops(); ++i) {
        if (const auto& op = quantumComputation.at(i); op != nullptr) {
            if (!coreOperationSimulation(*quantumComputation.at(i), output)) {
                return;
            }
        } else {
            std::cerr << "Operation " << std::to_string(i) + " in quantum computation was NULL!\n";
            return;
        }
    }

    const TimeStamp simulationEndTime = std::chrono::steady_clock::now();
    const auto      simulationRunTime = std::chrono::duration_cast<std::chrono::milliseconds>(simulationEndTime - simulationStartTime);
    if (statistics != nullptr) {
        statistics->set("runtime", static_cast<double>(simulationRunTime.count()));
    }
}
