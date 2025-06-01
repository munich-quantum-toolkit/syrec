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
#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"
#include "core/syrec/variable.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace utils {
    class BaseSymbolTable {
    public:
        struct ModuleOverloadResolutionResult {
            enum Result : std::uint8_t {
                CallerArgumentsInvalid,
                SingleMatchFound,
                MultipleMatchesFound,
                NoMatchFound
            };

            explicit ModuleOverloadResolutionResult(Result resolutionResult, const std::optional<std::shared_ptr<syrec::Module>>& moduleMatchingSignature):
                resolutionResult(resolutionResult), moduleMatchingSignature(moduleMatchingSignature) {}

            Result                                        resolutionResult;
            std::optional<std::shared_ptr<syrec::Module>> moduleMatchingSignature;
        };

        [[maybe_unused]] bool                        insertModule(const syrec::Module::ptr& module);
        [[nodiscard]] syrec::Module::vec             getModulesByName(const std::string_view& accessedModuleIdentifier) const;
        [[nodiscard]] bool                           existsModuleForName(const std::string_view& accessedModuleIdentifier) const;
        [[nodiscard]] ModuleOverloadResolutionResult getModulesMatchingSignature(const std::string_view& accessedModuleIdentifier, const syrec::Variable::vec& callerArguments) const;

        [[nodiscard]] std::optional<TemporaryVariableScope::ptr>    getActiveTemporaryScope() const;
        [[maybe_unused]] TemporaryVariableScope::ptr                openTemporaryScope();
        [[maybe_unused]] std::optional<TemporaryVariableScope::ptr> closeTemporaryScope();

    protected:
        // To be able to perform heterogeneous lookup using std::string_view in a STL set/dictionary container in C++17 only std::map and std::set can be used. C++20 supports this functionality also for the unordered STL container variants.
        std::map<std::string, syrec::Module::vec, std::less<>> declaredModules;
        std::vector<TemporaryVariableScope::ptr>               temporaryVariableScopes;

        [[nodiscard]] ModuleOverloadResolutionResult getModulesMatchingSignature(const std::string_view& accessedModuleIdentifier, const syrec::Variable::vec& callerArguments, bool validateCallerArguments) const;
        [[nodiscard]] constexpr static bool          doesVariableTypePairCreateOverloadResolutionAmbiguity(syrec::Variable::Type lType, syrec::Variable::Type rType) {
            switch (lType) {
                case syrec::Variable::Type::In:
                    return rType != syrec::Variable::Type::Out;
                case syrec::Variable::Type::Out:
                case syrec::Variable::Type::Inout:
                    return rType != syrec::Variable::Type::In;
                default:
                    return true;
            }
        }
        [[nodiscard]] static bool doVariableStructuresMatch(const syrec::Variable& lVariable, const syrec::Variable& rVariable) noexcept;
    };
} // namespace utils
