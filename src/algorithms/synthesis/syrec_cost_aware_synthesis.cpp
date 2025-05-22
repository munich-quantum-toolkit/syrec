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
#include "core/properties.hpp"
#include "core/syrec/program.hpp"
#include "ir/Definitions.hpp"

#include <vector>

namespace syrec {
    bool CostAwareSynthesis::expAdd(const unsigned bitwidth, std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& lines) {
        return getConstantLines(bitwidth, 0U, rhs) && bitwiseCnot(annotatableQuantumComputation, rhs, lhs) // duplicate lhs
            && increase(annotatableQuantumComputation, rhs, lines);
    }

    bool CostAwareSynthesis::expSubtract(const unsigned bitwidth, std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& lines) {
        return getConstantLines(bitwidth, 0U, rhs) && bitwiseCnot(annotatableQuantumComputation, rhs, lhs) // duplicate lhs
            && decrease(annotatableQuantumComputation, rhs, lines);
    }

    bool CostAwareSynthesis::expExor(const unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        return getConstantLines(bitwidth, 0U, lines) && bitwiseCnot(annotatableQuantumComputation, lines, lhs) // duplicate lhs
            && bitwiseCnot(annotatableQuantumComputation, lines, rhs);
    }

    bool CostAwareSynthesis::synthesize(AnnotatableQuantumComputation& annotatableQuantumComputation, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        CostAwareSynthesis synthesizer(annotatableQuantumComputation);
        return SyrecSynthesis::synthesize(&synthesizer, program, settings, statistics);
    }
} // namespace syrec
