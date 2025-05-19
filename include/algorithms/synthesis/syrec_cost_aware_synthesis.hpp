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
        bool processStatement(const Statement::ptr& statement) override {
            return SyrecSynthesis::onStatement(statement);
        }

        bool assignAdd(std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, [[maybe_unused]] unsigned op) override {
            return increase(rhs, lhs);
        }

        bool assignSubtract(std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, [[maybe_unused]] unsigned op) override {
            return decrease(rhs, lhs);
        }

        bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] unsigned op) override {
            return bitwiseCnot(lhs, rhs);
        }

        bool expAdd(unsigned bitwidth, std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& lines) override;
        bool expSubtract(unsigned bitwidth, std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& lines) override;
        bool expExor(unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override;
    };
} // namespace syrec
