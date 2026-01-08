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

#include <stdexcept>

namespace syrec {

    inline void trim(std::string& str) {
        str.erase(str.find_last_not_of(' ') + 1); //suffixing spaces
        str.erase(0, str.find_first_not_of(' ')); //prefixing spaces
    }

    void parsePla(TruthTable& tt, std::istream& in);

    auto extend(TruthTable& tt) -> void;

    bool readPla(TruthTable& tt, const std::string& filename);

} // namespace syrec
