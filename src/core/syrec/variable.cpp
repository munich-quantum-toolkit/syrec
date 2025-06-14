/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/variable.hpp"

#include "core/syrec/number.hpp"

#include <cassert>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace syrec {

    Variable::Variable(const Type type, std::string name, std::vector<unsigned> dimensions, const unsigned bitwidth):
        type(type),
        name(std::move(name)),
        dimensions(std::move(dimensions)),
        bitwidth(bitwidth) {}

    void Variable::setReference(Variable::ptr updatedReference) {
        reference = std::move(updatedReference);
    }

    void VariableAccess::setVar(Variable::ptr v) {
        var = std::move(v);
    }

    Variable::ptr VariableAccess::getVar() const {
        if (var != nullptr && var->reference) {
            return var->reference;
        }
        return var;
    }

    unsigned VariableAccess::bitwidth() const {
        if (range) {
            auto [first, second] = *range;

            /* if both variables are loop variables but have the same name,
           then the bit-width is 1, otherwise we cannot determine it now. */
            if (first->isLoopVariable() && second->isLoopVariable()) {
                if (first->variableName() == second->variableName()) {
                    return 1U;
                }
                assert(false);
            }
            Number::LoopVariableMapping const map; // empty map
            return static_cast<unsigned>(std::abs(static_cast<int>(first->evaluate(map) - second->evaluate(map)))) + 1U;
        }
        return var->bitwidth;
    }
} // namespace syrec
