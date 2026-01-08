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

namespace syrec {

    struct MinHeapNode {
        TruthTable::Cube data;

        std::size_t freq{};

        std::shared_ptr<MinHeapNode> left{};
        std::shared_ptr<MinHeapNode> right{};

        MinHeapNode(TruthTable::Cube data, const std::size_t freq):
            data(std::move(data)), freq(freq) {}

        auto operator>(const MinHeapNode& other) const -> bool {
            return (freq > other.freq || (freq == other.freq && (data > other.data)));
        }

        template<class T>
        auto traverse(TruthTable::Cube&& encodedCube, T& encoding) const -> void {
            // leaf node -> add encoding
            if (!data.empty()) {
                encoding.emplace(data, std::move(encodedCube));
                return;
            }

            // non-leaf node -> traverse left and right subtree
            if (left) {
                left->traverse(encodedCube.appendZero(), encoding);
            }
            if (right) {
                right->traverse(encodedCube.appendOne(), encoding);
            }
        }
    };

    template<class T>
    auto computeOutputFreq(TruthTable const& tt, T& outputFreq) -> void;

    template<class T>
    auto topNodeOfHuffmanTree(T const& outputFreq) -> std::shared_ptr<MinHeapNode>;

    template<class T>
    auto alterTTAndCodewords(TruthTable& tt, T& encoding, std::size_t const& requiredGarbage) -> void;

    auto encodeWithAdditionalLine(TruthTable& tt) -> TruthTable::CubeMap;

    auto encodeWithoutAdditionalLine(TruthTable& tt) -> TruthTable::CubeMultiMap;

    auto augmentWithConstants(TruthTable& tt, std::size_t const& nBits, bool appendZero = false) -> void;

} //namespace syrec
