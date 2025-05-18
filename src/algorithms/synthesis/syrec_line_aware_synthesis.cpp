/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"

#include "algorithms/synthesis/syrec_synthesis.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace syrec {
    bool LineAwareSynthesis::processStatement(qc::QuantumComputation& quantumComputation, const Statement::ptr& statement) {
        const auto* const stmtCastedAsAssignmentStmt = dynamic_cast<const AssignStatement*>(statement.get());
        if (stmtCastedAsAssignmentStmt == nullptr) {
            return SyrecSynthesis::onStatement(quantumComputation, statement);
        }

        const AssignStatement& assignmentStmt = *stmtCastedAsAssignmentStmt;
        std::vector<qc::Qubit> d;
        std::vector<qc::Qubit> dd;
        std::vector<qc::Qubit> ddd;
        std::vector<qc::Qubit> statLhs;
        getVariables(assignmentStmt.lhs, statLhs);

        // The line aware synthesis of an assignment can only be performed when the rhs input signals are repeated (since the results are stored in the rhs)
        // and the right-hand side expression of the assignment consists of only Variable- or BinaryExpressions with the latter only containing the operations (+, - or ^).
        const bool canAssignmentSynthesisBeOptimized = opRhsLhsExpression(assignmentStmt.rhs, d) && !opVec.empty() && flow(assignmentStmt.rhs, ddd) && checkRepeats() && flow(assignmentStmt.rhs, dd);
        if (!canAssignmentSynthesisBeOptimized) {
            expOpVector.clear();
            assignOpVector.clear();
            expLhsVector.clear();
            expRhsVector.clear();
            opVec.clear();
            return SyrecSynthesis::onStatement(quantumComputation, statement);
        }

        setOrUpdateGlobalGateAnnotation(GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

        bool synthesisOk = true;
        if (expOpVector.size() == 1) {
            if (expOpVector.at(0) == 1 || expOpVector.at(0) == 2) {
                /// cancel out the signals
                expOpVector.clear();
                assignOpVector.clear();
                expLhsVector.clear();
                expRhsVector.clear();
                opVec.clear();
            } else {
                if (assignmentStmt.op == 1) {
                    synthesisOk = expressionSingleOp(quantumComputation, 1, expLhsVector.at(0), statLhs) &&
                                  expressionSingleOp(quantumComputation, 1, expRhsVector.at(0), statLhs);
                } else {
                    synthesisOk = expressionSingleOp(quantumComputation, assignmentStmt.op, expLhsVector.at(0), statLhs) &&
                                  expressionSingleOp(quantumComputation, expOpVector.at(0), expRhsVector.at(0), statLhs);
                }
                expOpVector.clear();
                assignOpVector.clear();
                expLhsVector.clear();
                expRhsVector.clear();
                opVec.clear();
            }
            return synthesisOk;
        }

        std::vector<qc::Qubit> lines;
        if (expLhsVector.at(0) == expRhsVector.at(0)) {
            if (expOpVector.at(0) == 1 || expOpVector.at(0) == 2) {
                /// cancel out the signals
            } else if (expOpVector.at(0) != 1 || expOpVector.at(0) != 2) {
                synthesisOk = expressionSingleOp(quantumComputation, assignmentStmt.op, expLhsVector.at(0), statLhs) &&
                              expressionSingleOp(quantumComputation, expOpVector.at(0), expRhsVector.at(0), statLhs);
            }
        } else {
            synthesisOk = solver(quantumComputation, statLhs, assignmentStmt.op, expLhsVector.at(0), expOpVector.at(0), expRhsVector.at(0));
        }

        const std::size_t z = (expOpVector.size() - static_cast<std::size_t>(expOpVector.size() % 2 == 0)) / 2;
        // TODO: Replace qc::Qubit type with syrec typed enum after changes have been merged
        std::vector<qc::Qubit> statAssignOp(z == 0 ? 1 : z, 0);

        for (std::size_t k = 0; k <= z - 1; k++) {
            statAssignOp[k] = assignOpVector[k];
        }

        /// Assignment operations
        std::reverse(statAssignOp.begin(), statAssignOp.end());

        /// If reversible assignment is "-", the assignment operations must negated appropriately
        if (assignmentStmt.op == 1) {
            for (qc::Qubit& i: statAssignOp) {
                if (i == 0) {
                    i = 1;
                } else if (i == 1) {
                    i = 0;
                }
            }
        }

        std::size_t j = 0;
        for (std::size_t i = 1; i <= expOpVector.size() - 1 && synthesisOk; i++) {
            /// when both rhs and lhs exist
            if ((!expLhsVector.at(i).empty()) && (!expRhsVector.at(i).empty())) {
                if (expLhsVector.at(i) == expRhsVector.at(i)) {
                    if (expOpVector.at(i) == 1 || expOpVector.at(i) == 2) {
                        /// cancel out the signals
                        j++;
                    } else if (expOpVector.at(i) != 1 || expOpVector.at(i) != 2) {
                        if (statAssignOp.at(j) == 1) {
                            synthesisOk = expressionSingleOp(quantumComputation, 1, expLhsVector.at(i), statLhs) &&
                                          expressionSingleOp(quantumComputation, 1, expRhsVector.at(i), statLhs);
                            j++;
                        } else {
                            synthesisOk = expressionSingleOp(quantumComputation, statAssignOp.at(j), expLhsVector.at(i), statLhs) &&
                                          expressionSingleOp(quantumComputation, expOpVector.at(i), expRhsVector.at(i), statLhs);
                            j++;
                        }
                    }
                } else {
                    synthesisOk = solver(quantumComputation, statLhs, statAssignOp.at(j), expLhsVector.at(i), expOpVector.at(i), expRhsVector.at(i));
                    j++;
                }
            }
            /// when only lhs exists o rhs exists
            else if (((expLhsVector.at(i).empty()) && !(expRhsVector.at(i).empty())) || ((!expLhsVector.at(i).empty()) && (expRhsVector.at(i).empty()))) {
                synthesisOk = expEvaluate(quantumComputation, lines, statAssignOp.at(j), expRhsVector.at(i), statLhs);
                j           = j + 1;
            }
        }
        expOpVector.clear();
        assignOpVector.clear();
        expLhsVector.clear();
        expRhsVector.clear();
        opVec.clear();
        return synthesisOk;
    }

    bool LineAwareSynthesis::flow(const Expression::ptr& expression, std::vector<qc::Qubit>& v) {
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return (binary->op == BinaryExpression::Add || binary->op == BinaryExpression::Subtract || binary->op == BinaryExpression::Exor) && flow(*binary, v);
        }
        if (auto const* var = dynamic_cast<VariableExpression*>(expression.get())) {
            return flow(*var, v);
        }
        return false;
    }

    bool LineAwareSynthesis::flow(const VariableExpression& expression, std::vector<qc::Qubit>& v) {
        getVariables(expression.var, v);
        return true;
    }

    /// generating LHS and RHS (can be whole expressions as well)
    bool LineAwareSynthesis::flow(const BinaryExpression& expression, const std::vector<qc::Qubit>& v [[maybe_unused]]) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;
        assignOpVector.push_back(expression.op);

        if (!flow(expression.lhs, lhs) || !flow(expression.rhs, rhs)) {
            return false;
        }

        expLhsVector.push_back(lhs);
        expRhsVector.push_back(rhs);
        expOpVector.push_back(expression.op);
        return true;
    }

    bool LineAwareSynthesis::solver(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& expRhs, qc::Qubit statOp, const std::vector<qc::Qubit>& expLhs, qc::Qubit expOp, const std::vector<qc::Qubit>& statLhs) {
        bool synthesisOk = true;
        if (statOp == expOp) {
            if (expOp == 1) {
                synthesisOk = expressionSingleOp(quantumComputation, 1, expLhs, expRhs) &&
                              expressionSingleOp(quantumComputation, 0, statLhs, expRhs);
            } else {
                synthesisOk = expressionSingleOp(quantumComputation, statOp, expLhs, expRhs) &&
                              expressionSingleOp(quantumComputation, statOp, statLhs, expRhs);
            }
        } else {
            std::vector<qc::Qubit> lines;
            subFlag     = true;
            synthesisOk = expEvaluate(quantumComputation, lines, expOp, expLhs, statLhs);
            subFlag     = false;
            synthesisOk &= expEvaluate(quantumComputation, lines, statOp, lines, expRhs);
            subFlag = true;
            if (expOp < 3) {
                synthesisOk &= expressionOpInverse(quantumComputation, expOp, expLhs, statLhs);
            }
        }
        subFlag = false;
        return synthesisOk;
    }

    bool LineAwareSynthesis::opRhsLhsExpression(const Expression::ptr& expression, std::vector<qc::Qubit>& v) {
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return opRhsLhsExpression(*binary, v);
        }
        if (auto const* var = dynamic_cast<VariableExpression*>(expression.get())) {
            return opRhsLhsExpression(*var, v);
        }
        return false;
    }

    bool LineAwareSynthesis::opRhsLhsExpression(const VariableExpression& expression, std::vector<qc::Qubit>& v) {
        getVariables(expression.var, v);
        return true;
    }

    bool LineAwareSynthesis::opRhsLhsExpression(const BinaryExpression& expression, std::vector<qc::Qubit>& v) {
        std::vector<qc::Qubit> lhs;
        std::vector<qc::Qubit> rhs;

        if (!opRhsLhsExpression(expression.lhs, lhs) || !opRhsLhsExpression(expression.rhs, rhs)) {
            return false;
        }
        v = rhs;
        opVec.push_back(expression.op);
        return true;
    }

    void LineAwareSynthesis::popExp() {
        expOpp.pop();
        expLhss.pop();
        expRhss.pop();
    }

    bool LineAwareSynthesis::inverse(qc::QuantumComputation& quantumComputation) {
        const bool synthesisOfInversionOk = expressionOpInverse(quantumComputation, expOpp.top(), expLhss.top(), expRhss.top());
        subFlag                           = false;
        popExp();
        return synthesisOfInversionOk;
    }

    bool LineAwareSynthesis::assignAdd(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, const unsigned op) {
        bool synthesisOfAssignmentOk = true;
        if (!expOpp.empty() && expOpp.top() == op) {
            synthesisOfAssignmentOk = increase(quantumComputation, rhs, expLhss.top()) && increase(quantumComputation, rhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = increase(quantumComputation, rhs, lhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse(quantumComputation);
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignSubtract(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, const unsigned op) {
        bool synthesisOfAssignmentOk = true;
        if (!expOpp.empty() && expOpp.top() == op) {
            synthesisOfAssignmentOk = decrease(quantumComputation, rhs, expLhss.top()) &&
                                      increase(quantumComputation, rhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = decrease(quantumComputation, rhs, lhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse(quantumComputation);
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignExor(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, const unsigned op) {
        bool synthesisOfAssignmentOk = true;
        if (!expOpp.empty() && expOpp.top() == op) {
            synthesisOfAssignmentOk = bitwiseCnot(quantumComputation, lhs, expLhss.top()) && bitwiseCnot(quantumComputation, lhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = bitwiseCnot(quantumComputation, lhs, rhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse(quantumComputation);
        }
        return synthesisOfAssignmentOk;
    }

    /// This function is used when input signals (rhs) are equal (just to solve statements individually)
    bool LineAwareSynthesis::expEvaluate(qc::QuantumComputation& quantumComputation, std::vector<qc::Qubit>& lines, unsigned op, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        bool synthesisOk = true;
        switch (op) {
            case BinaryExpression::Add: // +
                synthesisOk = increase(quantumComputation, rhs, lhs);
                lines       = rhs;
                break;
            case BinaryExpression::Subtract: // -
                if (subFlag) {
                    synthesisOk = decreaseNewAssign(quantumComputation, rhs, lhs);
                    lines       = rhs;
                } else {
                    synthesisOk = decrease(quantumComputation, rhs, lhs);
                    lines       = rhs;
                }
                break;
            case BinaryExpression::Exor:                                 // ^
                synthesisOk = bitwiseCnot(quantumComputation, rhs, lhs); // duplicate lhs
                lines       = rhs;
                break;
            default:
                return true;
        }
        return synthesisOk;
    }

    bool LineAwareSynthesis::decreaseNewAssign(qc::QuantumComputation& quantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (const auto lh: lhs) {
            createAndAddNotGate(quantumComputation, lh);
        }
        if (!increase(quantumComputation, rhs, lhs)) {
            return false;
        }
        for (const auto lh: lhs) {
            createAndAddNotGate(quantumComputation, lh);
        }
        for (const auto rh: rhs) {
            createAndAddNotGate(quantumComputation, rh);
        }
        return true;
    }

    bool LineAwareSynthesis::expressionSingleOp(qc::QuantumComputation& quantumComputation, const unsigned op, const std::vector<qc::Qubit>& expLhs, const std::vector<qc::Qubit>& expRhs) {
        // With the return value we only propagate an error if the defined 'synthesis' operation for any of the handled operations fails. In all other cases, we assume that
        // no synthesis should be performed and simply return OK.
        switch (op) {
            case BinaryExpression::Add: // +
                return increase(quantumComputation, expRhs, expLhs);
            case BinaryExpression::Subtract: // -
                return subFlag ? decreaseNewAssign(quantumComputation, expRhs, expLhs) : decrease(quantumComputation, expRhs, expLhs);
            case BinaryExpression::Exor: // ^
                return bitwiseCnot(quantumComputation, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::expressionOpInverse(qc::QuantumComputation& quantumComputation, const unsigned op, const std::vector<qc::Qubit>& expLhs, const std::vector<qc::Qubit>& expRhs) {
        // With the return value we only propagate an error if the defined 'synthesis' operation for any of the handled operations fails. In all other cases, we assume that
        // no synthesis should be performed and simply return OK.
        switch (op) {
            case BinaryExpression::Add: // +
                return decrease(quantumComputation, expRhs, expLhs);
            case BinaryExpression::Subtract: // -
                return decreaseNewAssign(quantumComputation, expRhs, expLhs);
            case BinaryExpression::Exor: // ^
                return bitwiseCnot(quantumComputation, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::synthesize(qc::QuantumComputation& quantumComputation, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        LineAwareSynthesis synthesizer(quantumComputation);
        return SyrecSynthesis::synthesize(&synthesizer, quantumComputation, program, settings, statistics);
    }
} // namespace syrec
