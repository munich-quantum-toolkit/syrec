/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/qubit_inlining_stack.hpp"

#include "core/syrec/module.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace syrec;

namespace {
    [[nodiscard]] std::optional<std::string> stringifiyModuleParameterType(Variable::Type parameterType) {
        switch (parameterType) {
            case Variable::Type::In:
                return "in";
            case Variable::Type::Out:
                return "out";
            case Variable::Type::Inout:
                return "inout";
            default:
                return std::nullopt;
        }
    }

    [[nodiscard]] bool stringifyModuleParameter(const Variable& moduleParameter, std::ostringstream& outputStream) {
        const auto& stringifiedModuleParameterType = stringifiyModuleParameterType(moduleParameter.type);
        if (!stringifiedModuleParameterType.has_value()) {
            return false;
        }
        outputStream << *stringifiedModuleParameterType << " " << moduleParameter.name;
        for (const auto& valuesOfDimension: moduleParameter.dimensions) {
            outputStream << "[" << std::to_string(valuesOfDimension) << "]";
        }
        outputStream << "(" << std::to_string(moduleParameter.bitwidth) << ")";
        return true;
    }
} // namespace

std::optional<std::string> QubitInliningStack::QubitInliningStackEntry::stringifySignatureOfCalledModule() const {
    // TODO: Is ostringstream the correct choice here?
    std::ostringstream stringifiedCalledModuleSignature;
    stringifiedCalledModuleSignature << "module " << targetModule->name << "(";

    bool stringificationSuccessful = true;
    for (std::size_t i = 0; i < targetModule->parameters.size() && stringificationSuccessful; ++i) {
        stringifiedCalledModuleSignature << (i != 0 ? ", " : "");
        stringificationSuccessful = targetModule->parameters.at(i) != nullptr && stringifyModuleParameter(*targetModule->parameters.at(i), stringifiedCalledModuleSignature);
    }
    stringifiedCalledModuleSignature << ")";

    if (!stringificationSuccessful) {
        return std::nullopt;
    }
    return stringifiedCalledModuleSignature.str();
}

bool QubitInliningStack::push(const QubitInliningStackEntry& inlineStackEntry) {
    if (inlineStackEntry.targetModule == nullptr) {
        return false;
    }
    stackEntries.push_back(inlineStackEntry);
    return true;
}

bool QubitInliningStack::pop() {
    if (!stackEntries.empty()) {
        stackEntries.pop_back();
        return true;
    }
    return false;
}

std::size_t QubitInliningStack::size() const {
    return stackEntries.size();
}

QubitInliningStack::QubitInliningStackEntry* QubitInliningStack::getStackEntryAt(std::size_t idx) {
    return idx < size() ? &stackEntries[idx] : nullptr;
}
