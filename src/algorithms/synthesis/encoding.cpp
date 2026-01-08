/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/encoding.hpp"

#include "core/truthTable/truth_table.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <queue>
#include <stack>
#include <utility>
#include <vector>

namespace syrec {

    template<class T>
    auto computeOutputFreq(TruthTable const& tt, T& outputFreq) -> void {
        for (const auto& [input, output]: tt) {
            auto it = outputFreq.find(output);
            if (it == outputFreq.end()) {
                outputFreq.emplace(output, 1U);
            } else {
                ++it->second;
            }
        }
    }

    template<class T>
    auto topNodeOfHuffmanTree(T const& outputFreq) -> std::shared_ptr<MinHeapNode> {
        // create a priority queue for building the Huffman tree
        auto comp = [](const std::shared_ptr<MinHeapNode>& a, const std::shared_ptr<MinHeapNode>& b) {
            return *a > *b;
        };

        std::priority_queue<std::shared_ptr<MinHeapNode>, std::vector<std::shared_ptr<MinHeapNode>>, decltype(comp)>
                minHeap(comp);

        // initialize the leaves of the Huffman tree from the output frequencies
        for (const auto& [output, freq]: outputFreq) {
            const auto requiredGarbage = static_cast<std::size_t>(std::ceil(std::log2(freq)));
            minHeap.emplace(std::make_shared<MinHeapNode>(output, requiredGarbage));
        }

        // combine the nodes with the smallest weights until there is only one node left
        while (minHeap.size() > 1U) {
            // pop the two nodes with the smallest weights
            const auto left = minHeap.top();
            minHeap.pop();
            const auto right = minHeap.top();
            minHeap.pop();
            // compute appropriate frequency to cover both nodes
            const auto freq = std::max(left->freq, right->freq) + 1U;
            // create new parent node
            auto top   = std::make_shared<MinHeapNode>(TruthTable::Cube{}, freq);
            top->left  = left;
            top->right = right;
            // add node to queue
            minHeap.emplace(std::move(top));
        }

        return minHeap.top();
    }

    template<class T>
    auto alterTTAndCodewords(TruthTable& tt, T& encoding, std::size_t const& requiredGarbage) -> void {
        // Minimum no. of additional lines required.
        const auto additionalLines = tt.minimumAdditionalLinesRequired();
        const auto nBits           = std::max(tt.nInputs(), tt.nOutputs() + additionalLines);
        const auto r               = nBits - requiredGarbage;
        const auto nPrimaryOutputs = tt.nPrimaryOutputs();

        // resize garbage to the correct size.
        tt.getGarbage().resize(requiredGarbage);

        // the bits excluding the primary outputs.
        const auto garbageBits = nBits - nPrimaryOutputs;

        // the bits excluding the primary outputs must be lesser than or equal to the codeword length.
        assert(garbageBits <= requiredGarbage);

        // the bits excluding the primary outputs are set to garbage.
        for (auto i = 0U; i < garbageBits; i++) {
            tt.setGarbage(i);
        }

        // modify the codewords by filling in the redundant dc positions.
        for (auto& [pattern, code]: encoding) {
            TruthTable::Cube outCube(pattern);
            outCube.resize(nBits);

            TruthTable::Cube newCode{};
            newCode.reserve(requiredGarbage);
            for (auto i = 0U; i < requiredGarbage; i++) {
                if (code[i].has_value()) {
                    newCode.emplace_back(code[i]);
                } else {
                    newCode.emplace_back(outCube[r + i]);
                }
            }

            code = newCode;
        }
    }

    auto encodeWithAdditionalLine(TruthTable& tt) -> TruthTable::CubeMap {
        std::map<TruthTable::Cube, std::size_t> outputFreq;

        computeOutputFreq(tt, outputFreq);

        // if the truth table function is already reversible, no encoding is necessary
        if (outputFreq.size() == tt.size()) {
            return {};
        }

        auto const& topNode = topNodeOfHuffmanTree(outputFreq);

        // determine encoding from Huffman tree
        TruthTable::CubeMap encoding{};
        topNode->traverse({}, encoding);

        const auto requiredGarbage = topNode->freq;

        // resize all outputs to the correct size (by adding don't care values)
        for (auto& [input, output]: encoding) {
            output.resize(requiredGarbage);
        }

        alterTTAndCodewords(tt, encoding, requiredGarbage);

        // encode all the outputs
        for (auto& [input, output]: tt) {
            output = encoding[output];
        }

        return encoding;
    }

    auto encodeWithoutAdditionalLine(TruthTable& tt) -> TruthTable::CubeMultiMap {
        std::multimap<TruthTable::Cube, std::size_t> outputFreq;
        computeOutputFreq(tt, outputFreq);

        // ensure that all output frequencies are a power of two
        for (auto it = outputFreq.begin(); it != outputFreq.end();) {
            auto freq = it->second;

            // continue if the output frequency is a power of two
            if ((freq & (freq - 1U)) == 0U) {
                ++it;
                continue;
            }

            // split frequency into powers of two
            std::size_t bitPos = 0U;
            while (freq > 0U) {
                if ((freq & 1U) == 1U) {
                    outputFreq.emplace(it->first, 1U << bitPos);
                }
                freq >>= 1U;
                ++bitPos;
            }
            // need to update iterator after erasing to avoid invalidating it
            it = outputFreq.erase(it);
        }

        // if the truth table function is already reversible, no encoding is necessary
        if (outputFreq.size() == tt.size()) {
            return {};
        }

        auto const& topNode = topNodeOfHuffmanTree(outputFreq);

        // determine encoding from Huffman tree
        TruthTable::CubeMultiMap encoding{};
        topNode->traverse({}, encoding);

        const auto requiredGarbage = topNode->freq;

        std::map<TruthTable::Cube, std::stack<TruthTable::Cube>> encFreq;
        // resize all outputs to the correct size (by adding don't care values)
        for (auto& [input, output]: encoding) {
            auto freq = 1U << (requiredGarbage - output.size());
            output.resize(requiredGarbage);

            while (freq != 0U) {
                encFreq[input].emplace(output);
                freq--;
            }
        }

        alterTTAndCodewords(tt, encoding, requiredGarbage);

        // encode all the outputs
        for (auto& [input, output]: tt) {
            const auto out = output;
            output         = encFreq[out].top();
            encFreq[out].pop();
        }

        return encoding;
    }

    auto augmentWithConstants(TruthTable& tt, std::size_t const& nBits, bool appendZero) -> void {
        const auto requiredOutConstants = nBits - tt.nOutputs();
        const auto requiredInConstants  = nBits - tt.nInputs();

        for (auto& [input, output]: tt) {
            const auto currentGarbageVecSize = tt.getGarbage().size();

            if (appendZero) {
                // based on the requiredOutConstants, zeros are appended to the outputs.
                for (auto i = 0U; i < requiredOutConstants; i++) {
                    // based on the requiredOutConstants, zeros are appended to the outputs.
                    output.emplace_back(false);
                    if (currentGarbageVecSize != nBits) {
                        // add garbage at the LSB.
                        tt.getGarbage().insert(tt.getGarbage().begin(), true);
                    }
                }

            } else {
                for (auto i = 0U; i < requiredOutConstants; i++) {
                    // based on the requiredOutConstants, zeros are inserted to the outputs.
                    output.insertZero();
                    if (currentGarbageVecSize != nBits) {
                        tt.getGarbage().resize(currentGarbageVecSize + 1);
                    }
                }
            }

            const auto inputSize  = input.size();
            const auto outputSize = output.size();
            if (inputSize >= outputSize) {
                continue;
            }

            const auto requiredConstants = outputSize - inputSize;
            auto       newCube           = input;
            newCube.reserve(outputSize);

            const auto currentConstantVecSize = tt.getConstants().size();

            if (appendZero) {
                for (std::size_t i = 0; i < requiredInConstants; i++) {
                    // based on the requiredInConstants, zeros are appended to the inputs.
                    newCube.emplace_back(false);
                    if (currentConstantVecSize != nBits) {
                        // add a constant at the LSB.
                        tt.getConstants().insert(tt.getConstants().begin(), true);
                    }
                }
            } else {
                // based on the requiredConstants, zeros are inserted to the inputs.
                for (std::size_t i = 0; i < requiredConstants; i++) {
                    newCube.insertZero();
                    if (currentConstantVecSize != nBits) {
                        // add a constant at the MSB.
                        tt.getConstants().emplace_back(true);
                    }
                }
            }
            auto nh  = tt.extract(input);
            nh.key() = newCube;
            tt.insert(std::move(nh));
        }
    }

    // explicitly instantiate function templates.
    template void                         computeOutputFreq(TruthTable const& tt, std::map<TruthTable::Cube, std::size_t>& outputFreq);
    template void                         computeOutputFreq(TruthTable const& tt, std::multimap<TruthTable::Cube, std::size_t>& outputFreq);
    template std::shared_ptr<MinHeapNode> topNodeOfHuffmanTree(std::map<TruthTable::Cube, std::size_t> const& outputFreq);
    template std::shared_ptr<MinHeapNode> topNodeOfHuffmanTree(std::multimap<TruthTable::Cube, std::size_t> const& outputFreq);
    template void                         alterTTAndCodewords(TruthTable& tt, TruthTable::CubeMap& encoding, std::size_t const& requiredGarbage);
    template void                         alterTTAndCodewords(TruthTable& tt, TruthTable::CubeMultiMap& encoding, std::size_t const& requiredGarbage);

} // namespace syrec
