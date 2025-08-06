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
            /**
             * The optional line number of the Call-/UncallStatement that called/uncalled the target module in the SyReC program.
             * Note that the parent entry of an inline stack entry stores this information since the parent did also contain the
             * Call-/UncallStatement.
             */
            std::optional<unsigned int> lineNumberOfCallOfTargetModule;
            /**
             * An optional boolean flag to determine whether the target module was called/uncalled.
             * Note that the parent entry of an inline stack entry stores this information since the parent did also contain the
             * Call-/UncallStatement.
             */
            std::optional<bool> isTargetModuleAccessedViaCallStmt;
            /**
             * The target module in which the qubit was generated.
             */
            Module::ptr targetModule;

            /**
             * Stringify the signature of the target module
             * @return The stringified signature of the target module, std::nullopt if either the target module was not set or the stringification failed.
             */
            [[nodiscard]] std::optional<std::string> stringifySignatureOfCalledModule() const;
        };

        /**
         * Push a new element onto the stack
         * @param inlineStackEntry The element to push
         * @return If the target module of the new stack entry was set (i.e. unequal to a null pointer) then true is returned, otherwise false.
         */
        [[maybe_unused]] bool push(const QubitInliningStackEntry& inlineStackEntry);

        /**
         * Pop an element from the stack
         * @return Whether an element was popped from the stack.
         */
        [[maybe_unused]] bool pop();

        /**
         * Get the size of the stack
         * @return The size of the stack
         */
        [[nodiscard]] std::size_t size() const;

        /**
         * Fetch an element from the stack using a zero-based index.
         * @param idx The index of an element on the stack
         * @return If an element of the stack at the given index exists then a reference to it is returned, otherwise nullptr is returned.
         * @remark The stack is responsible to manage the lifetime of the fetched elements, if the size of the stack changes (i.e. due to a push or pop operation) all previously fetched references to stack elements are invalid.
         */
        [[nodiscard]] QubitInliningStackEntry* getStackEntryAt(std::size_t idx);

    protected:
        std::vector<QubitInliningStackEntry> stackEntries;
    };
} // namespace syrec
