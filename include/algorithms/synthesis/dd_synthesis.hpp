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
#include "dd/Node.hpp"
#include "dd/Package.hpp"
#include "ir/Definitions.hpp"
#include "ir/QuantumComputation.hpp"
#include "ir/operations/Control.hpp"

#include <cstddef>
#include <memory>

namespace syrec {

    auto buildDD(const TruthTable& tt, std::unique_ptr<dd::Package>& dd) -> dd::mEdge;

    class DDSynthesizer {
    public:
        static auto synthesizeCodingTechniques(const TruthTable& tt, const bool withAdditionalLine = true) -> std::shared_ptr<qc::QuantumComputation> {
            DDSynthesizer synthesizer{};
            return synthesizer.synthesizeCodingTechniquesTT(tt, withAdditionalLine);
        }

        static auto synthesizeOnePass(const TruthTable& tt) -> std::shared_ptr<qc::QuantumComputation> {
            DDSynthesizer synthesizer{};
            return synthesizer.synthesizeOnePassTT(tt);
        }

        auto synthesize(dd::mEdge src, std::unique_ptr<dd::Package>& dd) -> std::shared_ptr<qc::QuantumComputation>;

        [[nodiscard]] auto numGate() const -> std::size_t {
            return numGates;
        }

        auto reset() -> void {
            runtime     = 0.;
            numGates    = 0U;
            n           = 0U;
            m           = 0U;
            totalNoBits = 0U;
            r           = 0U;
            garbageFlag = false;
        }

        [[nodiscard]] auto getExecutionTime() const -> double {
            return runtime;
        }

    private:
        double                                  runtime  = 0.;
        std::size_t                             numGates = 0U;
        std::unique_ptr<dd::Package>            ddSynth;
        std::shared_ptr<qc::QuantumComputation> qc;

        // n -> No. of primary inputs.
        // m -> No. of primary outputs.
        // totalNoBits -> Total no. of bits required to create the circuit
        // r -> Additional variables/bits required to decode the output patterns.

        std::size_t n           = 0U;
        std::size_t m           = 0U;
        std::size_t totalNoBits = 0U;
        std::size_t r           = 0U;
        bool        garbageFlag = false;

        static auto pathFromSrcDst(dd::mEdge const& src, dd::mNode* const& dst, TruthTable::Cube::Set& sigVec) -> void;
        static auto pathFromSrcDst(dd::mEdge const& src, size_t level, dd::mNode* const& dst, TruthTable::Cube::Set& sigVec, TruthTable::Cube& cube) -> void;

        [[nodiscard]] static auto finalSrcPathSignature(dd::mEdge const& src, dd::mEdge const& current, TruthTable::Cube::Set const& p1SigVec, TruthTable::Cube::Set const& p2SigVec, bool const& changePaths, std::unique_ptr<dd::Package>& dd) -> TruthTable::Cube::Set;

        static auto pathSignature(dd::mEdge const& src, size_t pathLength, TruthTable::Cube::Set& sigVec) -> void;
        static auto pathSignature(dd::mEdge const& src, size_t pathLength, TruthTable::Cube::Set& sigVec, TruthTable::Cube& cube) -> void;

        static auto completeUniCubes(TruthTable::Cube::Set const& p1SigVec, TruthTable::Cube::Set const& p2SigVec, TruthTable::Cube::Set& uniqueCubeVec) -> void;

        auto applyOperation(qc::Qubit targetBit, dd::mEdge& to, const qc::Controls& ctrl, const std::unique_ptr<dd::Package>& dd) -> void;

        static auto controlRoot(dd::mEdge const& current, qc::Controls& ctrl, TruthTable::Cube const& ctrlCube) -> void;
        static auto controlNonRoot(dd::mEdge const& current, qc::Controls& ctrl, TruthTable::Cube const& ctrlCube) -> void;

        static auto dcNodeCondition(dd::mEdge const& current) -> bool;

        auto swapPaths(dd::mEdge src, dd::mEdge const& current, TruthTable::Cube::Set const& p1SigVec, TruthTable::Cube::Set const& p2SigVec, TruthTable::Cube::Set const& p3SigVec, TruthTable::Cube::Set const& p4SigVec, std::unique_ptr<dd::Package>& dd) -> dd::mEdge;

        auto        shiftUniquePaths(dd::mEdge src, dd::mEdge const& current, TruthTable::Cube::Set const& p1SigVec, TruthTable::Cube::Set const& p2SigVec, TruthTable::Cube::Set const& p3SigVec, TruthTable::Cube::Set const& p4SigVec, bool& changePaths, std::unique_ptr<dd::Package>& dd) -> dd::mEdge;
        static auto terminate(dd::mEdge const& current, TruthTable::Cube::Set const& p1SigVec, TruthTable::Cube::Set const& p2SigVec, TruthTable::Cube::Set const& p3SigVec, TruthTable::Cube::Set const& p4SigVec) -> bool;

        auto unifyPath(dd::mEdge src, dd::mEdge const& current, TruthTable::Cube::Set const& p1SigVec, TruthTable::Cube::Set const& p2SigVec, bool const& changePaths, std::unique_ptr<dd::Package>& dd) -> dd::mEdge;
        auto shiftingPaths(dd::mEdge const& src, dd::mEdge const& current, std::unique_ptr<dd::Package>& dd) -> dd::mEdge;

        template<class T>
        auto decoder(T const& codewords) -> void;

        auto initializeSynthesizer(TruthTable const& tt) -> void;

        auto buildAndSynthesize(TruthTable const& tt) -> void;

        auto synthesizeOnePassTT(TruthTable tt) -> std::shared_ptr<qc::QuantumComputation>;

        auto synthesizeCodingTechniquesTT(TruthTable tt, bool withAdditionalLine) -> std::shared_ptr<qc::QuantumComputation>;
    };

} // namespace syrec
