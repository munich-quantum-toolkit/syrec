/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/symbolTable/temporary_variable_scope.hpp"

#include "core/syrec/number.hpp"
#include "core/syrec/variable.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

bool utils::TemporaryVariableScope::ScopeEntry::isReferenceToLoopVariable() const {
    return getLoopVariableData().value_or(nullptr) != nullptr;
}

std::string utils::TemporaryVariableScope::ScopeEntry::getVariableIdentifier() const {
    if (const std::optional<std::shared_ptr<const syrec::Variable>>& entryAsVariable = getVariableData(); entryAsVariable.has_value() && *entryAsVariable != nullptr) {
        return entryAsVariable->get()->name;
    }
    if (const std::optional<std::shared_ptr<const syrec::Number>>& entryAsNumber = getLoopVariableData(); entryAsNumber.has_value() && *entryAsNumber != nullptr) {
        return entryAsNumber->get()->variableName();
    }
    return "";
}

std::vector<unsigned> utils::TemporaryVariableScope::ScopeEntry::getDeclaredVariableDimensions() const {
    if (const std::optional<std::shared_ptr<const syrec::Variable>>& entryAsVariable = getVariableData(); entryAsVariable.has_value() && *entryAsVariable != nullptr) {
        return entryAsVariable->get()->dimensions;
    }
    return {1};
}

std::optional<unsigned> utils::TemporaryVariableScope::ScopeEntry::getDeclaredVariableBitwidth() const {
    if (const std::optional<std::shared_ptr<const syrec::Variable>>& entryAsVariable = getVariableData(); entryAsVariable.has_value() && *entryAsVariable != nullptr) {
        return entryAsVariable->get()->bitwidth;
    }
    return std::nullopt;
}

std::optional<syrec::Variable::ptr> utils::TemporaryVariableScope::ScopeEntry::getVariableData() const {
    if (std::holds_alternative<syrec::Variable::ptr>(data)) {
        return std::get<syrec::Variable::ptr>(data);
    }
    return std::nullopt;
}

std::optional<syrec::Number::ptr> utils::TemporaryVariableScope::ScopeEntry::getLoopVariableData() const {
    if (std::holds_alternative<syrec::Number::ptr>(data)) {
        return std::get<syrec::Number::ptr>(data);
    }
    return std::nullopt;
}

bool utils::TemporaryVariableScope::existsVariableForName(const std::string_view& signalIdentifier) const {
    return getVariableByName(signalIdentifier).has_value();
}

std::optional<utils::TemporaryVariableScope::ScopeEntry::readOnlyPtr> utils::TemporaryVariableScope::getVariableByName(const std::string_view& signalIdentifier) const {
    const auto scopeEntryMatchingSignalIdentifier = !signalIdentifier.empty() ? signalIdentifierLookup.find(signalIdentifier) : signalIdentifierLookup.end();
    if (scopeEntryMatchingSignalIdentifier == signalIdentifierLookup.end()) {
        return std::nullopt;
    }
    return scopeEntryMatchingSignalIdentifier->second;
}

std::vector<utils::TemporaryVariableScope::ScopeEntry::readOnlyPtr> utils::TemporaryVariableScope::getVariablesMatchingType(const std::unordered_set<syrec::Variable::Type>& lookedForVariableTypes) const {
    if (lookedForVariableTypes.empty()) {
        return {};
    }

    std::vector<ScopeEntry::readOnlyPtr> variablesMatchingType;

    auto       signalsIterator   = signalIdentifierLookup.begin();
    const auto endSignalIterator = signalIdentifierLookup.end();
    while (signalsIterator != endSignalIterator) {
        if (const std::shared_ptr<const syrec::Variable> variableData = signalsIterator->second->getVariableData().value_or(nullptr);
            variableData != nullptr && lookedForVariableTypes.count(variableData->type) != 0) {
            variablesMatchingType.emplace_back(signalsIterator->second);
        }
        ++signalsIterator;
    }
    return variablesMatchingType;
}

bool utils::TemporaryVariableScope::recordVariable(const syrec::Variable::ptr& signal) {
    if (signal == nullptr || signal->name.empty() || existsVariableForName(signal->name)) {
        return false;
    }
    auto scopeEntryForSignal = std::make_shared<ScopeEntry>(signal);
    signalIdentifierLookup.insert({signal->name, scopeEntryForSignal});
    return true;
}

bool utils::TemporaryVariableScope::recordLoopVariable(const syrec::Number::ptr& loopVariable) {
    if (loopVariable == nullptr || !loopVariable->isLoopVariable() || loopVariable->variableName().empty() || loopVariable->variableName().front() != '$' || existsVariableForName(loopVariable->variableName())) {
        return false;
    }
    auto scopeEntryForSignal = std::make_shared<ScopeEntry>(loopVariable);
    signalIdentifierLookup.insert({loopVariable->variableName(), scopeEntryForSignal});
    return true;
}

bool utils::TemporaryVariableScope::removeVariable(const std::string_view& signalIdentifier) {
    const auto scopeEntryMatchingSignalIdentifier = !signalIdentifier.empty() ? signalIdentifierLookup.find(signalIdentifier) : signalIdentifierLookup.end();
    if (scopeEntryMatchingSignalIdentifier == signalIdentifierLookup.end()) {
        return false;
    }
    signalIdentifierLookup.erase(scopeEntryMatchingSignalIdentifier);
    if (const auto& entryForVariableInKnownLoopVariableValueLookup = knownLoopVariableValues.find(signalIdentifier); entryForVariableInKnownLoopVariableValueLookup != knownLoopVariableValues.end()) {
        knownLoopVariableValues.erase(entryForVariableInKnownLoopVariableValueLookup);
    }
    return true;
}

bool utils::TemporaryVariableScope::updateValueOfLoopVariable(const std::string_view& loopVariableIdentifier, const std::optional<unsigned int>& newValue) {
    if (signalIdentifierLookup.count(loopVariableIdentifier) == 0) {
        return false;
    }

    if (newValue.has_value()) {
        knownLoopVariableValues.insert_or_assign(std::string(loopVariableIdentifier), *newValue);
        return true;
    }
    if (const auto& entryForLoopVariable = knownLoopVariableValues.find(loopVariableIdentifier); entryForLoopVariable != knownLoopVariableValues.end()) {
        knownLoopVariableValues.erase(entryForLoopVariable);
        return true;
    }
    return false;
}

std::optional<unsigned> utils::TemporaryVariableScope::getValueOfLoopVariable(const std::string_view& loopVariableIdentifier) {
    if (signalIdentifierLookup.count(loopVariableIdentifier) == 0 || knownLoopVariableValues.count(loopVariableIdentifier) == 0) {
        return std::nullopt;
    }

    if (const auto& entryForLoopVariable = knownLoopVariableValues.find(loopVariableIdentifier); entryForLoopVariable != knownLoopVariableValues.end()) {
        return knownLoopVariableValues.find(loopVariableIdentifier)->second;
    }
    return std::nullopt;
}
