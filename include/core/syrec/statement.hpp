/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/variable.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace syrec {

    struct Module;

    /**
     * @brief Abstract base class for all SyReC statements
     *
     * All statement classes are derived from this abstract class.
     * Each class has to implement the print() method. Otherwise,
     * the different classes are solely used for distinction.
     */
    struct Statement {
        /**
       * @brief Smart pointer
       */
        using ptr = std::shared_ptr<Statement>;

        /**
       * @brief Vector of smart pointers
       */
        using vec = std::vector<ptr>;

        /**
       * @brief Standard constructor
       *
       * Initializes default values
       */
        Statement() = default;

        /**
       * @brief Deconstructor
       */
        virtual ~Statement() = default;

        unsigned lineNumber = 0U;

        [[maybe_unused]] virtual std::optional<ptr> reverse() = 0;
    };

    struct SkipStatement: Statement {
        [[maybe_unused]] std::optional<ptr> reverse() override {
            return std::make_shared<SkipStatement>();
        }
    };

    /**
     * @brief SWAP Statement
     *
     * This class represents the SyReC SWAP Statement (<=>)
     * between two variables lhs() and rhs().
     */
    struct SwapStatement: Statement {
        /**
       * @brief Constructor
       *
       * @param lhs Variable access on left hand side
       * @param rhs Variable access on right hand side
       */
        SwapStatement(VariableAccess::ptr lhs,
                      VariableAccess::ptr rhs):
            lhs(std::move(lhs)),
            rhs(std::move(rhs)) {}

        [[maybe_unused]] std::optional<ptr> reverse() override {
            return std::make_shared<SwapStatement>(lhs, rhs);
        }

        VariableAccess::ptr lhs{};
        VariableAccess::ptr rhs{};
    };

    /**
     * @brief Unary Statement
     *
     * This class represents the SyReC Unary statements (++, --, ~)
     * on the variable access var().
     */
    struct UnaryStatement: Statement {
        /**
       * @brief Type of the statement
       */
        enum class UnaryOperation : std::uint8_t {
            /**
         * @brief Inversion of the variable
         */
            Invert,

            /**
         * @brief Increment of the variable by 1
         */
            Increment,

            /**
         * @brief Decrement of the variable by 1
         */
            Decrement
        };

        /**
       * @brief Constructor
       *
       * @param unaryOperation Operation
       * @param var Variable access to be transformed
       */
        UnaryStatement(const UnaryOperation unaryOperation,
                       VariableAccess::ptr  var):
            unaryOperation(unaryOperation),
            var(std::move(var)) {}

        [[maybe_unused]] std::optional<ptr> reverse() override {
            auto invertedOperation = UnaryOperation::Increment;
            switch (unaryOperation) {
                case UnaryOperation::Increment: // NOLINT(bugprone-branch-clone)
                    invertedOperation = UnaryOperation::Decrement;
                    break;
                case UnaryOperation::Decrement:
                    invertedOperation = UnaryOperation::Increment;
                    break;
                case UnaryOperation::Invert:
                    invertedOperation = UnaryOperation::Invert;
                    break;
                default:
                    return std::nullopt;
            }
            return std::make_shared<UnaryStatement>(invertedOperation, var);
        }

        UnaryOperation      unaryOperation;
        VariableAccess::ptr var{};
    };

    /**
     * @brief Assignment Statement
     *
     * This class represents the SyReC assignment statements (+=, -=, ^=)
     * of the expression rhs() to the variable access lhs().
     */
    struct AssignStatement: Statement {
        /**
       * @brief Type of assignment
       */
        enum class AssignOperation : std::uint8_t {
            /**
         * @brief Addition to itself
         */
            Add,

            /**
         * @brief Subtraction from itself
         */
            Subtract,

            /**
         * @brief Reflexive EXOR operation
         */
            Exor
        };

        /**
       * @brief Constructor
       *
       * @param lhs Variable access to which the operation is applied
       * @param assignOperation Operation to be applied
       * @param rhs Expression to be evaluated
       */
        AssignStatement(VariableAccess::ptr   lhs,
                        const AssignOperation assignOperation,
                        Expression::ptr       rhs):
            lhs(std::move(lhs)),
            assignOperation(assignOperation), rhs(std::move(rhs)) {}

        [[maybe_unused]] std::optional<ptr> reverse() override {
            auto invertedAssignOperation = AssignOperation::Add;
            switch (assignOperation) {
                case AssignOperation::Add: // NOLINT(bugprone-branch-clone)
                    invertedAssignOperation = AssignOperation::Subtract;
                    break;
                case AssignOperation::Subtract:
                    invertedAssignOperation = AssignOperation::Add;
                    break;
                case AssignOperation::Exor:
                    invertedAssignOperation = AssignOperation::Exor;
                    break;
                default:
                    return std::nullopt;
            }
            return std::make_shared<AssignStatement>(lhs, invertedAssignOperation, rhs);
        }

        VariableAccess::ptr lhs{};
        AssignOperation     assignOperation;
        Expression::ptr     rhs{};
    };

    /**
     * @brief IF Statement
     *
     * This class represents the SyReC \b if statement
     */
    struct IfStatement: Statement {
        /**
       * @brief Standard constructor
       *
       * Initializes default values
       */
        IfStatement() = default;

        /**
       * @brief Sets the condition for the execution of the then_statements()
       *
       * The expression \p condition is assumed to have a bit-width of 1 bit.
       *
       * @param cond Expression
       */
        void setCondition(Expression::ptr cond) {
            condition = std::move(cond);
        }

        /**
       * @brief Adds a statement to the then branch
       *
       * @param thenStatement Statement to be executed in the if branch
       */
        void addThenStatement(const ptr& thenStatement) {
            thenStatements.emplace_back(thenStatement);
        }

        /**
       * @brief Adds a statement to the else branch
       *
       * @param elseStatement Statement to be executed in the else branch
       */
        void addElseStatement(const ptr& elseStatement) {
            elseStatements.emplace_back(elseStatement);
        }

        /**
       * @brief Sets the reverse condition for the execution of the if_statements()
       *
       * The expression \p fi_condition is assumed to have a bit-width of 1 bit.
       * The reverse condition is checked in order the if statement is uncalled,
       * i.e. executed reversed. Usually it is the same has the condition(), unless
       * the evaluation of the condition does not change in one of the branches.
       *
       * @param fiCond Expression
       */
        void setFiCondition(Expression::ptr fiCond) {
            fiCondition = std::move(fiCond);
        }

        [[maybe_unused]] std::optional<ptr> reverse() override;

        Expression::ptr condition{};
        vec             thenStatements{};
        vec             elseStatements{};
        Expression::ptr fiCondition{};
    };

    /**
     * @brief FOR Statement
     *
     * This class represents the SyReC \b for statement
     */
    struct ForStatement: Statement {
        /**
       * @brief Standard constructor
       *
       * Initializes default values
       */
        ForStatement() = default;

        /**
       * @brief Adds a statement to be executed in the loop
       *
       * @param statement Statement
       */
        void addStatement(const ptr& statement) {
            statements.emplace_back(statement);
        }

        [[maybe_unused]] std::optional<ptr> reverse() override;

        std::string                         loopVariable;
        std::pair<Number::ptr, Number::ptr> range{};
        Number::ptr                         step{};
        vec                                 statements{};
    };

    /**
     * @brief CALL Statement
     *
     * This class represents the SyReC \b call statement to call a module.
     */
    struct CallStatement: Statement {
        /**
       * @brief Constructor with module and parameters
       *
       * @param target Module to call
       * @param parameters Parameters to assign
       */
        CallStatement(std::shared_ptr<Module> target, std::vector<std::string> parameters):
            target(std::move(target)), parameters(std::move(parameters)) {}

        [[maybe_unused]] std::optional<ptr> reverse() override;

        std::shared_ptr<Module>  target{};
        std::vector<std::string> parameters{};
    };

    /**
     * @brief UNCALL Statement
     *
     * This class represents the SyReC \b uncall statement to uncall a module.
     */
    struct UncallStatement: Statement {
        /**
       * @brief Constructor with module and parameters
       *
       * @param target Module to uncall
       * @param parameters Parameters to assign
       */
        UncallStatement(std::shared_ptr<Module> target, std::vector<std::string> parameters):
            target(std::move(target)), parameters(std::move(parameters)) {}

        [[maybe_unused]] std::optional<ptr> reverse() override;

        std::shared_ptr<Module>  target{};
        std::vector<std::string> parameters{};
    };

    [[nodiscard]] inline bool invertStatementBlock(const std::vector<Statement::ptr>& statementsToInvert, std::vector<Statement::ptr>& resultContainer) {
        resultContainer.resize(statementsToInvert.size(), nullptr);

        bool        inversionSuccessful = true;
        std::size_t resultContainerIdx  = 0;
        for (auto it = statementsToInvert.crbegin(); it != statementsToInvert.crend() && inversionSuccessful; ++it) {
            const std::optional<Statement::ptr> reversedStmt = *it != nullptr ? it->get()->reverse() : std::nullopt;
            resultContainer[resultContainerIdx++]            = reversedStmt.value_or(nullptr);
            inversionSuccessful                              = reversedStmt.has_value();
        }
        return inversionSuccessful;
    }

    inline std::optional<Statement::ptr> CallStatement::reverse() {
        return std::make_shared<UncallStatement>(target, parameters);
    }

    inline std::optional<Statement::ptr> UncallStatement::reverse() {
        return std::make_shared<CallStatement>(target, parameters);
    }

    inline std::optional<Statement::ptr> IfStatement::reverse() {
        auto fiStmt = std::make_shared<IfStatement>();
        fiStmt->setCondition(fiCondition);
        fiStmt->setFiCondition(condition);
        return invertStatementBlock(thenStatements, fiStmt->thenStatements) && invertStatementBlock(elseStatements, fiStmt->elseStatements) ? std::make_optional(fiStmt) : std::nullopt;
    }

    inline std::optional<Statement::ptr> ForStatement::reverse() {
        auto invertedForStmt          = std::make_shared<ForStatement>();
        invertedForStmt->loopVariable = loopVariable;
        invertedForStmt->range        = std::make_pair(range.second, range.first);
        invertedForStmt->step         = step;
        return invertStatementBlock(statements, invertedForStmt->statements) ? std::make_optional(invertedForStmt) : std::nullopt;
    }
} // namespace syrec
