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
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <memory>
#include <vector>

namespace syrec {
    class LineAwareSynthesis: public SyrecSynthesis {
    public:
        using SyrecSynthesis::SyrecSynthesis;

        static bool synthesize(qc::QuantumComputation& circ, const Program& program, const Properties::ptr& settings = std::make_shared<Properties>(), const Properties::ptr& statistics = std::make_shared<Properties>());

    protected:
        bool processStatement(qc::QuantumComputation& quantumComputation, const Statement::ptr& statement) override;

        bool opRhsLhsExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& v) override;

        bool opRhsLhsExpression(const VariableExpression& expression, std::vector<qc::Qubit>& v) override;

        bool opRhsLhsExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& v) override;

        void popExp();

        bool inverse(qc::QuantumComputation& quantumComputation);

        bool assignAdd(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, unsigned op) override;

        bool assignSubtract(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, unsigned op) override;

        bool assignExor(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, unsigned op) override;

        bool solver(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& expRhs, unsigned statOp, const std::vector<qc::Qubit>& expLhs, unsigned expOp, const std::vector<qc::Qubit>& statLhs);

        bool flow(const Expression::ptr& expression, std::vector<qc::Qubit>& v);
        bool flow(const VariableExpression& expression, std::vector<qc::Qubit>& v);
        bool flow(const BinaryExpression& expression, const std::vector<qc::Qubit>& v);

        bool expAdd(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override {
            const bool synthesisOfExprOk = increase(quantumComputation, rhs, lhs);
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expSubtract(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override {
            const bool synthesisOfExprOk = decreaseNewAssign(quantumComputation, rhs, lhs);
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expExor(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override {
            const bool synthesisOfExprOk = bitwiseCnot(quantumComputation, rhs, lhs); // duplicate lhs
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expEvaluate(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lines, unsigned op, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs);

        bool expressionSingleOp(qc::QuantumComputation& quantumComputation, unsigned op, const std::vector<qc::Qubit>& expLhs, const std::vector<qc::Qubit>& expRhs);

        bool decreaseNewAssign(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);

        bool expressionOpInverse(qc::QuantumComputation& quantumComputation, [[maybe_unused]] unsigned op, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs) override;
    };
} // namespace syrec
