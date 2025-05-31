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
#include "core/annotatable_quantum_computation.hpp"
#include "core/properties.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "ir/Definitions.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace syrec {
    bool LineAwareSynthesis::processStatement(const Statement::ptr& statement) {
        const auto* const stmtCastedAsAssignmentStmt = dynamic_cast<const AssignStatement*>(statement.get());
        if (stmtCastedAsAssignmentStmt == nullptr) {
            return SyrecSynthesis::onStatement(statement);
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
            return SyrecSynthesis::onStatement(statement);
        }

        // To be able to associate which gates are associated with a statement in the syrec-editor we need to set the appropriate annotation that will be added for each created gate
        annotatableQuantumComputation.setOrUpdateGlobalQuantumOperationAnnotation(GATE_ANNOTATION_KEY_ASSOCIATED_STATEMENT_LINE_NUMBER, std::to_string(static_cast<std::size_t>(statement->lineNumber)));

        // Binaryexpression ADD=0, MINUS=1, EXOR=2
        // AssignOperation ADD=0, MINUS=1, EXOR=2
        bool synthesisOk = true;
        if (expOpVector.size() == 1) {
            if (expOpVector.at(0) == BinaryExpression::BinaryOperation::Subtract || expOpVector.at(0) == BinaryExpression::BinaryOperation::Exor) {
                /// cancel out the signals
                expOpVector.clear();
                assignOpVector.clear();
                expLhsVector.clear();
                expRhsVector.clear();
                opVec.clear();
            } else {
                if (assignmentStmt.assignOperation == AssignStatement::AssignOperation::Subtract) {
                    synthesisOk = expressionSingleOp(BinaryExpression::BinaryOperation::Subtract, expLhsVector.at(0), statLhs) &&
                                  expressionSingleOp(BinaryExpression::BinaryOperation::Subtract, expRhsVector.at(0), statLhs);
                } else {
                    const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = tryMapAssignmentToBinaryOperation(assignmentStmt.assignOperation);
                    synthesisOk                                                                    = mappedToBinaryOperation.has_value() && expressionSingleOp(*mappedToBinaryOperation, expLhsVector.at(0), statLhs) &&
                                  expressionSingleOp(expOpVector.at(0), expRhsVector.at(0), statLhs);
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
            if (expOpVector.at(0) == BinaryExpression::BinaryOperation::Subtract || expOpVector.at(0) == BinaryExpression::BinaryOperation::Exor) {
                /// cancel out the signals
            } else if (expOpVector.at(0) != BinaryExpression::BinaryOperation::Subtract || expOpVector.at(0) != BinaryExpression::BinaryOperation::Exor) {
                const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = tryMapAssignmentToBinaryOperation(assignmentStmt.assignOperation);
                synthesisOk                                                                    = mappedToBinaryOperation.has_value() && expressionSingleOp(*mappedToBinaryOperation, expLhsVector.at(0), statLhs) &&
                              expressionSingleOp(expOpVector.at(0), expRhsVector.at(0), statLhs);
            }
        } else {
            synthesisOk = solver(statLhs, assignmentStmt.assignOperation, expLhsVector.at(0), expOpVector.at(0), expRhsVector.at(0));
        }

        const std::size_t z = (expOpVector.size() - static_cast<std::size_t>(expOpVector.size() % 2 == 0)) / 2;
        std::vector       statAssignOp(z == 0 ? 1 : z, AssignStatement::AssignOperation::Add);

        for (std::size_t k = 0; k <= z - 1; k++) {
            statAssignOp[k] = assignOpVector[k];
        }

        /// Assignment operations
        std::reverse(statAssignOp.begin(), statAssignOp.end());

        /// If reversible assignment is "-", the assignment operations must negated appropriately
        if (assignmentStmt.assignOperation == AssignStatement::AssignOperation::Subtract) {
            for (AssignStatement::AssignOperation& i: statAssignOp) {
                if (i == AssignStatement::AssignOperation::Add) {
                    i = AssignStatement::AssignOperation::Subtract;
                } else if (i == AssignStatement::AssignOperation::Subtract) {
                    i = AssignStatement::AssignOperation::Add;
                }
            }
        }

        std::size_t j = 0;
        for (std::size_t i = 1; i <= expOpVector.size() - 1 && synthesisOk; i++) {
            /// when both rhs and lhs exist
            if ((!expLhsVector.at(i).empty()) && (!expRhsVector.at(i).empty())) {
                if (expLhsVector.at(i) == expRhsVector.at(i)) {
                    if (expOpVector.at(i) == BinaryExpression::BinaryOperation::Subtract || expOpVector.at(i) == BinaryExpression::BinaryOperation::Exor) {
                        /// cancel out the signals
                        j++;
                    } else if (expOpVector.at(i) != BinaryExpression::BinaryOperation::Subtract || expOpVector.at(i) != BinaryExpression::BinaryOperation::Exor) {
                        if (statAssignOp.at(j) == AssignStatement::AssignOperation::Subtract) {
                            synthesisOk = expressionSingleOp(BinaryExpression::BinaryOperation::Subtract, expLhsVector.at(i), statLhs) &&
                                          expressionSingleOp(BinaryExpression::BinaryOperation::Subtract, expRhsVector.at(i), statLhs);
                            j++;
                        } else {
                            const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = tryMapAssignmentToBinaryOperation(statAssignOp.at(j));
                            synthesisOk                                                                    = mappedToBinaryOperation.has_value() && expressionSingleOp(*mappedToBinaryOperation, expLhsVector.at(i), statLhs) &&
                                          expressionSingleOp(expOpVector.at(i), expRhsVector.at(i), statLhs);
                            j++;
                        }
                    }
                } else {
                    synthesisOk = solver(statLhs, statAssignOp.at(j), expLhsVector.at(i), expOpVector.at(i), expRhsVector.at(i));
                    j++;
                }
            }
            /// when only lhs exists o rhs exists
            else if (((expLhsVector.at(i).empty()) && !(expRhsVector.at(i).empty())) || ((!expLhsVector.at(i).empty()) && (expRhsVector.at(i).empty()))) {
                const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = tryMapAssignmentToBinaryOperation(statAssignOp.at(j));
                synthesisOk                                                                    = mappedToBinaryOperation.has_value() && expEvaluate(lines, *mappedToBinaryOperation, expRhsVector.at(i), statLhs);
                j                                                                              = j + 1;
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
            return (binary->binaryOperation == BinaryExpression::BinaryOperation::Add || binary->binaryOperation == BinaryExpression::BinaryOperation::Subtract || binary->binaryOperation == BinaryExpression::BinaryOperation::Exor) && flow(*binary, v);
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

        if (const std::optional<AssignStatement::AssignOperation> mappedToAssignmentOperation = tryMapBinaryToAssignmentOperation(expression.binaryOperation); mappedToAssignmentOperation.has_value()) {
            assignOpVector.push_back(*mappedToAssignmentOperation);
        } else {
            return false;
        }

        if (!flow(expression.lhs, lhs) || !flow(expression.rhs, rhs)) {
            return false;
        }

        expLhsVector.push_back(lhs);
        expRhsVector.push_back(rhs);
        expOpVector.push_back(expression.binaryOperation);
        return true;
    }

    bool LineAwareSynthesis::solver(const std::vector<qc::Qubit>& expRhs, const AssignStatement::AssignOperation assignOperation, const std::vector<qc::Qubit>& expLhs, const BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& statLhs) {
        const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = tryMapAssignmentToBinaryOperation(assignOperation);
        if (!mappedToBinaryOperation.has_value()) {
            subFlag = false;
            return false;
        }

        bool synthesisOk = false;
        if (*mappedToBinaryOperation == binaryOperation) {
            if (binaryOperation == BinaryExpression::BinaryOperation::Subtract) {
                synthesisOk = expressionSingleOp(BinaryExpression::BinaryOperation::Subtract, expLhs, expRhs) &&
                              expressionSingleOp(BinaryExpression::BinaryOperation::Add, statLhs, expRhs);
            } else {
                synthesisOk = expressionSingleOp(*mappedToBinaryOperation, expLhs, expRhs) &&
                              expressionSingleOp(*mappedToBinaryOperation, statLhs, expRhs);
            }
        } else {
            std::vector<qc::Qubit> lines;
            subFlag     = true;
            synthesisOk = expEvaluate(lines, binaryOperation, expLhs, statLhs);
            subFlag     = false;
            synthesisOk &= expEvaluate(lines, *mappedToBinaryOperation, lines, expRhs);
            subFlag = true;
            switch (binaryOperation) {
                case BinaryExpression::BinaryOperation::Add:
                case BinaryExpression::BinaryOperation::Subtract:
                case BinaryExpression::BinaryOperation::Exor:
                    synthesisOk &= expressionOpInverse(binaryOperation, expLhs, statLhs);
                    break;
                default:
                    break;
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
        opVec.push_back(expression.binaryOperation);
        return true;
    }

    void LineAwareSynthesis::popExp() {
        expOpp.pop();
        expLhss.pop();
        expRhss.pop();
    }

    bool LineAwareSynthesis::inverse() {
        const bool synthesisOfInversionOk = expressionOpInverse(expOpp.top(), expLhss.top(), expRhss.top());
        subFlag                           = false;
        popExp();
        return synthesisOfInversionOk;
    }

    bool LineAwareSynthesis::assignAdd(std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, const AssignStatement::AssignOperation assignOperation) {
        bool synthesisOfAssignmentOk = true;
        if (const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = !expOpp.empty() ? tryMapAssignmentToBinaryOperation(assignOperation) : std::nullopt;
            mappedToBinaryOperation.has_value() && *mappedToBinaryOperation == expOpp.top()) {
            synthesisOfAssignmentOk = increase(annotatableQuantumComputation, rhs, expLhss.top()) && increase(annotatableQuantumComputation, rhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = increase(annotatableQuantumComputation, rhs, lhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse();
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignSubtract(std::vector<qc::Qubit>& rhs, std::vector<qc::Qubit>& lhs, const AssignStatement::AssignOperation assignOperation) {
        bool synthesisOfAssignmentOk = true;
        if (const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = !expOpp.empty() ? tryMapAssignmentToBinaryOperation(assignOperation) : std::nullopt;
            mappedToBinaryOperation.has_value() && *mappedToBinaryOperation == expOpp.top()) {
            synthesisOfAssignmentOk = decrease(annotatableQuantumComputation, rhs, expLhss.top()) &&
                                      increase(annotatableQuantumComputation, rhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = decrease(annotatableQuantumComputation, rhs, lhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse();
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignExor(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, const AssignStatement::AssignOperation assignOperation) {
        bool synthesisOfAssignmentOk = true;
        if (const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = !expOpp.empty() ? tryMapAssignmentToBinaryOperation(assignOperation) : std::nullopt;
            mappedToBinaryOperation.has_value() && *mappedToBinaryOperation == expOpp.top()) {
            synthesisOfAssignmentOk = bitwiseCnot(annotatableQuantumComputation, lhs, expLhss.top()) && bitwiseCnot(annotatableQuantumComputation, lhs, expRhss.top());
            popExp();
        } else {
            synthesisOfAssignmentOk = bitwiseCnot(annotatableQuantumComputation, lhs, rhs);
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse();
        }
        return synthesisOfAssignmentOk;
    }

    /// This function is used when input signals (rhs) are equal (just to solve statements individually)
    bool LineAwareSynthesis::expEvaluate(std::vector<qc::Qubit>& lines, const BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) const {
        bool synthesisOk = true;
        switch (binaryOperation) {
            case BinaryExpression::BinaryOperation::Add: // +
                synthesisOk = increase(annotatableQuantumComputation, rhs, lhs);
                lines       = rhs;
                break;
            case BinaryExpression::BinaryOperation::Subtract: // -
                if (subFlag) {
                    synthesisOk = decreaseNewAssign(annotatableQuantumComputation, rhs, lhs);
                    lines       = rhs;
                } else {
                    synthesisOk = decrease(annotatableQuantumComputation, rhs, lhs);
                    lines       = rhs;
                }
                break;
            case BinaryExpression::BinaryOperation::Exor:                           // ^
                synthesisOk = bitwiseCnot(annotatableQuantumComputation, rhs, lhs); // duplicate lhs
                lines       = rhs;
                break;
            default:
                break;
        }
        return synthesisOk;
    }

    bool LineAwareSynthesis::decreaseNewAssign(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& rhs, const std::vector<qc::Qubit>& lhs) {
        const std::size_t nQbitsOfOperation      = lhs.size();
        bool              synthesisOfOperationOk = lhs.size() == rhs.size();
        for (std::size_t i = 0; i < nQbitsOfOperation && synthesisOfOperationOk; ++i) {
            synthesisOfOperationOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(lhs[i]);
        }
        synthesisOfOperationOk &= increase(annotatableQuantumComputation, rhs, lhs);

        for (std::size_t i = 0; i < nQbitsOfOperation && synthesisOfOperationOk; ++i) {
            synthesisOfOperationOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(lhs[i]);
        }
        for (std::size_t i = 0; i < nQbitsOfOperation && synthesisOfOperationOk; ++i) {
            synthesisOfOperationOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(rhs[i]);
        }
        return synthesisOfOperationOk;
    }

    bool LineAwareSynthesis::expressionSingleOp(BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& expLhs, const std::vector<qc::Qubit>& expRhs) const {
        // With the return value we only propagate an error if the defined 'synthesis' operation for any of the handled operations fails. In all other cases, we assume that
        // no synthesis should be performed and simply return OK.
        switch (binaryOperation) {
            case BinaryExpression::BinaryOperation::Add: // +
                return increase(annotatableQuantumComputation, expRhs, expLhs);
            case BinaryExpression::BinaryOperation::Subtract: // -
                return subFlag ? decreaseNewAssign(annotatableQuantumComputation, expRhs, expLhs) : decrease(annotatableQuantumComputation, expRhs, expLhs);
            case BinaryExpression::BinaryOperation::Exor: // ^
                return bitwiseCnot(annotatableQuantumComputation, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::expressionOpInverse(BinaryExpression::BinaryOperation binaryOperation, const std::vector<qc::Qubit>& expLhs, const std::vector<qc::Qubit>& expRhs) {
        // With the return value we only propagate an error if the defined 'synthesis' operation for any of the handled operations fails. In all other cases, we assume that
        // no synthesis should be performed and simply return OK.
        switch (binaryOperation) {
            case BinaryExpression::BinaryOperation::Add: // +
                return decrease(annotatableQuantumComputation, expRhs, expLhs);
            case BinaryExpression::BinaryOperation::Subtract: // -
                return decreaseNewAssign(annotatableQuantumComputation, expRhs, expLhs);
            case BinaryExpression::BinaryOperation::Exor: // ^
                return bitwiseCnot(annotatableQuantumComputation, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::synthesize(AnnotatableQuantumComputation& annotatableQuantumComputation, const Program& program, const Properties::ptr& settings, const Properties::ptr& statistics) {
        LineAwareSynthesis synthesizer(annotatableQuantumComputation);
        return SyrecSynthesis::synthesize(&synthesizer, program, settings, statistics);
    }
} // namespace syrec
