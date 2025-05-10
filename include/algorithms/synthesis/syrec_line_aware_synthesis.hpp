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
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"

#include <memory>
#include <vector>

namespace syrec {
    class LineAwareSynthesis: public SyrecSynthesis {
    public:
        using SyrecSynthesis::SyrecSynthesis;

        static bool synthesize(Circuit& circ, const Program& program, const Properties::ptr& settings = std::make_shared<Properties>(), const Properties::ptr& statistics = std::make_shared<Properties>());

    protected:
        bool processStatement(Circuit& circuit, const Statement::ptr& statement) override;

        bool opRhsLhsExpression(const Expression::ptr& expression, std::vector<unsigned>& v) override;

        bool opRhsLhsExpression(const VariableExpression& expression, std::vector<unsigned>& v) override;

        bool opRhsLhsExpression(const BinaryExpression& expression, std::vector<unsigned>& v) override;

        void popExp();

        bool inverse(Circuit& circuit);

        bool assignAdd(Circuit& circuit, std::vector<unsigned>& rhs, std::vector<unsigned>& lhs, const unsigned& op) override;

        bool assignSubtract(Circuit& circuit, std::vector<unsigned>& rhs, std::vector<unsigned>& lhs, const unsigned& op) override;

        bool assignExor(Circuit& circuit, std::vector<unsigned>& lhs, std::vector<unsigned>& rhs, const unsigned& op) override;

        bool solver(Circuit& circuit, const std::vector<unsigned>& expRhs, unsigned statOp, const std::vector<unsigned>& expLhs, unsigned expOp, const std::vector<unsigned>& statLhs);

        bool flow(const Expression::ptr& expression, std::vector<unsigned>& v);
        bool flow(const VariableExpression& expression, std::vector<unsigned>& v);
        bool flow(const BinaryExpression& expression, const std::vector<unsigned>& v);

        bool expAdd(Circuit& circuit, [[maybe_unused]] const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) override {
            const bool synthesisOfExprOk = increase(circuit, rhs, lhs);
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expSubtract(Circuit& circuit, [[maybe_unused]] const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) override {
            const bool synthesisOfExprOk = decreaseNewAssign(circuit, rhs, lhs);
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expExor(Circuit& circuit, [[maybe_unused]] const unsigned& bitwidth, std::vector<unsigned>& lines, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) override {
            const bool synthesisOfExprOk = bitwiseCnot(circuit, rhs, lhs); // duplicate lhs
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expEvaluate(Circuit& circuit, std::vector<unsigned>& lines, unsigned op, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) const;

        bool expressionSingleOp(Circuit& circuit, unsigned op, const std::vector<unsigned>& expLhs, const std::vector<unsigned>& expRhs) const;

        static bool decreaseNewAssign(Circuit& circuit, const std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs);

        bool expressionOpInverse(Circuit& circuit, [[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<unsigned>& expLhs, [[maybe_unused]] const std::vector<unsigned>& expRhs) const override;
    };
} // namespace syrec
