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

#include <memory>
#include <vector>

namespace syrec {
    /**
    * TODO
    * @brief Simple Simulation function for a circuit
    *
    * This method calls the \em gate_simulation setting's functor on
    * all gates of the circuit \p circ. Thereby,
    * the last calculated output pattern serves as the input pattern
    * for the next gate. The last calculated output pattern is written
    * to \p output.
    *
    * @param quantumComputation TODO
    * @param quantumComputationInputQubitValues TODO
    * @param quantumComputationOutputQubitValues TODO
    * @param statistics <table border="0" width="100%">
    *   <tr>
    *     <td class="indexkey">Information</td>
    *     <td class="indexkey">Type</td>
    *     <td class="indexkey">Description</td>
    *   </tr>
    *   <tr>
    *     <td class="indexvalue">runtime</td>
    *     <td class="indexvalue">double</td>
    *     <td class="indexvalue">Run-time consumed by the algorithm in CPU seconds.</td>
    *   </tr>
    * </table>
    */
    void simulateQuantumComputationExecutionForState(const qc::QuantumComputation& quantumComputation, const std::vector<bool>& quantumComputationInputQubitValues, std::vector<bool>& quantumComputationOutputQubitValues, const Properties::ptr& statistics);
} // namespace syrec
