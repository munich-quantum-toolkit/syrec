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

#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <memory>
#include <string>
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
       * This methods tries to find a parameter or a variable
       * by its name. If no such parameter or variable exists,
       * then the empty smart pointer variable::ptr() is returned.
       * Otherwise, using the \ref variable::type() "type" it can
       * be determined, whether it is a parameter of a variable.
       */
        [[nodiscard]] Variable::ptr findParameterOrVariable(const std::string& n) const {
            for (Variable::ptr var: parameters) {
                if (var->name == n) {
                    return var;
                }
            }

            return {};
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
