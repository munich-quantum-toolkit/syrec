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

#include "core/truthTable/truth_table.hpp"

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// Boolean expression simplifier (Quine–McCluskey algorithm)
// Adapted from https://github.com/madmann91/minbool
namespace minbool {

    inline std::size_t popcount(const std::uint64_t n) {
        return std::bitset<64U>(n).count();
    }

    struct MinTerm {
        using Value = syrec::TruthTable::Cube::Value;

        explicit MinTerm(const std::uint64_t value = 0U, const std::uint64_t dash = 0U):
            value(value),
            dash(dash) {}

        Value operator[](const std::size_t i) const {
            if (((dash >> i) & 1U) == 1U) {
                return {};
            }
            return {((value >> i) & 1U) == 1U};
        }

        [[nodiscard]] MinTerm combine(const MinTerm& other) const {
            const std::uint64_t mask = (value ^ other.value) | (dash ^ other.dash);
            return MinTerm{value & ~mask, dash | mask};
        }

        template<typename F>
        void foreachValue(F f, const std::size_t n, const std::size_t bit = 0U, const std::uint64_t cur = 0U) const {
            if (bit == n) {
                f(cur);
            } else {
                const auto val = (*this)[bit];
                if (val == Value{}) {
                    foreachValue(f, n, bit + 1U, cur);
                    foreachValue(f, n, bit + 1U, cur | (1 << bit));
                } else {
                    if (val == Value{false}) {
                        foreachValue(f, n, bit + 1U, cur);
                    } else {
                        foreachValue(f, n, bit + 1U, cur | (1U << bit));
                    }
                }
            }
        }

        friend bool operator<(const MinTerm& lhs, const MinTerm& rhs) {
            return lhs.value < rhs.value || (lhs.value == rhs.value && lhs.dash < rhs.dash);
        }

        friend bool operator==(const MinTerm& lhs, const MinTerm& rhs) {
            return lhs.value == rhs.value && lhs.dash == rhs.dash;
        }

        std::uint64_t value;
        std::uint64_t dash;
    };

} // namespace minbool

namespace std {
    template<>
    struct hash<minbool::MinTerm> {
        auto operator()(const minbool::MinTerm& term) const -> size_t {
            return (33 * term.value ^ term.dash);
        }
    };
} // namespace std

namespace minbool {

    struct ImplicantTable {
        std::vector<std::size_t> groups;
        std::vector<bool>        marks;
        std::vector<MinTerm>     terms;
        std::size_t              nBits;

        explicit ImplicantTable(const std::size_t nBits):
            nBits(nBits) {}

        [[nodiscard]] std::size_t size() const { return terms.size(); }

        void fill(const std::vector<MinTerm>& minterms);

        void combine(std::vector<MinTerm>& res);

        void primes(std::vector<MinTerm>& res) {
            const auto nTerms = terms.size();
            for (std::size_t i = 0U; i < nTerms; ++i) {
                if (!marks[i]) {
                    res.emplace_back(terms[i]);
                }
            }
        }
    };

    struct PrimeChart {
        std::unordered_map<std::uint64_t, std::vector<MinTerm>> columns;
        std::size_t                                             nBits;

        explicit PrimeChart(const std::size_t nBits):
            nBits(nBits) {}

        [[nodiscard]] std::size_t size() const {
            return columns.size();
        }

        void fill(const std::vector<MinTerm>& primes);

        bool removeEssentials(std::vector<MinTerm>& essentials);

        void removeHeuristic(std::vector<MinTerm>& solution);

        bool simplify();
    };

    std::vector<MinTerm> primeImplicants(std::vector<MinTerm>& terms, const std::size_t& n);

    bool evalBoolean(const std::vector<MinTerm>& solution, std::uint64_t v, const std::size_t& n);

    bool checkSolution(const std::vector<MinTerm>& solution, const std::unordered_set<std::uint64_t>& onValues, const std::size_t& n);

    syrec::TruthTable::Cube::Set minimizeBoolean(syrec::TruthTable::Cube::Set const& sigVec);

} // namespace minbool
