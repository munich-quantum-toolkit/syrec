/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/if_statement_expression_components_recorder.hpp"

#include <optional>

using namespace utils;

void IfStatementExpressionComponentsRecorder::recordExpressionComponent(const ExpressionComponent& expressionComponent) {
    if (operationMode == OperationMode::Recording) {
        expressionComponents.emplace_back(expressionComponent);
        return;
    }
    if (operationMode != OperationMode::Comparing) {
        aggregateOfComparisonResultsOfExpressionComponents = std::nullopt;
        return;
    }

    if (aggregateOfComparisonResultsOfExpressionComponents.has_value() && !aggregateOfComparisonResultsOfExpressionComponents.value()) {
        return;
    }

    if (indexForComparisonOfExpressionComponents < expressionComponents.size()) {
        const ExpressionComponent& expectedExpressionComponentAtIndex = expressionComponents.at(indexForComparisonOfExpressionComponents++);
        aggregateOfComparisonResultsOfExpressionComponents            = expectedExpressionComponentAtIndex == expressionComponent;
    } else {
        aggregateOfComparisonResultsOfExpressionComponents = false;
    }
}

void IfStatementExpressionComponentsRecorder::switchMode(OperationMode newOperationMode) {
    operationMode = newOperationMode;
}

std::optional<bool> IfStatementExpressionComponentsRecorder::recordedMatchingExpressionComponents() const {
    return aggregateOfComparisonResultsOfExpressionComponents;
}
