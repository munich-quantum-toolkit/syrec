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

#include "core/syrec/variable.hpp"

namespace variable_assignability_check {
    [[nodiscard]] constexpr static bool doesModuleParameterTypeAllowAssignmentFromVariableType(syrec::Variable::Type assignedToVariableType, syrec::Variable::Type assignedFromVariableType) {
        switch (assignedToVariableType) {
            case syrec::Variable::Type::In:
                return true;
            case syrec::Variable::Type::Out:
            case syrec::Variable::Type::Inout:
                return assignedFromVariableType == syrec::Variable::Type::Out || assignedFromVariableType == syrec::Variable::Type::Inout || assignedFromVariableType == syrec::Variable::Type::Wire;
            default:
                return false;
        }
    }
} // namespace variable_assignability_check
