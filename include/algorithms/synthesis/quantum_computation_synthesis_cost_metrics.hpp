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

#include "ir/QuantumComputation.hpp"
#include "ir/operations/OpType.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace syrec {
    using SynthesisCostMetricValue = std::uint64_t;

    [[nodiscard]] inline SynthesisCostMetricValue getQuantumCostForSynthesis(const qc::QuantumComputation& quantumComputation) {
        SynthesisCostMetricValue cost = 0;

        const auto numQubits = quantumComputation.getNqubits();
        if (numQubits == 0) {
            return cost;
        }

        for (const auto& quantumOperation: quantumComputation) {
            const std::size_t c             = std::min(quantumOperation->getNcontrols() + static_cast<std::size_t>(quantumOperation->getType() == qc::OpType::SWAP), numQubits - 1);
            const std::size_t numEmptyLines = numQubits - c - 1U;

            switch (c) {
                case 0U:
                case 1U:
                    cost += 1ULL;
                    break;
                case 2U:
                    cost += 5ULL;
                    break;
                case 3U:
                    cost += 13ULL;
                    break;
                case 4U:
                    cost += (numEmptyLines >= 2U) ? 26ULL : 29ULL;
                    break;
                case 5U:
                    if (numEmptyLines >= 3U) {
                        cost += 38ULL;
                    } else if (numEmptyLines >= 1U) {
                        cost += 52ULL;
                    } else {
                        cost += 61ULL;
                    }
                    break;
                case 6U:
                    if (numEmptyLines >= 4U) {
                        cost += 50ULL;
                    } else if (numEmptyLines >= 1U) {
                        cost += 80ULL;
                    } else {
                        cost += 125ULL;
                    }
                    break;
                case 7U:
                    if (numEmptyLines >= 5U) {
                        cost += 62ULL;
                    } else if (numEmptyLines >= 1U) {
                        cost += 100ULL;
                    } else {
                        cost += 253ULL;
                    }
                    break;
                default:
                    if (numEmptyLines >= c - 2U) {
                        cost += 12ULL * c - 22ULL;
                    } else if (numEmptyLines >= 1U) {
                        cost += 24ULL * c - 87ULL;
                    } else {
                        cost += (1ULL << (c + 1ULL)) - 3ULL;
                    }
            }
        }
        return cost;
    }

    [[nodiscard]] inline SynthesisCostMetricValue getTransistorCostForSynthesis(const qc::QuantumComputation& quantumComputation) {
        SynthesisCostMetricValue cost = 0;
        for (const auto& quantumOperation: quantumComputation) {
            cost += quantumOperation->getNcontrols() * 8;
        }
        return cost;
    }
} // namespace syrec