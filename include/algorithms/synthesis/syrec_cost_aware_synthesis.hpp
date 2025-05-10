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
#include "core/circuit.hpp"
#include "core/properties.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"

#include <memory>
#include <vector>

namespace syrec {
    class CostAwareSynthesis: public SyrecSynthesis {
    public:
        using SyrecSynthesis::SyrecSynthesis;

        static bool synthesize(Circuit& circ, const Program& program, const Properties::ptr& settings = std::make_shared<Properties>(), const Properties::ptr& statistics = std::make_shared<Properties>());

    protected:
        bool processStatement(Circuit& circuit, const Statement::ptr& statement) override {
            return SyrecSynthesis::onStatement(circuit, statement);
        }

        bool assignAdd(Circuit& circuit, std::vector<unsigned>& rhs, std::vector<unsigned>& lhs, [[maybe_unused]] const unsigned& op) override {
            return increase(circuit, rhs, lhs);
        }

        bool assignSubtract(Circuit& circuit, std::vector<unsigned>& rhs, std::vector<unsigned>& lhs, [[maybe_unused]] const unsigned& op) override {
            return decrease(circuit, rhs, lhs);
        }

        bool assignExor(Circuit& circuit, std::vector<unsigned>& lhs, std::vector<unsigned>& rhs, [[maybe_unused]] const unsigned& op) override {
            return bitwiseCnot(circuit, lhs, rhs);
        }

        bool expAdd(Circuit& circuit, const unsigned& bitwidth, std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs, const std::vector<unsigned>& lines) override;
        bool expSubtract(Circuit& circuit, const unsigned& bitwidth, std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs, const std::vector<unsigned>& lines) override;
        bool expExor(Circuit& circuit, const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) override;
    };
} // namespace syrec
