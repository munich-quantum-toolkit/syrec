/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/symbolTable/base_symbol_table.hpp"

#include "core/syrec/module.hpp"
#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"
#include "core/syrec/parser/utils/variable_assignability_check.hpp"
#include "core/syrec/variable.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

bool utils::BaseSymbolTable::insertModule(const syrec::Module::ptr& module) {
    if (module == nullptr || module->name.empty() || !std::all_of(module->parameters.cbegin(), module->parameters.cend(), [](const syrec::Variable::ptr& moduleParameter) { return moduleParameter && !moduleParameter->name.empty(); })) {
        return false;
    }

    // Check whether the insertion of the module will not create any ambiguity for any future module overload resolution attempts.
    // I.e. Two modules sharing the same identifier, number of parameters (n) and structure for each of their defined parameters can only exist,
    // if no index 0 <= i < n exists where the variable types of the module_1[i]->type and module_2[i]->type allows for the assignment of a variable of arbitrary type.
    // The valid combinations for a pair of parameters from two modules are:
    // * (in, out) and vice versa
    // * (inout, in) and vice versa
    //
    const syrec::Module::vec& modulesMatchingIdentifier = getModulesByName(module->name);
    if (std::any_of(
                modulesMatchingIdentifier.cbegin(),
                modulesMatchingIdentifier.cend(),
                [&module](const syrec::Module::ptr& existingModuleMatchingIdentifier) {
                    return existingModuleMatchingIdentifier->parameters.size() == module->parameters.size() && std::equal(
                                                                                                                       existingModuleMatchingIdentifier->parameters.cbegin(),
                                                                                                                       existingModuleMatchingIdentifier->parameters.cend(),
                                                                                                                       module->parameters.cbegin(),
                                                                                                                       module->parameters.cend(),
                                                                                                                       [](const syrec::Variable::ptr& symTabModuleParameter, const syrec::Variable::ptr& userModuleParameter) {
                                                                                                                           return doVariableStructuresMatch(*symTabModuleParameter, *userModuleParameter) ? doesVariableTypePairCreateOverloadResolutionAmbiguity(symTabModuleParameter->type, userModuleParameter->type) : false;
                                                                                                                       });
                })) {
        return false;
    }

    if (declaredModules.find(module->name) == declaredModules.end()) {
        declaredModules.insert({module->name, syrec::Module::vec()});
    }
    declaredModules.at(module->name).emplace_back(module);
    return true;
}

syrec::Module::vec utils::BaseSymbolTable::getModulesByName(const std::string_view& accessedModuleIdentifier) const {
    auto modulesMatchingIdentifier = declaredModules.find(accessedModuleIdentifier);
    if (modulesMatchingIdentifier == declaredModules.end()) {
        return {};
    }
    return modulesMatchingIdentifier->second;
}

bool utils::BaseSymbolTable::existsModuleForName(const std::string_view& accessedModuleIdentifier) const {
    return !getModulesByName(accessedModuleIdentifier).empty();
}

utils::BaseSymbolTable::ModuleOverloadResolutionResult utils::BaseSymbolTable::getModulesMatchingSignature(const std::string_view& accessedModuleIdentifier, const syrec::Variable::vec& callerArguments) const {
    return getModulesMatchingSignature(accessedModuleIdentifier, callerArguments, true);
}

std::optional<utils::TemporaryVariableScope::ptr> utils::BaseSymbolTable::getActiveTemporaryScope() const {
    return temporaryVariableScopes.empty() ? std::nullopt : std::make_optional(temporaryVariableScopes.back());
}

utils::TemporaryVariableScope::ptr utils::BaseSymbolTable::openTemporaryScope() {
    temporaryVariableScopes.emplace_back(std::make_shared<utils::TemporaryVariableScope>());
    return temporaryVariableScopes.back();
}

std::optional<utils::TemporaryVariableScope::ptr> utils::BaseSymbolTable::closeTemporaryScope() {
    if (temporaryVariableScopes.empty()) {
        return std::nullopt;
    }
    temporaryVariableScopes.pop_back();
    return getActiveTemporaryScope();
}

// NON-PUBLIC FUNCTIONALITY
utils::BaseSymbolTable::ModuleOverloadResolutionResult utils::BaseSymbolTable::getModulesMatchingSignature(const std::string_view& accessedModuleIdentifier, const syrec::Variable::vec& callerArguments, bool validateCallerArguments) const {
    if (validateCallerArguments) {
        if (std::any_of(
                    callerArguments.cbegin(),
                    callerArguments.cend(),
                    [](const syrec::Variable::ptr& callerArgument) {
                        return !callerArgument || callerArgument->name.empty();
                    })) {
            return ModuleOverloadResolutionResult(ModuleOverloadResolutionResult::Result::CallerArgumentsInvalid, std::nullopt);
        }
    }

    syrec::Module::vec modulesMatchingIdentifier = getModulesByName(accessedModuleIdentifier);
    modulesMatchingIdentifier.erase(
            std::remove_if(
                    modulesMatchingIdentifier.begin(),
                    modulesMatchingIdentifier.end(),
                    [&callerArguments](const syrec::Module::ptr& moduleMatchingIdentifier) {
                        return moduleMatchingIdentifier->parameters.size() != callerArguments.size() || !std::equal(
                                                                                                                moduleMatchingIdentifier->parameters.cbegin(),
                                                                                                                moduleMatchingIdentifier->parameters.cend(),
                                                                                                                callerArguments.cbegin(),
                                                                                                                callerArguments.cend(),
                                                                                                                [](const syrec::Variable::ptr& moduleParameter, const syrec::Variable::ptr& callerArgument) {
                                                                                                                    return variable_assignability_check::doesModuleParameterTypeAllowAssignmentFromVariableType(moduleParameter->type, callerArgument->type) && doVariableStructuresMatch(*moduleParameter, *callerArgument);
                                                                                                                });
                    }),
            modulesMatchingIdentifier.end());

    if (modulesMatchingIdentifier.empty()) {
        return ModuleOverloadResolutionResult(ModuleOverloadResolutionResult::Result::NoMatchFound, std::nullopt);
    }

    return modulesMatchingIdentifier.size() == 1 ? ModuleOverloadResolutionResult(ModuleOverloadResolutionResult::SingleMatchFound, modulesMatchingIdentifier.front()) : ModuleOverloadResolutionResult(ModuleOverloadResolutionResult::MultipleMatchesFound, std::nullopt);
}

bool utils::BaseSymbolTable::doVariableStructuresMatch(const syrec::Variable& lVariable, const syrec::Variable& rVariable) noexcept {
    return lVariable.bitwidth == rVariable.bitwidth && std::equal(
                                                               lVariable.dimensions.cbegin(),
                                                               lVariable.dimensions.cend(),
                                                               rVariable.dimensions.cbegin(),
                                                               rVariable.dimensions.cend(),
                                                               [](const auto moduleParameterNumValuesOfDimension, const auto callerArgumentNumValuesOfDimension) {
                                                                   return moduleParameterNumValuesOfDimension == callerArgumentNumValuesOfDimension;
                                                               });
}
