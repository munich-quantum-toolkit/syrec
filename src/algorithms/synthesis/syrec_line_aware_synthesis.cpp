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
#include "core/configurable_options.hpp"
#include "core/statistics.hpp"
#include "core/syrec/expression.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"
#include "ir/Definitions.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace syrec {
    bool LineAwareSynthesis::processStatement(const Statement::ptr& statement) {
        if (statement == nullptr) {
            return false;
        }

        // TODO: At the time (07.09.2025) that this comment was written, bugs in the line aware synthesis algorithm existed (see issue #280) that might not only changes of the public/internal line aware synthesis interface but also its implementation.
        // Additionally, since a variable access that uses non-compile time constant expressions (CTCE) in the dimension access requires special handling when used on the left-hand side of an assignment the required
        // changes to fix the existing bugs in the synthesis algorithm as well as to add support for the "special" variable accesses should be combined in a future rework. For now we do not support the synthesis of statements
        // that contain a variable access using a non-CTCE as index in its dimension access.
        std::optional    didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = true;
        const Statement* stmtReference                                                               = statement.get();
        if (const auto* const stmtCastedAsUnaryStmt = dynamic_cast<const UnaryStatement*>(stmtReference); stmtCastedAsUnaryStmt != nullptr) {
            didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = doesVariableAccessNotContainCompileTimeconstantExpressions(stmtCastedAsUnaryStmt->var);
        } else if (const auto* const stmtCastedAsIfStmt = dynamic_cast<const IfStatement*>(stmtReference); stmtCastedAsIfStmt != nullptr) {
            didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(stmtCastedAsIfStmt->condition);
        } else if (const auto* const stmtCastedAsSwapStmt = dynamic_cast<const SwapStatement*>(stmtReference); stmtCastedAsSwapStmt != nullptr) {
            didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = doesVariableAccessNotContainCompileTimeconstantExpressions(stmtCastedAsSwapStmt->lhs);
            if (didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex.has_value() && !*didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex) {
                didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = doesVariableAccessNotContainCompileTimeconstantExpressions(stmtCastedAsSwapStmt->rhs);
            }
        }

        const auto* const stmtCastedAsAssignmentStmt = dynamic_cast<const AssignStatement*>(statement.get());
        if (stmtCastedAsAssignmentStmt != nullptr) {
            didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = doesVariableAccessNotContainCompileTimeconstantExpressions(stmtCastedAsAssignmentStmt->lhs);
            if (didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex.has_value() && *didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex) {
                didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex = doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(stmtCastedAsAssignmentStmt->rhs);
            }
        }

        if (didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex.has_value() && !*didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex) {
            std::cerr << "Line aware synthesis cannot synthesis a statement that contains a variable access that uses a non-compile time constant expression as index in its dimension access component\n";
            return false;
        }
        // If we cannot determine whether the statement did not contain a variable access that used a non-compile time constant expression then either an error during the validation of an variable access used in the statement occurred
        // or no checks for the given statement type were defined which in turn means that the line aware synthesis can probably not handle the statement type and we delegate the synthesis to the base class.
        if (!didStmtNotContainVariableAccessUsingNonCompileTimeConstantExpressionAsIndex.has_value() || stmtCastedAsAssignmentStmt == nullptr) {
            return SyrecSynthesis::onStatement(statement);
        }

        const AssignStatement& assignmentStmt = *stmtCastedAsAssignmentStmt;
        std::vector<qc::Qubit> d;
        std::vector<qc::Qubit> dd;
        std::vector<qc::Qubit> ddd;
        std::vector<qc::Qubit> statLhs;

        // The line aware synthesis of an assignment can only be performed when the rhs input signals are repeated (since the results are stored in the rhs)
        // and the right-hand side expression of the assignment consists of only Variable- or BinaryExpressions with the latter only containing the operations (+, - or ^).
        const bool canAssignmentSynthesisBeOptimized = getVariables(assignmentStmt.lhs, statLhs) && opRhsLhsExpression(assignmentStmt.rhs, d) && !opVec.empty() && flow(assignmentStmt.rhs, ddd) && checkRepeats() && flow(assignmentStmt.rhs, dd);
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
        std::ranges::reverse(statAssignOp);

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
        return getVariables(expression.var, v);
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
        return getVariables(expression.var, v);
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

    bool LineAwareSynthesis::assignAdd(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, const AssignStatement::AssignOperation assignOperation) {
        bool synthesisOfAssignmentOk = true;
        if (const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = !expOpp.empty() ? tryMapAssignmentToBinaryOperation(assignOperation) : std::nullopt;
            mappedToBinaryOperation.has_value() && *mappedToBinaryOperation == expOpp.top()) {
            synthesisOfAssignmentOk = inplaceAdd(annotatableQuantumComputation, expLhss.top(), lhs) && inplaceAdd(annotatableQuantumComputation, expRhss.top(), lhs);
            popExp();
        } else {
            // The assignment lhs += rhs is synthesized using the inplace addition which stores the result of the addition in the qubits passed as the right hand side operand thus the operands of the assignment need to be passed in the reverse order.
            synthesisOfAssignmentOk = inplaceAdd(annotatableQuantumComputation, rhs, lhs); // NOLINT(readability-suspicious-call-argument)
        }

        while (!expOpp.empty() && synthesisOfAssignmentOk) {
            synthesisOfAssignmentOk = inverse();
        }
        return synthesisOfAssignmentOk;
    }

    bool LineAwareSynthesis::assignSubtract(std::vector<qc::Qubit>& lhs, std::vector<qc::Qubit>& rhs, const AssignStatement::AssignOperation assignOperation) {
        bool synthesisOfAssignmentOk = true;
        if (const std::optional<BinaryExpression::BinaryOperation> mappedToBinaryOperation = !expOpp.empty() ? tryMapAssignmentToBinaryOperation(assignOperation) : std::nullopt;
            mappedToBinaryOperation.has_value() && *mappedToBinaryOperation == expOpp.top()) {
            synthesisOfAssignmentOk = inplaceSubtract(annotatableQuantumComputation, expLhss.top(), lhs) &&
                                      inplaceAdd(annotatableQuantumComputation, expRhss.top(), lhs);
            popExp();
        } else {
            // The assignment lhs -= rhs is synthesized using the inplace subtraction which stores the result of the subtraction in the qubits passed as the right hand side operand thus the operands of the assignment need to be passed in the reverse order.
            synthesisOfAssignmentOk = inplaceSubtract(annotatableQuantumComputation, rhs, lhs); // NOLINT(readability-suspicious-call-argument)
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
                synthesisOk = inplaceAdd(annotatableQuantumComputation, lhs, rhs);
                lines       = rhs;
                break;
            case BinaryExpression::BinaryOperation::Subtract: // -
                if (subFlag) {
                    synthesisOk = decreaseNewAssign(annotatableQuantumComputation, lhs, rhs);
                    lines       = rhs;
                } else {
                    synthesisOk = inplaceSubtract(annotatableQuantumComputation, lhs, rhs);
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

    bool LineAwareSynthesis::decreaseNewAssign(AnnotatableQuantumComputation& annotatableQuantumComputation, const std::vector<qc::Qubit>& lhs, const std::vector<qc::Qubit>& rhs) {
        const std::size_t nQbitsOfOperation      = lhs.size();
        bool              synthesisOfOperationOk = lhs.size() == rhs.size();
        for (std::size_t i = 0; i < nQbitsOfOperation && synthesisOfOperationOk; ++i) {
            synthesisOfOperationOk &= annotatableQuantumComputation.addOperationsImplementingNotGate(lhs[i]);
        }
        synthesisOfOperationOk &= inplaceAdd(annotatableQuantumComputation, lhs, rhs);

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
                return inplaceAdd(annotatableQuantumComputation, expLhs, expRhs);
            case BinaryExpression::BinaryOperation::Subtract: // -
                return subFlag ? decreaseNewAssign(annotatableQuantumComputation, expLhs, expRhs) : inplaceSubtract(annotatableQuantumComputation, expLhs, expRhs);
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
                return inplaceSubtract(annotatableQuantumComputation, expLhs, expRhs);
            case BinaryExpression::BinaryOperation::Subtract: // -
                return decreaseNewAssign(annotatableQuantumComputation, expLhs, expRhs);
            case BinaryExpression::BinaryOperation::Exor: // ^
                return bitwiseCnot(annotatableQuantumComputation, expRhs, expLhs);
            default:
                return true;
        }
    }

    bool LineAwareSynthesis::synthesize(AnnotatableQuantumComputation& annotatableQuantumComputation, const Program& program, const ConfigurableOptions& settings, Statistics* optionalRecordedStatistics) {
        LineAwareSynthesis synthesizer(annotatableQuantumComputation);
        return SyrecSynthesis::synthesize(&synthesizer, program, settings, optionalRecordedStatistics);
    }

    std::optional<bool> LineAwareSynthesis::doesVariableAccessNotContainCompileTimeconstantExpressions(const VariableAccess::ptr& variableAccess) const {
        const std::optional<EvaluatedVariableAccess> evaluatedVariableAccess = evaluateAndValidateVariableAccess(variableAccess, loopMap, firstVariableQubitOffsetLookup);
        return evaluatedVariableAccess.has_value() ? std::make_optional(evaluatedVariableAccess->evaluatedDimensionAccess.containedOnlyNumericExpressions) : std::nullopt;
    }

    std::optional<bool> LineAwareSynthesis::doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(const Expression::ptr& expr) const {
        if (expr == nullptr) {
            return std::nullopt;
        }

        if (const auto& exprCastedAsBinaryOne = std::dynamic_pointer_cast<BinaryExpression>(expr); exprCastedAsBinaryOne != nullptr) {
            const std::optional<bool> doesLhsOperandNotContainVariableAccessWithCompileTimeConstantExpression = doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(exprCastedAsBinaryOne->lhs);
            const std::optional<bool> doesRhsOperandNotContainVariableAccessWithCompileTimeConstantExpression = doesLhsOperandNotContainVariableAccessWithCompileTimeConstantExpression.has_value() ? doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(exprCastedAsBinaryOne->rhs) : std::nullopt;
            return doesLhsOperandNotContainVariableAccessWithCompileTimeConstantExpression.has_value() && doesRhsOperandNotContainVariableAccessWithCompileTimeConstantExpression ? std::make_optional(*doesLhsOperandNotContainVariableAccessWithCompileTimeConstantExpression && *doesRhsOperandNotContainVariableAccessWithCompileTimeConstantExpression) : std::nullopt;
        }
        if (const auto& exprCastedAsUnaryOne = std::dynamic_pointer_cast<UnaryExpression>(expr); exprCastedAsUnaryOne != nullptr) {
            return doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(exprCastedAsUnaryOne->expr);
        }
        if (const auto& exprCastedAsShiftOne = std::dynamic_pointer_cast<ShiftExpression>(expr); exprCastedAsShiftOne != nullptr) {
            return doesExpressionNotContainVariableAccessWithCompileTimeConstantExpressions(exprCastedAsShiftOne->lhs);
        }
        if (const auto& exprCastedAsVariableOne = std::dynamic_pointer_cast<VariableExpression>(expr); exprCastedAsVariableOne != nullptr) {
            return doesVariableAccessNotContainCompileTimeconstantExpressions(exprCastedAsVariableOne->var);
        }
        if (const auto& exprAsNumericOne = std::dynamic_pointer_cast<NumericExpression>(expr); exprAsNumericOne != nullptr) {
            return true;
        }
        return false;
    }
} // namespace syrec
