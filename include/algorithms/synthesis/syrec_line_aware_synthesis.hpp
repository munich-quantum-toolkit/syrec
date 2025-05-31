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
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "ir/Definitions.hpp"

#include <memory>
#include <vector>

namespace syrec {
    class LineAwareSynthesis: public SyrecSynthesis {
    public:
        using SyrecSynthesis::SyrecSynthesis;

        static bool synthesize(AnnotatableQuantumComputation& annotatableQuantumComputation, const Program& program, const Properties::ptr& settings = std::make_shared<Properties>(), const Properties::ptr& statistics = std::make_shared<Properties>());

    protected:
        bool processStatement(const Statement::ptr& statement) override;

        bool opRhsLhsExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& v) override;

        bool opRhsLhsExpression(const VariableExpression& expression, std::vector<qc::Qubit>& v) override;

        bool opRhsLhsExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& v) override;

        void popExp();

        bool inverse();

        bool assignAdd(std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) override;

        bool assignSubtract(std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) override;

        bool assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation) override;

        bool solver(const std::vector<qc::Qubit>& expRhs, [[maybe_unused]] AssignStatement::AssignOperation assignOperation, const std::vector<qc::Qubit>& expLhs, BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& statLhs);

        bool flow(const Expression::ptr& expression, std::vector<qc::Qubit>& v);
        bool flow(const VariableExpression& expression, std::vector<qc::Qubit>& v);
        bool flow(const BinaryExpression& expression, const std::vector<qc::Qubit>& v);

        bool expAdd([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override {
            const bool synthesisOfExprOk = increase(annotatableQuantumComputation, rhs, lhs);
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expSubtract([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override {
            const bool synthesisOfExprOk = decreaseNewAssign(annotatableQuantumComputation, rhs, lhs);
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expExor([[maybe_unused]] unsigned bitwidth, std::vector<qc::Qubit>& lines, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) override {
            const bool synthesisOfExprOk = bitwiseCnot(annotatableQuantumComputation, rhs, lhs); // duplicate lhs
            lines                        = rhs;
            return synthesisOfExprOk;
        }

        bool expEvaluate(std::vector<qc::Qubit>& lines, BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) const;

        bool expressionSingleOp(BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& expLhs, const std::vector<qc::Qubit>& expRhs) const;

        static bool decreaseNewAssign(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs);

        bool expressionOpInverse([[maybe_unused]] BinaryExpression::BinaryOperation binaryOperation, [[maybe_unused]] const std::vector<qc::Qubit>& expLhs, [[maybe_unused]] const std::vector<qc::Qubit>& expRhs) override;
    };
} // namespace syrec
