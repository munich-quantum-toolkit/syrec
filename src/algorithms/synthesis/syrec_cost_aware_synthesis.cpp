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
#include "core/annotatable_quantum_computation.hpp"
#include "core/configurable_options.hpp"
#include "core/statistics.hpp"
#include "core/syrec/program.hpp"
#include "ir/Definitions.hpp"

#include <vector>

namespace syrec {
    bool CostAwareSynthesis::expAdd(const unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        // The result of the expression (a + b) is stored in the ancillary qubits 'lines' and synthesized by first copying the left hand side operand of the addition to the ancillary qubits followed by an
        // inplace addition to add the right hand side operand of the addition to the ancillary qubits 'lines'. Note that we need to pass the right hand side operand of the 'original' addition as the left hand side
        // of the inplace addition since the latter stores the result of the addition in the qubits passed as the right hand side operand.
        return getConstantLines(bitwidth, 0U, lines) && bitwiseCnot(annotatableQuantumComputation, lines, lhs) // duplicate lhs
            && inplaceAdd(annotatableQuantumComputation, rhs, lines);                                          // NOLINT(readability-suspicious-call-argument)
    }

    bool CostAwareSynthesis::expSubtract(const unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        // The result of the expression (a - b) is stored in the ancillary qubits 'lines' and synthesized by first copying the left hand side operand of the subtraction to the ancillary qubits followed by an
        // inplace subtraction to add the right hand side operand of the addition to the ancillary qubits 'lines'. Note that we need to pass the right hand side operand of the 'original' subtraction as the left hand side
        // of the inplace subtraction since the latter stores the result of the subtraction in the qubits passed as the right hand side operand.
        return getConstantLines(bitwidth, 0U, lines) && bitwiseCnot(annotatableQuantumComputation, lines, lhs) // duplicate lhs
            && inplaceSubtract(annotatableQuantumComputation, rhs, lines);                                     // NOLINT(readability-suspicious-call-argument)
    }

    bool CostAwareSynthesis::expExor(const unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        return getConstantLines(bitwidth, 0U, lines) && bitwiseCnot(annotatableQuantumComputation, lines, lhs) // duplicate lhs
            && bitwiseCnot(annotatableQuantumComputation, lines, rhs);
    }

    bool CostAwareSynthesis::synthesize(AnnotatableQuantumComputation& annotatableQuantumComputation, const Program& program, const ConfigurableOptions& settings, Statistics* optionalRecordedStatistics) {
        CostAwareSynthesis synthesizer(annotatableQuantumComputation);
        return SyrecSynthesis::synthesize(&synthesizer, program, settings, optionalRecordedStatistics);
    }
} // namespace syrec
