/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/optimization/esop_minimization.hpp"

#include "core/truthTable/truth_table.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minbool {

    void ImplicantTable::fill(const std::vector<MinTerm>& minterms) {
        groups.resize(nBits + 2U, 0U);
        for (const auto& term: minterms) {
            ++groups[popcount(term.value)];
        }
        std::partial_sum(groups.begin(), groups.end(), groups.begin());
        terms.resize(minterms.size());
        marks.resize(minterms.size());
        for (const auto& term: minterms) {
            groups[popcount(term.value)]        = groups[popcount(term.value)] - 1;
            terms[groups[popcount(term.value)]] = term;
        }
    }

    void ImplicantTable::combine(std::vector<MinTerm>& res) {
        for (std::size_t i = 0; i < nBits; ++i) {
            for (std::size_t j = groups[i]; j < groups[i + 1]; ++j) {
                for (std::size_t k = groups[i + 1]; k < groups[i + 2]; ++k) {
                    auto const& termA = terms[j];
                    auto const& termB = terms[k];
                    if ((termA.value & termB.value) == termA.value && (termA.dash == termB.dash)) {
                        marks[j] = true;
                        marks[k] = true;
                        res.emplace_back(termA.combine(termB));
                    }
                }
            }
        }
    }

    void PrimeChart::fill(const std::vector<MinTerm>& primes) {
        for (const auto& prime: primes) {
            prime.foreachValue([this, &prime](const std::uint64_t value) {
                columns[value].emplace_back(prime);
            },
                               nBits);
        }
        for (auto& [first, second]: columns) {
            std::ranges::sort(second, std::less{});
        }
    }

    bool PrimeChart::removeEssentials(std::vector<MinTerm>& essentials) {
        std::size_t const count = essentials.size();
        for (const auto& [first, second]: columns) {
            if (second.size() == 1U) {
                essentials.emplace_back(second.front());
            }
        }
        // No essential prime has been found
        if (essentials.size() == count) {
            return false;
        }
        std::sort(essentials.begin() + static_cast<int>(count), essentials.end());
        essentials.erase(std::unique(essentials.begin() + static_cast<int>(count), essentials.end()), essentials.end());

        std::for_each(essentials.begin() + static_cast<int>(count), essentials.end(), [&](const MinTerm& term) {
            term.foreachValue([this](const std::uint64_t value) {
                columns.erase(value);
            },
                              nBits);
        });
        return true;
    }

    void PrimeChart::removeHeuristic(std::vector<MinTerm>& solution) {
        assert(size() > 0);
        std::unordered_map<MinTerm, std::size_t> covers;
        for (auto const& [first, second]: columns) {
            for (const auto& term: second) {
                ++covers[term];
            }
        }
        // Heuristic: Remove the term that covers the most columns
        std::size_t maxCovers = 0U;
        MinTerm     term;
        for (auto const& [first, second]: covers) {
            if (second > maxCovers) {
                maxCovers = second;
                term      = first;
            }
        }
        solution.emplace_back(term);
        term.foreachValue([this](const std::uint64_t value) {
            columns.erase(value);
        },
                          nBits);
    }

    bool PrimeChart::simplify() {
        bool change = false;

        for (auto& [pair1First, pair1Second]: columns) {
            for (auto& [pair2First, pair2Second]: columns) {
                if (pair1First == pair2First) {
                    continue;
                }
                // Dominating columns are eliminated
                std::ranges::sort(pair1Second, std::less{});
                std::ranges::sort(pair2Second, std::less{});
                if (std::ranges::includes(pair2Second, pair1Second, std::less{})) {
                    columns.erase(pair2First);
                    change = true;
                    break;
                }
            }
        }
        // Transpose columns → rows
        std::unordered_map<MinTerm, std::vector<std::uint64_t>> rows;
        for (auto& [first, second]: columns) {
            for (auto const& term: second) {
                rows[term].emplace_back(first);
            }
            second.clear();
        }
        for (auto& [first, second]: rows) {
            std::ranges::sort(second);
        }

        for (const auto& [pair1First, pair1Second]: rows) {
            for (const auto& [pair2First, pair2Second]: rows) {
                if (pair1First == pair2First) {
                    continue;
                }
                // Dominated rows are eliminated
                if (std::ranges::includes(pair1Second, pair2Second)) {
                    rows.erase(pair2First);
                    change = true;
                    break;
                }
            }
        }

        // Transpose rows => columns
        for (auto const& [first, second]: rows) {
            for (const auto& value: second) {
                columns[value].emplace_back(first);
            }
        }
        for (auto& [first, second]: columns) {
            std::ranges::sort(second, std::less{});
        }

        return change;
    }

    std::vector<MinTerm> primeImplicants(std::vector<MinTerm>& terms, const std::size_t& n) {
        std::vector<MinTerm> primes;

        while (!terms.empty()) {
            ImplicantTable table(n);
            table.fill(terms);
            terms.clear();
            table.combine(terms);
            // Remove duplicates
            std::ranges::sort(terms, std::less{});
            terms.erase(std::ranges::unique(terms).begin(), terms.end());
            table.primes(primes);
        }
        return primes;
    }

    bool evalBoolean(const std::vector<MinTerm>& solution, const std::uint64_t v, const std::size_t& n) {
        for (const auto& term: solution) {
            bool prod = true;
            for (std::size_t i = 0U; i < n; ++i) {
                bool const bit = ((v >> i) & 1) != 0;
                if (term[i] == MinTerm::Value{true}) {
                    prod = prod && bit;
                } else if (term[i] == MinTerm::Value{false}) {
                    prod = prod && !bit;
                }
            }
            if (prod) {
                return true;
            }
        }
        return false;
    }

    bool checkSolution(const std::vector<MinTerm>&              solution,
                       const std::unordered_set<std::uint64_t>& onValues, const std::size_t& n) {
        for (const auto& v: onValues) {
            if (!evalBoolean(solution, v, n)) {
                return false;
            }
        }
        for (std::uint64_t i = (1 << n) - 1; i > 0U; --i) {
            if (!onValues.contains(i) && evalBoolean(solution, i, n)) {
                return false;
            }
        }
        return onValues.contains(0) || !evalBoolean(solution, 0U, n);
    }

    syrec::TruthTable::Cube::Set minimizeBoolean(syrec::TruthTable::Cube::Set const& sigVec) {
        if (sigVec.size() <= 1U) {
            return sigVec;
        }

        std::unordered_set<std::uint64_t> onValues;
        onValues.reserve(sigVec.size());

        for (const auto& onSig: sigVec) {
            onValues.emplace(onSig.toInteger());
        }

        std::vector<MinTerm> init;
        init.reserve(sigVec.size());

        for (const auto& on: onValues) {
            init.emplace_back(on);
        }

        const auto n      = sigVec.begin()->size();
        const auto primes = primeImplicants(init, n);

        PrimeChart chart(n);
        chart.fill(primes);

        std::vector<MinTerm> solution;
        while (chart.size() > 0) {
            bool change = chart.removeEssentials(solution);
            change      = change || chart.simplify();
            if (!change && chart.size() > 0) {
                chart.removeHeuristic(solution);
            }
        }

        assert(checkSolution(solution, onValues, n));

        syrec::TruthTable::Cube::Set finalSigVec;

        for (auto const& ctrlCube: solution) {
            syrec::TruthTable::Cube c;
            for (int j = static_cast<int>(n) - 1; j >= 0; --j) {
                c.emplace_back(ctrlCube[static_cast<std::size_t>(j)]);
            }
            finalSigVec.emplace(c);
        }

        return finalSigVec;
    }

} // namespace minbool
