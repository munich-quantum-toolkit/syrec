/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/io/pla_parser.hpp"

#include "core/truthTable/truth_table.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <istream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace syrec {

    void parsePla(TruthTable& tt, std::istream& in) {
        std::string      line;
        std::size_t      nInputs  = 0;
        std::size_t      nOutputs = 0;
        std::regex const whitespace("\\s+");

        while (in.good() && getline(in, line)) {
            trim(line);
            line = std::regex_replace(line, whitespace, " ");
            if ((line.empty()) || (line.starts_with('#')) || (line.starts_with(".ilb")) || (line.starts_with(".ob")) || (line.starts_with(".p")) || (line.starts_with(".type "))) {
                continue;
            }

            if (line.starts_with(".i")) {
                nInputs = static_cast<std::size_t>(std::stoi(line.substr(3)));
                // resize the tt constants.
                tt.getConstants().resize(nInputs);
            }

            else if (line.starts_with(".o")) {
                nOutputs = static_cast<std::size_t>(std::stoi(line.substr(3)));
                // resize the tt garbage.
                tt.getGarbage().resize(nOutputs);
            }

            else if (line == ".e") {
                break;
            }

            else {
                assert((line[0] == '0' || line[0] == '1' || line[0] == '-' || line[0] == '~'));

                std::vector<std::string> inputOutputMapping;

                auto        lineString = std::stringstream(line);
                std::string tokenString;
                while (std::getline(lineString, tokenString, ' ')) {
                    inputOutputMapping.emplace_back(tokenString);
                }

                if (inputOutputMapping.size() != 2) {
                    throw std::invalid_argument("Expected exactly 2 columns (input and output), received " + std::to_string(inputOutputMapping.size()) + std::string(" columns"));
                }

                if (inputOutputMapping[0].size() != nInputs) {
                    throw std::invalid_argument(".i " + std::string("(") + std::to_string(nInputs) + std::string(")") + std::string(" not equal to received number of inputs ") + std::string("(") + std::to_string(inputOutputMapping[0].size()) + std::string(")"));
                }

                if (inputOutputMapping[1].size() != nOutputs) {
                    throw std::invalid_argument(".o " + std::string("(") + std::to_string(nOutputs) + std::string(")") + std::string(" not equal to received number of outputs ") + std::string("(") + std::to_string(inputOutputMapping[1].size()) + std::string(")"));
                }

                TruthTable::Cube cubeIn;
                cubeIn.reserve(nInputs);

                for (auto s: inputOutputMapping[0]) {
                    cubeIn.emplace_back(TruthTable::Cube::getValue(s));
                }

                TruthTable::Cube cubeOut;
                cubeOut.reserve(nOutputs);

                for (auto s: inputOutputMapping[1]) {
                    cubeOut.emplace_back(TruthTable::Cube::getValue(s));
                }

                tt.try_emplace(cubeIn, cubeOut);
            }
        }
    }

    auto extend(TruthTable& tt) -> void {
        // ensure that the resulting complete table can be stored in the cube map (at most 63 inputs, probably less in practice)
        if (!tt.empty() && (tt.nInputs() > static_cast<std::size_t>(std::log2(tt.max_size())) || tt.nInputs() > 63U)) {
            throw std::invalid_argument("Overflow!, Number of inputs is greater than maximum capacity " + std::string("(") + std::to_string(std::min(static_cast<unsigned>(std::log2(tt.max_size())), 63U)) + std::string(")"));
        }

        TruthTable newTT{};

        for (auto const& [input, output]: tt) {
            // compute the complete cubes for the input
            auto completeInputs = input.completeCubes();
            // move all the complete cubes to the new cube map
            for (auto const& completeInput: completeInputs) {
                const auto inputIt = newTT.find(input);

                if (inputIt != newTT.end()) {
                    TruthTable::Cube newOutCube{inputIt->second};

                    // clubbing the 1's of the new output with the old one
                    for (auto i = 0U; i < tt.nOutputs(); i++) {
                        if (output[i].has_value() && newOutCube[i].has_value() && *output[i]) {
                            newOutCube[i] = true;
                        }
                    }

                    assert(output != newOutCube);
                    // erasing the old output.
                    newTT.erase(inputIt);
                    newTT.try_emplace(completeInput, newOutCube);
                } else {
                    newTT.try_emplace(completeInput, output);
                }
            }
        }
        // swap the new cube map with the old one
        tt.swap(newTT);

        // construct output cube
        const auto output = TruthTable::Cube(tt.nOutputs(), false);

        std::uint64_t pos = 0U;
        for (const auto& [input, _]: tt) {
            // fill in all the missing inputs
            const auto number = input.toInteger();
            for (std::uint64_t i = pos; i < number; ++i) {
                tt[TruthTable::Cube::fromInteger(i, tt.nInputs())] = output;
            }
            pos = number + 1U;
        }
        // fill in the remaining missing inputs (if any)
        const std::uint64_t max = 1ULL << tt.nInputs();
        for (std::uint64_t i = pos; i < max; ++i) {
            tt[TruthTable::Cube::fromInteger(i, tt.nInputs())] = output;
        }
    }

    bool readPla(TruthTable& tt, const std::string& filename) {
        std::ifstream is;
        is.open(filename.c_str(), std::ifstream::in);

        if (!is.good()) {
            std::cerr << "Cannot open " + filename << '\n';
            return false;
        }

        parsePla(tt, is);

        // extending the truth table.
        extend(tt);

        return true;
    }

} // namespace syrec
