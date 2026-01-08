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

#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace syrec {

    /**
     * @brief SyReC module data type
     *
     * This class represents a SyReC module. It consists of a name(), parameters(),
     * local variables(), and a list of statements().
     */
    struct Module {
        /**
       * @brief Smart pointer
       */
        using ptr = std::shared_ptr<Module>;

        /**
       * @brief Vector of smart pointers
       */
        using vec = std::vector<ptr>;

        /**
       * @brief Constructor
       *
       * Initializes a module with a name
       *
       * @param name Name of the module
       */
        explicit Module(std::string name):
            name(std::move(name)) {}

        /**
       * @brief Adds a parameter to the module
       *
       * @param parameter Parameter
       */
        void addParameter(const Variable::ptr& parameter) {
            parameters.emplace_back(parameter);
        }

        /**
       * @brief Finds a parameter or variable in the module
       *
       * @param variableIdentifierToFind The identifier used to find a matching parameter or local variable of the module.
       * @returns The first matching parameter or local variable of the module, otherwise std::nullopt.
       * @remark The \ref variable::type() of the returned variable can be used to determine whether the variable is a parameter or local variable of the module.
       */
        [[nodiscard]] std::optional<Variable::ptr> findParameterOrVariable(const std::string_view& variableIdentifierToFind) const {
            for (const auto& parameter: parameters) {
                if (parameter->name == variableIdentifierToFind) {
                    return parameter;
                }
            }

            for (const auto& localVariable: variables) {
                if (localVariable->name == variableIdentifierToFind) {
                    return localVariable;
                }
            }
            return std::nullopt;
        }

        /**
       * @brief Adds a statement to the module
       *
       * @param statement Statement
       */
        void addStatement(const std::shared_ptr<Statement>& statement) {
            statements.emplace_back(statement);
        }

        std::string    name;
        Variable::vec  parameters;
        Variable::vec  variables;
        Statement::vec statements;
    };

} // namespace syrec
