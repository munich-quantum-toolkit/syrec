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

#include "ir/Definitions.hpp"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace syrec {
    /**
     * A lookup to register and manage the offset to the first qubit of a variable in a hierarchy of called/uncalled SyReC modules
     */
    class FirstVariableQubitOffsetLookup {
    public:
        /**
         * Open a new variable qubit offset scope.
         * @remark Registering a new variable qubit offset can only be done if an open scope exists in the internal lookup.
         */
        void openNewVariableQubitOffsetScope();

        /**
         * Close the last opened offset scope.
         * @return Whether a scope was closed.
         */
        [[maybe_unused]] bool closeVariableQubitOffsetScope();

        /**
         * Register or update the qubit offset for a variable in the last opened scope.
         * @param variableIdentifier The identifier of the variable for which an offset shall be recorded.
         * @param offsetToFirstQubitOfVariable The offset to record.
         * @return Whether an entry was added or updated in the last opened scope.
         */
        [[maybe_unused]] bool registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(const std::string_view& variableIdentifier, qc::Qubit offsetToFirstQubitOfVariable);

        /**
         * Fetch the last registered offset to the first qubit of a variable in the last opened scope.
         * @param variableIdentifier The identifier of the variable for which the registered offset shall be fetched.
         * @return The registered offset to the first qubit of the variable, otherwise std::nullopt.
         */
        [[nodiscard]] std::optional<qc::Qubit> getOffsetToFirstQubitOfVariableInCurrentScope(const std::string_view& variableIdentifier) const;

    protected:
        using QubitOffsetScope = std::unordered_map<std::string_view, qc::Qubit>;
        std::vector<QubitOffsetScope> recordedOffsetsToFirstQubitPerVariableScopes;
    };
} // namespace syrec
