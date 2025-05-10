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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

namespace syrec {
    /**
   * @brief Represents a gate in a circuit
   */
    struct Gate {
        /**
        * @brief Represents a gate type
        */
        enum class Type : std::uint8_t { None,
                                         Fredkin,
                                         Toffoli };

        /**
        * @brief Type for accessing the line (line index)
        */
        using Line = std::size_t;

        /**
        * @brief Container for storing lines
        */
        using LinesLookup = std::set<Line>;
        using cost_t      = std::uint_least64_t;
        using ptr         = std::shared_ptr<Gate>;

        [[nodiscard]] cost_t quantumCost(unsigned lines) const {
            cost_t costs = 0U;

            const unsigned n = lines;
            std::size_t    c = controls.size();

            if (type == Gate::Type::Fredkin) {
                c += 1U;
            }

            c += static_cast<std::size_t>(std::max(-1 * static_cast<int>(c), 0));
            c = std::min(static_cast<unsigned>(c), lines - 1U);

            const unsigned e = n - static_cast<unsigned>(c) - 1U; // empty lines

            switch (c) {
                case 0U:
                case 1U:
                    costs = 1ULL;
                    break;
                case 2U:
                    costs = 5ULL;
                    break;
                case 3U:
                    costs = 13ULL;
                    break;
                case 4U:
                    costs = (e >= 2U) ? 26ULL : 29ULL;
                    break;
                case 5U:
                    if (e >= 3U) {
                        costs = 38ULL;
                    } else if (e >= 1U) {
                        costs = 52ULL;
                    } else {
                        costs = 61ULL;
                    }
                    break;
                case 6U:
                    if (e >= 4U) {
                        costs = 50ULL;
                    } else if (e >= 1U) {
                        costs = 80ULL;
                    } else {
                        costs = 125ULL;
                    }
                    break;
                case 7U:
                    if (e >= 5U) {
                        costs = 62ULL;
                    } else if (e >= 1U) {
                        costs = 100ULL;
                    } else {
                        costs = 253ULL;
                    }
                    break;
                default:
                    if (e >= c - 2U) {
                        costs = 12ULL * c - 22ULL;
                    } else if (e >= 1U) {
                        costs = 24ULL * c - 87ULL;
                    } else {
                        costs = (1ULL << (c + 1ULL)) - 3ULL;
                    }
            }

            return costs;
        }

        /**
         * @brief Returns the QASM representation of the gate.
         *
         * @note Multiple controls are not supported by QASM without corresponding definitions.
         * Since these definitions grow really large, we do not add them to the resulting QASM files.
         * Instead, multiple controls are merely indicated by prefixing the gate name with as many 'c' as there are controls.
         * The MQT can handle this notation.
         * This special handling will become obsolete once OpenQASM 3.0 is supported within the MQT.
         *
         * @return QASM string
         */
        [[nodiscard]] std::string toQasm() const {
            std::stringstream ss;
            ss << std::string(controls.size(), 'c');
            switch (type) {
                case Type::Fredkin:
                    ss << "swap";
                    break;
                case Type::Toffoli:
                    ss << "x";
                    break;
                // GCOVR_EXCL_START
                default:
                    throw std::runtime_error("Gate not supported");
                    // GCOVR_EXCL_STOP
            }
            for (const auto& control: controls) {
                ss << " q[" << control << "],";
            }
            if (type == Type::Toffoli) {
                ss << " q[" << *targets.begin() << "];";
            } else {
                ss << " q[" << *targets.begin() << "], q[" << *std::next(targets.begin()) << "];";
            }
            return ss.str();
        }

        LinesLookup controls;
        LinesLookup targets;
        Type        type = Type::None;
    };

} // namespace syrec
