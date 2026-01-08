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

namespace syrec {
    /**
     * An object to store collected statistics during parsing/synthesis.
     */
    struct Statistics {
        /**
         * The measured runtime in milliseconds.
         */
        double runtimeInMilliseconds = 0;
    };
} // namespace syrec
