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

#include "dd/Package.hpp"

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

namespace syrec {

    class TruthTable {
    public:
        class Cube {
        public:
            using Value  = std::optional<bool>;
            using Vector = std::vector<Cube>;
            using Set    = std::set<Cube>;

        private:
            std::vector<Value> cube{};

        public:
            Cube() = default;
            explicit Cube(std::vector<Value> cube):
                cube(std::move(cube)) {}

            Cube(const std::size_t bw, const Value& initializer) {
                cube.resize(bw, initializer);
            }

            template<class InputIt>
            Cube(InputIt first, InputIt last) {
                std::copy(first, last, std::back_inserter(cube));
            }

            static auto getValue(const char& c) -> Value {
                switch (c) {
                    case '-':
                    case '~':
                        return {};
                    case '0':
                        return false;
                    case '1':
                        return true;
                    default:
                        throw std::invalid_argument("Unknown Character");
                }
            }

            static auto findMissingCube(TruthTable::Cube::Set const& p1SigVec) -> Cube {
                if (p1SigVec.empty()) {
                    return {};
                }

                const auto    bitwidth = p1SigVec.begin()->size();
                std::uint64_t idx      = 0U;
                while (true) {
                    if (auto cube = Cube::fromInteger(idx, bitwidth); std::find(p1SigVec.begin(), p1SigVec.end(), cube) == p1SigVec.end()) {
                        return cube;
                    }
                    ++idx;
                }
            }

            // construct a cube from a (64bit) number with a given bitwidth
            static auto fromInteger(const std::uint64_t number, const std::size_t bw) -> Cube {
                assert(bw <= 64U);
                Cube cube{};
                cube.reserve(bw);
                for (std::size_t i = 0U; i < bw; ++i) {
                    cube.emplace_back((number & (1ULL << (bw - 1U - i))) != 0U);
                }
                return cube;
            }

            // construct a cube from a string
            static auto fromString(const std::string& str) -> Cube {
                Cube cube{};
                cube.reserve(str.size());
                for (const auto& s: str) {
                    cube.emplace_back(getValue(s));
                }
                return cube;
            }

            // return integer representation of the cube
            [[nodiscard]] auto toInteger() const -> std::uint64_t {
                assert(cube.size() <= 64U);
                assert(std::none_of(cube.cbegin(), cube.cend(), [](auto const& v) { return !v.has_value(); }));
                std::uint64_t result = 0U;
                for (std::size_t i = 0U; i < cube.size(); ++i) {
                    if (*cube[i]) {
                        result |= (1ULL << (cube.size() - 1U - i));
                    }
                }
                return result;
            }

            // return bool vec representation of the cube (order is b0,b1,b2......bn)
            [[nodiscard]] auto toBoolVec() const -> std::vector<bool> {
                assert(std::all_of(cube.cbegin(), cube.cend(), [](auto const& v) { return v.has_value(); }));
                const auto        nBits = size();
                std::vector<bool> result(nBits);
                for (std::size_t i = 0U; i < nBits; ++i) {
                    result[nBits - 1 - i] = *cube[i];
                }
                return result;
            }

            // return string representation of the cube
            [[nodiscard]] auto toString() const -> std::string {
                std::stringstream ss{};
                for (const auto& i: cube) {
                    if (!i.has_value()) {
                        ss << '-';
                    } else {
                        ss << (*i ? '1' : '0');
                    }
                }
                return ss.str();
            }

            // checks if 2 Cubes are equal irrespective of don't care
            [[nodiscard]] static auto checkCubeEquality(const Cube& c1, const Cube& c2, const bool equalityUpToDontCare = true) -> bool {
                if (c1.size() != c2.size()) {
                    return false;
                }
                const auto nBits = c1.size();
                for (auto i = 0U; i < nBits; ++i) {
                    if (equalityUpToDontCare && (!c1[i].has_value() || !c2[i].has_value())) {
                        continue;
                    }
                    if (*c1[i] != *c2[i]) {
                        return false;
                    }
                }
                return true;
            }

            [[nodiscard]] auto completeCubes() const -> Vector;

            auto insertZero() -> void {
                cube.insert(cube.begin(), false);
            }
            [[nodiscard]] auto append(const Value& v) const -> Cube {
                auto c = cube;
                c.emplace_back(v);
                return Cube(c);
            }
            [[nodiscard]] auto appendZero() const -> Cube {
                return append(false);
            }
            [[nodiscard]] auto appendOne() const -> Cube {
                return append(true);
            }

            // pass-through functions for underlying vector

            auto operator[](std::size_t pos) -> Value& {
                return cube[pos];
            }

            auto operator[](std::size_t pos) const -> const Value& {
                return cube[pos];
            }

            auto operator<(const Cube& cv) const -> bool {
                return (cube < cv.cube);
            }

            auto operator>(const Cube& cv) const -> bool {
                return (cube > cv.cube);
            }

            auto operator==(const Cube& cv) const -> bool {
                return (cube == cv.cube);
            }

            auto operator!=(const Cube& cv) const -> bool {
                return (cube != cv.cube);
            }

            auto reserve(const std::size_t n) -> void {
                cube.reserve(n);
            }

            auto resize(const std::size_t n, Value val = Value()) -> void {
                cube.resize(n, val);
            }

            auto resize(const std::size_t n, const Value& val) -> void {
                cube.resize(n, val);
            }

            auto emplace_back(const Value& v) -> void { // NOLINT(readability-identifier-naming) keeping same Interface as std::vector
                cube.emplace_back(v);
            }

            auto pop_back() -> void { // NOLINT(readability-identifier-naming) keeping same Interface as std::vector
                cube.pop_back();
            }

            [[nodiscard]] auto equals(const std::uint64_t num, const std::size_t bw) const -> bool {
                return *this == (Cube::fromInteger(num, bw));
            }

            [[nodiscard]] auto equals(const std::string& str) const -> bool {
                return *this == (Cube::fromString(str));
            }

            [[nodiscard]] auto size() const -> std::size_t {
                return cube.size();
            }
            [[nodiscard]] auto empty() const -> bool {
                return cube.empty();
            }
            [[nodiscard]] auto begin() const -> decltype(cube.begin()) {
                return cube.begin();
            }
            [[nodiscard]] auto cbegin() const -> decltype(cube.cbegin()) {
                return cube.cbegin();
            }
            [[nodiscard]] auto end() const -> decltype(cube.end()) {
                return cube.end();
            }
            [[nodiscard]] auto cend() const -> decltype(cube.cend()) {
                return cube.cend();
            }
        };

        using CubeMap      = std::map<Cube, Cube>;
        using CubeMultiMap = std::multimap<Cube, Cube>;

    private:
        CubeMap           cubeMap{};
        std::vector<bool> constants;
        std::vector<bool> garbage;

    public:
        auto setConstants(std::vector<bool> const& c) -> void {
            constants = c;
        }

        auto setGarbage(std::vector<bool> const& g) -> void {
            garbage = g;
        }

        [[maybe_unused]] auto setConstant(const std::size_t n) -> void {
            constants[n] = true;
        }

        auto setGarbage(const std::size_t n) -> void {
            garbage[n] = true;
        }

        [[nodiscard]] auto getConstants() const -> const std::vector<bool>& {
            return constants;
        }

        [[nodiscard]] auto getGarbage() const -> const std::vector<bool>& {
            return garbage;
        }

        [[nodiscard]] auto getConstants() -> std::vector<bool>& {
            return constants;
        }

        [[nodiscard]] auto getGarbage() -> std::vector<bool>& {
            return garbage;
        }

        [[nodiscard]] auto isConstant(const std::size_t n) const -> bool {
            return constants[n];
        }

        [[nodiscard]] auto isGarbage(const std::size_t n) const -> bool {
            return garbage[n];
        }

        auto operator==(const TruthTable& tt) const -> bool {
            return (cubeMap == tt.cubeMap);
        }

        auto operator[](const Cube& key) -> Cube& {
            return cubeMap[key];
        }

        auto operator[](Cube&& key) -> Cube& {
            return cubeMap[std::move(key)];
        }

        [[nodiscard]] auto begin() -> decltype(cubeMap.begin()) {
            return cubeMap.begin();
        }

        [[nodiscard]] auto end() -> decltype(cubeMap.end()) {
            return cubeMap.end();
        }

        [[nodiscard]] auto begin() const -> decltype(cubeMap.begin()) {
            return cubeMap.begin();
        }

        [[nodiscard]] auto end() const -> decltype(cubeMap.end()) {
            return cubeMap.end();
        }

        [[nodiscard]] auto empty() const -> bool {
            return cubeMap.empty();
        }

        [[nodiscard]] auto size() const -> std::size_t {
            return cubeMap.size();
        }

        [[nodiscard]] auto max_size() const -> std::size_t { // NOLINT(readability-identifier-naming) keeping same Interface as std::vector
            return cubeMap.max_size();
        }

        [[nodiscard]] auto nInputs() const -> std::size_t {
            if (cubeMap.empty()) {
                return 0U;
            }
            return cubeMap.begin()->first.size();
        }

        [[nodiscard]] auto nPrimaryInputs() const -> std::size_t {
            return static_cast<std::size_t>(std::count(constants.begin(), constants.end(), false));
        }

        [[nodiscard]] auto nOutputs() const -> std::size_t {
            if (cubeMap.empty()) {
                return 0U;
            }
            return cubeMap.begin()->second.size();
        }

        [[nodiscard]] auto nPrimaryOutputs() const -> std::size_t {
            return static_cast<std::size_t>(std::count(garbage.begin(), garbage.end(), false));
        }

        auto extract(Cube const& key) -> CubeMap::node_type {
            return cubeMap.extract(key);
        }

        auto swap(TruthTable& other) noexcept -> void {
            cubeMap.swap(other.cubeMap);
        }

        auto find(const std::uint64_t number, const std::size_t bw) -> decltype(cubeMap.cbegin()) {
            return cubeMap.find(Cube::fromInteger(number, bw));
        }

        auto find(const std::string& str) -> decltype(cubeMap.cbegin()) {
            return cubeMap.find(Cube::fromString(str));
        }

        auto find(const Cube& c) -> decltype(cubeMap.cbegin()) {
            return cubeMap.find(c);
        }

        template<class It>
        auto erase(It elem) -> It {
            return cubeMap.erase(elem);
        }

        // filters the inputs based on the number of primary inputs.
        [[nodiscard]] auto filteredInput(const Cube& input) const -> Cube;

        // filters the outputs based on the number of primary outputs.
        [[nodiscard]] auto filteredOutput(const Cube& output) const -> Cube;

        auto try_emplace(const Cube& input, const Cube& output) -> void { // NOLINT(readability-identifier-naming) keeping same Interface as std::vector
            assert(cubeMap.empty() || (input.size() == nInputs() && output.size() == nOutputs()));
            cubeMap.try_emplace(input, output);
        }
        auto try_emplace(Cube&& input, Cube&& output) -> void { // NOLINT(readability-identifier-naming) keeping same Interface as std::vector
            assert(cubeMap.empty() || (input.size() == nInputs() && output.size() == nOutputs()));
            cubeMap.try_emplace(std::move(input), std::move(output));
        }

        auto insert(CubeMap::node_type nh) -> void {
            cubeMap.insert(std::move(nh));
        }

        static auto equal(TruthTable const& tt1, TruthTable const& tt2, bool equalityUpToDontCare = true) -> bool;

        [[nodiscard]] auto minimumAdditionalLinesRequired() const -> std::size_t;

        auto clear() -> void {
            cubeMap.clear();
        }
    };
} // namespace syrec
