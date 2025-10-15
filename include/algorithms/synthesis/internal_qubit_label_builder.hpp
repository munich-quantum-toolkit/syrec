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

#include <cstddef>
#include <string>
#include <string_view>

namespace syrec {
    class InternalQubitLabelBuilder {
    public:
        static constexpr std::string_view   INTERNAL_QUBIT_LABEL_PREFIX = "__q";
        [[maybe_unused]] static std::string buildNonAncillaryQubitLabel(const std::size_t currNumQuantumRegistersInQuantumComputation) {
            return std::string(INTERNAL_QUBIT_LABEL_PREFIX) + std::to_string(currNumQuantumRegistersInQuantumComputation);
        }

        [[maybe_unused]] static std::string buildAncillaryQubitLabel(const std::size_t currNumQuantumRegistersInQuantumComputation) {
            return buildNonAncillaryQubitLabel(currNumQuantumRegistersInQuantumComputation) + "_anc";
        }
    };
} // namespace syrec
