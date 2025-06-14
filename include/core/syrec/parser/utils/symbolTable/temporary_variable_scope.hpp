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

#include "core/syrec/number.hpp"
#include "core/syrec/variable.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

namespace utils {
    class TemporaryVariableScope {
    public:
        using ptr = std::shared_ptr<TemporaryVariableScope>;
        class ScopeEntry {
        public:
            using ptr         = std::shared_ptr<ScopeEntry>;
            using readOnlyPtr = std::shared_ptr<const ScopeEntry>;

            explicit ScopeEntry(syrec::Variable::ptr signalData):
                data(signalData) {}

            explicit ScopeEntry(syrec::Number::ptr loopVariable):
                data(loopVariable) {}

            [[nodiscard]] bool                                isReferenceToLoopVariable() const;
            [[nodiscard]] std::string                         getVariableIdentifier() const;
            [[nodiscard]] std::vector<unsigned int>           getDeclaredVariableDimensions() const;
            [[nodiscard]] std::optional<unsigned int>         getDeclaredVariableBitwidth() const;
            [[nodiscard]] std::optional<syrec::Variable::ptr> getVariableData() const;
            [[nodiscard]] std::optional<syrec::Number::ptr>   getLoopVariableData() const;

        protected:
            std::variant<syrec::Variable::ptr, syrec::Number::ptr> data;
        };

        [[nodiscard]] bool                                   existsVariableForName(const std::string_view& signalIdentifier) const;
        [[nodiscard]] std::optional<ScopeEntry::readOnlyPtr> getVariableByName(const std::string_view& signalIdentifier) const;
        [[nodiscard]] std::vector<ScopeEntry::readOnlyPtr>   getVariablesMatchingType(const std::unordered_set<syrec::Variable::Type>& lookedForVariableTypes) const;
        [[maybe_unused]] bool                                recordVariable(const syrec::Variable::ptr& signal);
        [[maybe_unused]] bool                                recordLoopVariable(const syrec::Number::ptr& loopVariable);
        [[maybe_unused]] bool                                removeVariable(const std::string_view& signalIdentifier);
        [[maybe_unused]] bool                                updateValueOfLoopVariable(const std::string_view& loopVariableIdentifier, const std::optional<unsigned int>& newValue);
        [[nodiscard]] std::optional<unsigned int>            getValueOfLoopVariable(const std::string_view& loopVariableIdentifier);

    protected:
        // To be able to perform heterogeneous lookup using std::string_view in a STL set/dictionary container in C++17 only std::map and std::set can be used. C++20 supports this functionality also for the unordered STL container variants.
        std::map<std::string, ScopeEntry::ptr, std::less<>> signalIdentifierLookup;
        std::map<std::string, unsigned int, std::less<>>    knownLoopVariableValues;
    };
} //namespace utils
