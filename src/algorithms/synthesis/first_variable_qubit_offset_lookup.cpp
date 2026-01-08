/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/first_variable_qubit_offset_lookup.hpp"

#include "ir/Definitions.hpp"

#include <optional>
#include <string_view>

using namespace syrec;

void FirstVariableQubitOffsetLookup::openNewVariableQubitOffsetScope() {
    recordedOffsetsToFirstQubitPerVariableScopes.emplace_back();
}

bool FirstVariableQubitOffsetLookup::closeVariableQubitOffsetScope() {
    if (recordedOffsetsToFirstQubitPerVariableScopes.empty()) {
        return false;
    }
    recordedOffsetsToFirstQubitPerVariableScopes.pop_back();
    return true;
}

bool FirstVariableQubitOffsetLookup::registerOrUpdateOffsetToFirstQubitOfVariableInCurrentScope(const std::string_view& variableIdentifier, const qc::Qubit offsetToFirstQubitOfVariable) {
    if (variableIdentifier.empty() || recordedOffsetsToFirstQubitPerVariableScopes.empty()) {
        return false;
    }
    recordedOffsetsToFirstQubitPerVariableScopes.back()[variableIdentifier] = offsetToFirstQubitOfVariable;
    return true;
}

std::optional<qc::Qubit> FirstVariableQubitOffsetLookup::getOffsetToFirstQubitOfVariableInCurrentScope(const std::string_view& variableIdentifier) const {
    if (variableIdentifier.empty() || recordedOffsetsToFirstQubitPerVariableScopes.empty()) {
        return std::nullopt;
    }

    const QubitOffsetScope& lastOpenedQubitOffsetScope        = recordedOffsetsToFirstQubitPerVariableScopes.back();
    const auto&             registrationForVariableIdentifier = lastOpenedQubitOffsetScope.find(variableIdentifier);
    return registrationForVariableIdentifier != lastOpenedQubitOffsetScope.cend() ? std::make_optional(registrationForVariableIdentifier->second) : std::nullopt;
}
