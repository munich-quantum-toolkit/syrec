/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/simple_simulation.hpp"

#include "core/circuit.hpp"
#include "core/gate.hpp"
#include "core/n_bit_values_container.hpp"
#include "core/properties.hpp"
#include "core/utils/timer.hpp"

#include <cstddef>
#include <iostream>

namespace syrec {
    void coreGateSimulation(const Gate& g, NBitValuesContainer& input) {
        if (g.type == Gate::Type::Toffoli) {
            NBitValuesContainer cMask(input.size());
            for (const auto& c: g.controls) {
                cMask.set(c);
            }

            if (cMask.none() || ((input & cMask) == cMask)) {
                input.flip(*g.targets.begin());
            }
        } else if (g.type == Gate::Type::Fredkin) {
            NBitValuesContainer cMask(input.size());
            for (const auto& c: g.controls) {
                cMask.set(c);
            }

            if (cMask.none() || ((input & cMask) == cMask)) {
                // get both positions and values
                auto              it = g.targets.begin();
                const std::size_t t1 = *it++;
                const std::size_t t2 = *it;

                const bool t1v = input[t1];
                const bool t2v = input[t2];

                // only swap when different
                if (t1v != t2v) {
                    input.set(t1, t2v);
                    input.set(t2, t1v);
                }
            }
        } else {
            std::cerr << "Unknown gate: Simulation error\n";
        }
    }

    void simpleSimulation(NBitValuesContainer& output, const Circuit& circ, const NBitValuesContainer& input,
                          const Properties::ptr& statistics) {
        Timer<PropertiesTimer> t;

        if (statistics) {
            const PropertiesTimer rt(statistics);
            t.start(rt);
        }

        output = input;
        for (const auto& g: circ) {
            coreGateSimulation(*g, output);
        }

        if (statistics) {
            t.stop();
        }
    }
} // namespace syrec
