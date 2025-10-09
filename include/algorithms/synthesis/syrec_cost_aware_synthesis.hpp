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

#include "algorithms/synthesis/syrec_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/configurable_options.hpp"
#include "core/statistics.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "ir/Definitions.hpp"

#include <memory>
#include <vector>

namespace syrec {
    class CostAwareSynthesis: public SyrecSynthesis {
    public:
        using SyrecSynthesis::SyrecSynthesis;

        static bool synthesize(AnnotatableQuantumComputation& annotatableQuantumComputation, const Program& program, const ConfigurableOptions& settings = ConfigurableOptions(), Statistics* optionalRecordedStatistics = nullptr);

    protected:
        bool processStatement(const Statement::ptr& statement) override {
            return SyrecSynthesis::onStatement(statement);
        }

        bool assignAdd(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) override {
            // The assignment lhs += rhs is synthesized using the inplace addition which stores the result of the addition in the qubits passed as the right hand side operand thus the operands of the assignment need to be passed in the reverse order.
            return inplaceAdd(annotatableQuantumComputation, rhs, lhs); // NOLINT(readability-suspicious-call-argument)
        }

        bool assignSubtract(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) override {
            // The assignment lhs -= rhs is synthesized using the inplace subtraction which stores the result of the subtraction in the qubits passed as the right hand side operand thus the operands of the assignment need to be passed in the reverse order.
            return inplaceSubtract(annotatableQuantumComputation, rhs, lhs); // NOLINT(readability-suspicious-call-argument)
        }

        bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) override {
            return bitwiseCnot(annotatableQuantumComputation, lhs, rhs);
        }

        bool expAdd(unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override;
        bool expSubtract(unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override;
        bool expExor(unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override;
    };
} // namespace syrec
