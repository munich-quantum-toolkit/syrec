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

#include "algorithms/synthesis/syrec_synthesis.hpp"
#include "core/circuit.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"

#include <vector>

namespace syrec {
    bool CostAwareSynthesis::expAdd(Circuit& circuit, const unsigned& bitwidth, std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs, const std::vector<unsigned>& lines) {
        getConstantLines(circuit, bitwidth, 0U, rhs);
        return bitwiseCnot(circuit, rhs, lhs) // duplicate lhs
            && increase(circuit, rhs, lines);
    }

    bool CostAwareSynthesis::expSubtract(Circuit& circuit, const unsigned& bitwidth, std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs, const std::vector<unsigned>& lines) {
        getConstantLines(circuit, bitwidth, 0U, rhs);
        return bitwiseCnot(circuit, rhs, lhs) // duplicate lhs
            && decrease(circuit, rhs, lines);
    }

    bool CostAwareSynthesis::expExor(Circuit& circuit, const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) {
        getConstantLines(circuit, bitwidth, 0U, lines);
        return bitwiseCnot(circuit, lines, lhs) // duplicate lhs
            && bitwiseCnot(circuit, lines, rhs);
    }

    bool CostAwareSynthesis::synthesize(Circuit& circ, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        CostAwareSynthesis synthesizer(circ);
        return SyrecSynthesis::synthesize(&synthesizer, circ, program, settings, statistics);
    }
} // namespace syrec
