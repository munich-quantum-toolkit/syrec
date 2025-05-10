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
#include "core/circuit.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace syrec {
    bool LineAwareSynthesis::processStatement(Circuit& circuit, const Statement::ptr& statement) {
        const auto* const stmtCastedAsAssignmentStmt = dynamic_cast<const AssignStatement*>(statement.get());
        if (stmtCastedAsAssignmentStmt == nullptr) {
            return SyrecSynthesis::onStatement(circuit, statement);
        }

        const AssignStatement& assignmentStmt = *stmtCastedAsAssignmentStmt;
        std::vector<unsigned>  d;
        std::vector<unsigned>  dd;
        std::vector<unsigned>  ddd;
        std::vector<unsigned>  statLhs;
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
            return SyrecSynthesis::onStatement(circuit, statement);
        }

        // To be able to associate which gates are associated with a statement in the syrec-editor we need to set the appropriate annotation that will be added for each created gate
        circuit.setOrUpdateGlobalGateAnnotation(Circuit::GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

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
                    synthesisOk = expressionSingleOp(circuit, 1, expLhsVector.at(0), statLhs) &&
                                  expressionSingleOp(circuit, 1, expRhsVector.at(0), statLhs);
                } else {
                    synthesisOk = expressionSingleOp(circuit, assignmentStmt.op, expLhsVector.at(0), statLhs) &&
                                  expressionSingleOp(circuit, expOpVector.at(0), expRhsVector.at(0), statLhs);
                }
                expOpVector.clear();
                assignOpVector.clear();
                expLhsVector.clear();
                expRhsVector.clear();
                opVec.clear();
            }
            return synthesisOk;
        }

        std::vector<unsigned> lines;
        if (expLhsVector.at(0) == expRhsVector.at(0)) {
            if (expOpVector.at(0) == 1 || expOpVector.at(0) == 2) {
                /// cancel out the signals
            } else if (expOpVector.at(0) != 1 || expOpVector.at(0) != 2) {
                synthesisOk = expressionSingleOp(circuit, assignmentStmt.op, expLhsVector.at(0), statLhs) &&
                              expressionSingleOp(circuit, expOpVector.at(0), expRhsVector.at(0), statLhs);
            }
        } else {
            synthesisOk = solver(circuit, statLhs, assignmentStmt.op, expLhsVector.at(0), expOpVector.at(0), expRhsVector.at(0));
        }

        const std::size_t     z = (expOpVector.size() - static_cast<std::size_t>(expOpVector.size() % 2 == 0)) / 2;
        std::vector<unsigned> statAssignOp(z == 0 ? 1 : z, 0);

        for (std::size_t k = 0; k <= z - 1; k++) {
            statAssignOp[k] = assignOpVector[k];
        }

        /// Assignment operations
        std::reverse(statAssignOp.begin(), statAssignOp.end());

        /// If reversible assignment is "-", the assignment operations must negated appropriately
        if (assignmentStmt.op == 1) {
            for (unsigned int& i: statAssignOp) {
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
                            synthesisOk = expressionSingleOp(circuit, 1, expLhsVector.at(i), statLhs) &&
                                          expressionSingleOp(circuit, 1, expRhsVector.at(i), statLhs);
                            j++;
                        } else {
                            synthesisOk = expressionSingleOp(circuit, statAssignOp.at(j), expLhsVector.at(i), statLhs) &&
                                          expressionSingleOp(circuit, expOpVector.at(i), expRhsVector.at(i), statLhs);
                            j++;
                        }
                    }
                } else {
                    synthesisOk = solver(circuit, statLhs, statAssignOp.at(j), expLhsVector.at(i), expOpVector.at(i), expRhsVector.at(i));
                    j++;
                }
            }
            /// when only lhs exists o rhs exists
            else if (((expLhsVector.at(i).empty()) && !(expRhsVector.at(i).empty())) || ((!expLhsVector.at(i).empty()) && (expRhsVector.at(i).empty()))) {
                synthesisOk = expEvaluate(circuit, lines, statAssignOp.at(j), expRhsVector.at(i), statLhs);
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

    bool LineAwareSynthesis::flow(const Expression::ptr& expression, std::vector<unsigned>& v) {
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return (binary->op == BinaryExpression::Add || binary->op == BinaryExpression::Subtract || binary->op == BinaryExpression::Exor) && flow(*binary, v);
        }
        if (auto const* var = dynamic_cast<VariableExpression*>(expression.get())) {
            return flow(*var, v);
        }
        return false;
    }

    bool LineAwareSynthesis::flow(const VariableExpression& expression, std::vector<unsigned>& v) {
        getVariables(expression.var, v);
        return true;
    }

    /// generating LHS and RHS (can be whole expressions as well)
    bool LineAwareSynthesis::flow(const BinaryExpression& expression, const std::vector<unsigned>& v [[maybe_unused]]) {
        std::vector<unsigned> lhs;
        std::vector<unsigned> rhs;
        assignOpVector.push_back(expression.op);

        if (!flow(expression.lhs, lhs) || !flow(expression.rhs, rhs)) {
            return false;
        }

        expLhsVector.push_back(lhs);
        expRhsVector.push_back(rhs);
        expOpVector.push_back(expression.op);
        return true;
    }

    bool LineAwareSynthesis::solver(Circuit& circuit, const std::vector<unsigned>& expRhs, unsigned statOp, const std::vector<unsigned>& expLhs, unsigned expOp, const std::vector<unsigned>& statLhs) {
        bool synthesisOk = true;
        if (statOp == expOp) {
            if (expOp == 1) {
                synthesisOk = expressionSingleOp(circuit, 1, expLhs, expRhs) &&
                              expressionSingleOp(circuit, 0, statLhs, expRhs);
            } else {
                synthesisOk = expressionSingleOp(circuit, statOp, expLhs, expRhs) &&
                              expressionSingleOp(circuit, statOp, statLhs, expRhs);
            }
        } else {
            std::vector<unsigned> lines;
            subFlag     = true;
            synthesisOk = expEvaluate(circuit, lines, expOp, expLhs, statLhs);
            subFlag     = false;
            synthesisOk &= expEvaluate(circuit, lines, statOp, lines, expRhs);
            subFlag = true;
            if (expOp < 3) {
                synthesisOk &= expressionOpInverse(circuit, expOp, expLhs, statLhs);
            }
        }
        subFlag = false;
        return synthesisOk;
    }

    bool LineAwareSynthesis::opRhsLhsExpression(const Expression::ptr& expression, std::vector<unsigned>& v) {
        if (auto const* binary = dynamic_cast<BinaryExpression*>(expression.get())) {
            return opRhsLhsExpression(*binary, v);
        }
        if (auto const* var = dynamic_cast<VariableExpression*>(expression.get())) {
            return opRhsLhsExpression(*var, v);
        }
        return false;
    }

    bool LineAwareSynthesis::opRhsLhsExpression(const VariableExpression& expression, std::vector<unsigned>& v) {
        getVariables(expression.var, v);
        return true;
    }

    bool LineAwareSynthesis::opRhsLhsExpression(const BinaryExpression& expression, std::vector<unsigned>& v) {
        std::vector<unsigned> lhs;
        std::vector<unsigned> rhs;

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

    bool LineAwareSynthesis::inverse(Circuit& circuit) {
        const bool synthesisOfInversionOk = expressionOpInverse(circuit, expOpp.top(), expLhss.top(), expRhss.top());
        subFlag                           = false;
        popExp();
        return synthesisOfInversionOk;
    }

    bool LineAwareSynthesis::assignAdd(Circuit& circuit, std::vector<unsigned>& rhs, std::vector<unsigned>& lhs, const unsigned& op) {
        bool synthesisOfAssignmentOk = true;
        if (!expOpp.empty() && expOpp.top() == op) {
            synthesisOfAssignmentOk = increase(circuit, rhs, expLhss.top()) && increase(circuit, rhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = increase(circuit, rhs, lhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse(circuit);
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignSubtract(Circuit& circuit, std::vector<unsigned>& rhs, std::vector<unsigned>& lhs, const unsigned& op) {
        bool synthesisOfAssignmentOk = true;
        if (!expOpp.empty() && expOpp.top() == op) {
            synthesisOfAssignmentOk = decrease(circuit, rhs, expLhss.top()) &&
                                      increase(circuit, rhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = decrease(circuit, rhs, lhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse(circuit);
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignExor(Circuit& circuit, std::vector<unsigned>& lhs, std::vector<unsigned>& rhs, const unsigned& op) {
        bool synthesisOfAssignmentOk = true;
        if (!expOpp.empty() && expOpp.top() == op) {
            synthesisOfAssignmentOk = bitwiseCnot(circuit, lhs, expLhss.top()) && bitwiseCnot(circuit, lhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = bitwiseCnot(circuit, lhs, rhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse(circuit);
        }
        return synthesisOfAssignmentOk;
    }

    /// This function is used when input signals (rhs) are equal (just to solve statements individually)
    bool LineAwareSynthesis::expEvaluate(Circuit& circuit, std::vector<unsigned>& lines, unsigned op, const std::vector<unsigned>& lhs, const std::vector<unsigned>& rhs) const {
        bool synthesisOk = true;
        switch (op) {
            case BinaryExpression::Add: // +
                synthesisOk = increase(circuit, rhs, lhs);
                lines       = rhs;
                break;
            case BinaryExpression::Subtract: // -
                if (subFlag) {
                    synthesisOk = decreaseNewAssign(circuit, rhs, lhs);
                    lines       = rhs;
                } else {
                    synthesisOk = decrease(circuit, rhs, lhs);
                    lines       = rhs;
                }
                break;
            case BinaryExpression::Exor:                      // ^
                synthesisOk = bitwiseCnot(circuit, rhs, lhs); // duplicate lhs
                lines       = rhs;
                break;
            default:
                return true;
        }
        return synthesisOk;
    }

    bool LineAwareSynthesis::decreaseNewAssign(Circuit& circuit, const std::vector<unsigned>& rhs, const std::vector<unsigned>& lhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (const auto lh: lhs) {
            circuit.createAndAddNotGate(lh);
        }
        if (!increase(circuit, rhs, lhs)) {
            return false;
        }
        for (const auto lh: lhs) {
            circuit.createAndAddNotGate(lh);
        }
        for (const auto rh: rhs) {
            circuit.createAndAddNotGate(rh);
        }
        return true;
    }

    bool LineAwareSynthesis::expressionSingleOp(Circuit& circuit, const unsigned op, const std::vector<unsigned>& expLhs, const std::vector<unsigned>& expRhs) const {
        // With the return value we only propagate an error if the defined 'synthesis' operation for any of the handled operations fails. In all other cases, we assume that
        // no synthesis should be performed and simply return OK.
        switch (op) {
            case BinaryExpression::Add: // +
                return increase(circuit, expRhs, expLhs);
            case BinaryExpression::Subtract: // -
                return subFlag ? decreaseNewAssign(circuit, expRhs, expLhs) : decrease(circuit, expRhs, expLhs);
            case BinaryExpression::Exor: // ^
                return bitwiseCnot(circuit, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::expressionOpInverse(Circuit& circuit, const unsigned op, const std::vector<unsigned>& expLhs, const std::vector<unsigned>& expRhs) const {
        // With the return value we only propagate an error if the defined 'synthesis' operation for any of the handled operations fails. In all other cases, we assume that
        // no synthesis should be performed and simply return OK.
        switch (op) {
            case BinaryExpression::Add: // +
                return decrease(circuit, expRhs, expLhs);
            case BinaryExpression::Subtract: // -
                return decreaseNewAssign(circuit, expRhs, expLhs);
            case BinaryExpression::Exor: // ^
                return bitwiseCnot(circuit, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::synthesize(Circuit& circ, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        LineAwareSynthesis synthesizer(circ);
        return SyrecSynthesis::synthesize(&synthesizer, circ, program, settings, statistics);
    }
} // namespace syrec
