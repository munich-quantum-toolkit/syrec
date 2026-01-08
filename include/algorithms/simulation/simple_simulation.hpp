/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "core/n_bit_values_container.hpp"
#include "core/statistics.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Operation.hpp"

namespace syrec {
    /**
    * @brief Simulation for a single gate \p g
    *
    * This operator performs simulation for a single gate and is called by
    * \ref syrec::simple_simulation "simple_simulation".
    *
    * \b Important: The operator should modify \p input directly.
    *
    * @param op     The quantum operation to simulate
    * @param input An input pattern
    * @returns Whether the operation could be applied.
    */
    [[nodiscard]] bool coreOperationSimulation(const qc::Operation& op, NBitValuesContainer& input);

    /**
    * @brief Simple Simulation function for a circuit
    *
    * This method calls the \em gate_simulation setting's functor on
    * all gates of the circuit \p circ. Thereby,
    * the last calculated output pattern serves as the input pattern
    * for the next gate. The last calculated output pattern is written
    * to \p output.
    *
    * @param output Output pattern. The index of the pattern corresponds to the line index.
    * @param quantumComputation Quantum computation to be simulated.
    * @param input Input pattern. The index of the pattern corresponds to the line index.
    *              The bit-width of the input pattern has to be initialized properly to the
    *              number of lines.
    * @param optionalRecordedStatistics Container to optionally store recorded statistics during the simulation of the given input state.
    */
    void simpleSimulation(NBitValuesContainer& output, const qc::QuantumComputation& quantumComputation, const NBitValuesContainer& input, Statistics* optionalRecordedStatistics = nullptr);
} // namespace syrec
