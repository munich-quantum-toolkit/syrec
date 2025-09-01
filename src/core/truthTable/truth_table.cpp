/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/truthTable/truth_table.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace syrec {

    auto TruthTable::Cube::completeCubes() const -> Vector {
        std::vector<std::size_t> dcPositions;
        dcPositions.reserve(size());
        for (auto pos = 0U; pos < size(); pos++) {
            if (!cube[pos].has_value()) // if DC
            {
                dcPositions.emplace_back(pos);
            }
        }

        Vector     result{};
        const auto dcVecSize = dcPositions.size();
        const auto dcSize    = 1U << dcVecSize;
        result.reserve(dcSize);
        Cube dcCube(cube);

        for (auto i = 0U; i < dcSize; ++i) {
            for (auto j = 0U; j < dcVecSize; ++j) {
                const auto localBit = (i & (1U << (dcVecSize - j - 1))) != 0;

                dcCube[dcPositions[j]] = localBit;
            }
            result.emplace_back(dcCube);
        }
        return result;
    }

    auto TruthTable::filteredInput(const Cube& input) const -> Cube {
        // the size of the provided input should be the same as the constants stored in the tt.
        assert(input.size() == constants.size());
        const auto inputSize = input.size();

        Cube filteredInput{};
        filteredInput.reserve(nPrimaryInputs());
        for (auto i = 0U; i < inputSize; i++) {
            if (!isConstant(inputSize - 1 - i)) {
                filteredInput.emplace_back(input[i]);
            }
        }

        return filteredInput;
    }

    auto TruthTable::filteredOutput(const Cube& output) const -> Cube {
        // the size of the provided output should be the same as the garbage stored in the tt.
        assert(output.size() == garbage.size());
        const auto outputSize = output.size();

        Cube filteredOutput{};
        filteredOutput.reserve(nPrimaryOutputs());
        for (auto i = 0U; i < outputSize; i++) {
            if (!isGarbage(outputSize - 1 - i)) {
                filteredOutput.emplace_back(output[i]);
            }
        }
        return filteredOutput;
    }

    auto TruthTable::equal(TruthTable const& tt1, TruthTable const& tt2, bool equalityUpToDontCare) -> bool {
        if (!equalityUpToDontCare) {
            return (tt1 == tt2);
        }

        // the number of primary inputs and outputs should be equal for both the truth tables.
        if (tt1.nPrimaryInputs() != tt2.nPrimaryInputs() && tt1.nPrimaryOutputs() != tt2.nPrimaryOutputs()) {
            return false;
        }

        auto tt1It = tt1.begin();
        auto tt2It = tt2.begin();

        while (tt1It != tt1.end() || tt2It != tt2.end()) {
            const auto& [input1, output1] = *tt1It;
            const auto& [input2, output2] = *tt2It;
            if ((tt1.filteredInput(input1) != tt2.filteredInput(input2)) || (!TruthTable::Cube::checkCubeEquality(tt1.filteredOutput(output1), tt2.filteredOutput(output2)))) {
                return false;
            }
            ++tt1It;
            ++tt2It;
        }

        return true;
    }

    auto TruthTable::minimumAdditionalLinesRequired() const -> std::size_t {
        // calculate the frequency of each unique output pattern.
        std::map<TruthTable::Cube, std::size_t> outputFreq;
        for (const auto& [input, output]: cubeMap) {
            outputFreq[output]++;
        }

        const auto maxPair = std::ranges::max_element(outputFreq, [](const std::pair<TruthTable::Cube, std::size_t>& p1, const std::pair<TruthTable::Cube, std::size_t>& p2) { return p1.second < p2.second; });

        return static_cast<std::size_t>(std::ceil(std::log2(maxPair->second)));
    }
} // namespace syrec
