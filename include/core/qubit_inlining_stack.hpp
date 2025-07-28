/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "core/syrec/module.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace syrec {
    class QubitInliningStack {
    public:
        using ptr = std::shared_ptr<QubitInliningStack>;

        struct QubitInliningStackEntry {
            std::optional<unsigned int> lineNumberOfCallOfTargetModule;
            std::optional<bool>         isTargetModuleAccessedViaCallStmt;
            Module::ptr                 targetModule;

            [[nodiscard]] std::optional<std::string> stringifySignatureOfCalledModule() const;
        };

        [[maybe_unused]] bool                                   push(const QubitInliningStackEntry& inlineStackEntry);
        [[maybe_unused]] bool                                   pop();
        [[maybe_unused]] std::size_t                            size() const;
        [[maybe_unused]] std::optional<QubitInliningStackEntry> getStackEntryAt(std::size_t idx) const;

    protected:
        std::vector<QubitInliningStackEntry> stackEntries;
    };
} // namespace syrec
