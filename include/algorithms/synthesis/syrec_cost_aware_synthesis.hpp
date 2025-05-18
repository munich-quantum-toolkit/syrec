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
#include "core/properties.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <memory>
#include <vector>

namespace syrec {
    class CostAwareSynthesis: public SyrecSynthesis {
    public:
        using SyrecSynthesis::SyrecSynthesis;

        static bool synthesize(qc::QuantumComputation& quantumComputation, const Program& program, const Properties::ptr& settings = std::make_shared<Properties>(), const Properties::ptr& statistics = std::make_shared<Properties>());

    protected:
        bool processStatement(qc::QuantumComputation& quantumComputation, const Statement::ptr& statement) override {
            return SyrecSynthesis::onStatement(quantumComputation, statement);
        }

        bool assignAdd(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, [[maybe_unused]] unsigned op) override {
            return increase(quantumComputation, rhs, lhs);
        }

        bool assignSubtract(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, [[maybe_unused]] unsigned op) override {
            return decrease(quantumComputation, rhs, lhs);
        }

        bool assignExor(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op) override {
            return bitwiseCnot(quantumComputation, lhs, rhs);
        }

        bool expAdd(qc::QuantumComputation& quantumComputation, unsigned bitwidth, std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& lines) override;
        bool expSubtract(qc::QuantumComputation& quantumComputation, unsigned bitwidth, std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& lines) override;
        bool expExor(qc::QuantumComputation& quantumComputation, unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override;
    };
} // namespace syrec
