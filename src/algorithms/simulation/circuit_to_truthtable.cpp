/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/simulation/circuit_to_truthtable.hpp"

#include "core/truthTable/truth_table.hpp"
#include "dd/Package.hpp"
#include "dd/Simulation.hpp"
#include "dd/StateGeneration.hpp"
#include "ir/QuantumComputation.hpp"

#include <cassert>
#include <cstdint>
#include <memory>

namespace syrec {

    auto buildTruthTable(const qc::QuantumComputation& qc, TruthTable& tt) -> void {
        const auto nBits = qc.getNqubits();

        tt.setConstants(qc.getAncillary());
        tt.setGarbage(qc.getGarbage());

        assert(nBits < 65U);

        auto dd = std::make_unique<dd::Package>(nBits);

        const auto totalInputs = 1U << nBits;

        std::uint64_t n = 0U;

        while (n < totalInputs) {
            const auto inCube = TruthTable::Cube::fromInteger(n, nBits);
            ++n;

            const auto boolCube  = inCube.toBoolVec();
            bool       nextInput = false;

            for (auto i = 0U; i < nBits; i++) {
                if (tt.isConstant(i) && (boolCube[i])) {
                    nextInput = true;
                    break;
                }
            }

            if (nextInput) {
                continue;
            }

            auto const inEdge    = dd::makeBasisState(nBits, boolCube, *dd);
            const auto out       = dd::sample(qc, inEdge, *dd, 1);
            const auto outString = out.begin()->first;

            tt.try_emplace(inCube, TruthTable::Cube::fromString(outString));
        }
    }

} // namespace syrec
