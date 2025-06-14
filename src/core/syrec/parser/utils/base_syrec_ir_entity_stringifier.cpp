/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/base_syrec_ir_entity_stringifier.hpp"

#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/statement.hpp"
#include "core/syrec/variable.hpp"

#include <cstddef>
#include <ios>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

using namespace utils;

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Program& program) {
    resetInternals();

    if (program.modules().empty()) {
        return true;
    }

    bool       stringificationSuccessful               = true;
    const auto iteratorToFirstModuleWithNewlinePostfix = program.modules().cbegin();
    const auto iteratorToLastModuleWithNewlinePostfix  = std::prev(program.modules().cend());
    for (auto moduleIterator = iteratorToFirstModuleWithNewlinePostfix; stringificationSuccessful && moduleIterator != iteratorToLastModuleWithNewlinePostfix; ++moduleIterator) {
        stringificationSuccessful &= *moduleIterator && stringify(outputStream, **moduleIterator) && appendNewlineToStream(outputStream);
    }
    return stringificationSuccessful && program.modules().back() && stringify(outputStream, *program.modules().back());
}

// START OF NON-PUBLIC INTERFACE
void BaseSyrecIrEntityStringifier::resetInternals() {
    indentationSequence.clear();
}

bool BaseSyrecIrEntityStringifier::incrementIndentationLevel() {
    indentationSequence += additionalFormattingOptions.optionalCustomIndentationCharacterSequence.value_or(indentIdent);
    return true;
}

bool BaseSyrecIrEntityStringifier::decrementIndentationLevel() noexcept {
    if (!indentationSequence.empty()) {
        indentationSequence.pop_back();
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Module& programModule) {
    return !programModule.name.empty() && !programModule.statements.empty() && appendToStream(outputStream, moduleKeywordIdent + " " + programModule.name) && appendToStream(outputStream, '(') && stringify(outputStream, programModule.parameters, true) && appendToStream(outputStream, ')') && (!programModule.variables.empty() ? appendNewlineToStream(outputStream) : true) && incrementIndentationLevel() && stringify(outputStream, programModule.variables, !additionalFormattingOptions.omitVariableTypeSharedBySequenceOfLocalVariables) && appendNewlineToStream(outputStream) && stringify(outputStream, programModule.statements) && decrementIndentationLevel();
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Variable& variable, bool stringifyVariableType) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    bool stringificationSuccessful = stringifyVariableType ? stringify(outputStream, variable.type) && appendToStream(outputStream, ' ') : true;
    stringificationSuccessful &= !variable.name.empty() ? appendToStream(outputStream, variable.name) : false;
    stringificationSuccessful &= !variable.dimensions.empty();

    // While the user can configure whether the dimensions of a 1D signal with a single value are stringified, the user has no other option to force override this settings in a SyReC program
    // which will cause a uniform stringification of 1D signal declarations with a single value in a uniform way regardless of how the where defined in the SyReC program.
    if (variable.dimensions.size() != 1 || variable.dimensions.front() != 1 || !additionalFormattingOptions.omitNumberOfDimensionsDeclarationFor1DVariablesWithSingleValue) {
        for (const auto numValuesOfDimension: variable.dimensions) {
            stringificationSuccessful &= appendToStream(outputStream, "[" + std::to_string(numValuesOfDimension) + "]");
        }
    }
    return stringificationSuccessful && appendToStream(outputStream, "(" + std::to_string(variable.bitwidth) + ")");
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Statement::ptr& statement) {
    if (statement == nullptr || !outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    if (!appendIndentationPaddingSequence(outputStream, indentationSequence)) {
        return false;
    }

    const auto& statementInstancePointer = statement.get();
    // Since a syrec::SkipStatement is defined using an 'using syrec::SkipStatement = syrec::Statement' declaration,
    // no dynamic_cast<> check can be performed to determine whether the statement instance is a skip statement. Thus, we must
    // compare the typeid to perform said check.
    if (const auto& statementInstance = *statementInstancePointer; typeid(statementInstance) == typeid(syrec::SkipStatement)) {
        return stringifySkipStatement(outputStream);
    }

    // Conflicting information regarding the correct implementation of a down case to handle a polymorphic class have been found
    // ranging from the visitor pattern, the usage of dynamic_cast<> as well as custom RTTI implementations.
    // The Cpp guidelines "recommend" the usage of dynamic_cast<> when traversal of the class hierarchy is required at runtime (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rh-dynamic_cast)
    // An alternative approach utilizing std::visit could be in the form of: https://quuxplusone.github.io/blog/2020/09/29/oop-visit/
    // OR: https://www.oreilly.com/library/view/c-software-design/9781098113155/ch04.html (utilizing std::visit which in turn requires ownership of the object and thus seems no usable with the current function signature).
    // 16.12.2024: We currently follow the Cpp guidelines but further research needs to be done on whether a better performing solution can be found or whether the overhead incurred by the dynamic_cast<> is acceptable.
    if (const auto& stmtCastedAsAssignment = dynamic_cast<const syrec::AssignStatement*>(statementInstancePointer); stmtCastedAsAssignment != nullptr) {
        return stringify(outputStream, *stmtCastedAsAssignment);
    }
    if (const auto& stmtCastedAsModuleCall = dynamic_cast<const syrec::CallStatement*>(statementInstancePointer); stmtCastedAsModuleCall != nullptr) {
        return stringify(outputStream, *stmtCastedAsModuleCall);
    }
    if (const auto& stmtCastedAsForStmt = dynamic_cast<const syrec::ForStatement*>(statementInstancePointer); stmtCastedAsForStmt != nullptr) {
        return stringify(outputStream, *stmtCastedAsForStmt);
    }
    if (const auto& stmtCastedAsIfStmt = dynamic_cast<const syrec::IfStatement*>(statementInstancePointer); stmtCastedAsIfStmt != nullptr) {
        return stringify(outputStream, *stmtCastedAsIfStmt);
    }
    if (const auto& stmtCastedAsSwapStmt = dynamic_cast<const syrec::SwapStatement*>(statementInstancePointer); stmtCastedAsSwapStmt != nullptr) {
        return stringify(outputStream, *stmtCastedAsSwapStmt);
    }
    if (const auto& stmtCastedAsUnaryStmt = dynamic_cast<const syrec::UnaryStatement*>(statementInstancePointer); stmtCastedAsUnaryStmt != nullptr) {
        return stringify(outputStream, *stmtCastedAsUnaryStmt);
    }
    if (const auto& stmtCastedAsModuleUncall = dynamic_cast<const syrec::UncallStatement*>(statementInstancePointer); stmtCastedAsModuleUncall != nullptr) {
        return stringify(outputStream, *stmtCastedAsModuleUncall);
    }
    return false;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::AssignStatement& assignStatement) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    return assignStatement.lhs && assignStatement.rhs && stringify(outputStream, *assignStatement.lhs) && (additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? appendToStream(outputStream, " ") && stringify(outputStream, assignStatement.assignOperation) && appendToStream(outputStream, " ") : stringify(outputStream, assignStatement.assignOperation)) && stringify(outputStream, *assignStatement.rhs);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::CallStatement& callStatement) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    return callStatement.target && stringifyModuleCallVariant(outputStream, callKeywordIdent, *callStatement.target, callStatement.parameters);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::ForStatement& forStatement) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    bool stringificationSuccessful = forStatement.range.first && forStatement.range.second && forStatement.step && appendToStream(outputStream, forKeywordIdent + " ");
    if (!forStatement.loopVariable.empty()) {
        stringificationSuccessful &= appendIndentationPaddingSequence(outputStream, indentationSequence) && appendToStream(outputStream, forStatement.loopVariable) && additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? appendToStream(outputStream, " = ") : appendToStream(outputStream, '=');
    }
    return stringificationSuccessful && stringify(outputStream, *forStatement.range.first) && appendToStream(outputStream, " " + toKeywordIdent + " ") && (forStatement.range.first != forStatement.range.second ? stringify(outputStream, *forStatement.range.second) : true) && appendToStream(outputStream, " " + stepKeywordIdent + " ") && stringify(outputStream, *forStatement.step) && appendToStream(outputStream, " " + doKeywordIdent) && appendNewlineToStream(outputStream) && incrementIndentationLevel() && stringify(outputStream, forStatement.statements) && appendNewlineToStream(outputStream) && decrementIndentationLevel() && appendIndentationPaddingSequence(outputStream, indentationSequence) && appendToStream(outputStream, rofKeywordIdent);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::IfStatement& ifStatement) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return ifStatement.condition && ifStatement.fiCondition && appendToStream(outputStream, ifKeywordIdent + " ") && stringify(outputStream, *ifStatement.condition) && appendToStream(outputStream, " " + thenKeywordIdent) && appendNewlineToStream(outputStream) && incrementIndentationLevel() && stringify(outputStream, ifStatement.thenStatements) && decrementIndentationLevel() && appendNewlineToStream(outputStream) && decrementIndentationLevel() && appendIndentationPaddingSequence(outputStream, indentationSequence) && appendToStream(outputStream, elseKeywordIdent) && appendNewlineToStream(outputStream) && incrementIndentationLevel() && stringify(outputStream, ifStatement.elseStatements) && appendNewlineToStream(outputStream) && decrementIndentationLevel() && appendIndentationPaddingSequence(outputStream, indentationSequence) && appendToStream(outputStream, fiKeywordIdent + " ") && stringify(outputStream, *ifStatement.fiCondition);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::SwapStatement& swapStatement) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return swapStatement.lhs && swapStatement.rhs && stringify(outputStream, *swapStatement.lhs) && appendToStream(outputStream, additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? " " + swapOperationIdent + " " : swapOperationIdent) && stringify(outputStream, *swapStatement.rhs);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::UnaryStatement& unaryAssignStatement) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return unaryAssignStatement.var && stringify(outputStream, unaryAssignStatement.unaryOperation) && (additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? appendToStream(outputStream, " ") : true) && stringify(outputStream, *unaryAssignStatement.var);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::UncallStatement& uncallStatement) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return uncallStatement.target && stringifyModuleCallVariant(outputStream, uncallKeywordIdent, *uncallStatement.target, uncallStatement.parameters);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Expression& expression) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    if (const auto& castedToBinaryExpr = dynamic_cast<const syrec::BinaryExpression*>(&expression); castedToBinaryExpr != nullptr) {
        return stringify(outputStream, *castedToBinaryExpr);
    }
    if (const auto& castedToNumericExpr = dynamic_cast<const syrec::NumericExpression*>(&expression); castedToNumericExpr != nullptr) {
        return stringify(outputStream, *castedToNumericExpr);
    }
    if (const auto& castedToVariableExpr = dynamic_cast<const syrec::VariableExpression*>(&expression); castedToVariableExpr != nullptr) {
        return stringify(outputStream, *castedToVariableExpr);
    }
    if (const auto& castedToShiftExpr = dynamic_cast<const syrec::ShiftExpression*>(&expression); castedToShiftExpr != nullptr) {
        return stringify(outputStream, *castedToShiftExpr);
    }
    return false;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::BinaryExpression& binaryExpression) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return binaryExpression.lhs && binaryExpression.rhs && appendToStream(outputStream, '(') && stringify(outputStream, *binaryExpression.lhs) && (additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? appendToStream(outputStream, " ") && stringify(outputStream, binaryExpression.binaryOperation) && appendToStream(outputStream, " ") : stringify(outputStream, binaryExpression.binaryOperation)) && stringify(outputStream, *binaryExpression.rhs) && appendToStream(outputStream, ')');
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::NumericExpression& numericExpression) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return numericExpression.value && stringify(outputStream, *numericExpression.value);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::VariableExpression& variableExpression) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return variableExpression.var && stringify(outputStream, *variableExpression.var);
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::ShiftExpression& shiftExpression) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    return shiftExpression.lhs && shiftExpression.rhs && appendToStream(outputStream, '(') && stringify(outputStream, *shiftExpression.lhs) && (additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? appendToStream(outputStream, " ") && stringify(outputStream, shiftExpression.shiftOperation) && appendToStream(outputStream, " ") : stringify(outputStream, shiftExpression.shiftOperation)) && stringify(outputStream, *shiftExpression.rhs) && appendToStream(outputStream, ')');
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::VariableAccess& variableAccess) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    bool stringificationSuccessful = variableAccess.var && !variableAccess.var->name.empty() && (variableAccess.range.has_value() ? variableAccess.range->first && variableAccess.range->second : true) && appendToStream(outputStream, variableAccess.var->name);

    for (std::size_t i = 0; stringificationSuccessful && i < variableAccess.indexes.size(); ++i) {
        const auto& expressionDefiningAccessedValueOfDimension = variableAccess.indexes.at(i);
        stringificationSuccessful &= expressionDefiningAccessedValueOfDimension && appendToStream(outputStream, '[') && stringify(outputStream, *expressionDefiningAccessedValueOfDimension) && appendToStream(outputStream, ']');
    }

    if (stringificationSuccessful && variableAccess.range.has_value()) {
        stringificationSuccessful &= appendToStream(outputStream, bitRangeStartIdent) && stringify(outputStream, *variableAccess.range->first) && (variableAccess.range->first != variableAccess.range->second ? appendToStream(outputStream, bitRangeEndIdent) && stringify(outputStream, *variableAccess.range->second) : true);
    }
    return stringificationSuccessful;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Number& number) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }
    if (number.isConstant()) {
        return appendToStream(outputStream, std::to_string(number.evaluate({})));
    }
    if (number.isLoopVariable()) {
        return appendToStream(outputStream, number.variableName());
    }
    if (number.isConstantExpression()) {
        const std::optional<syrec::Number::ConstantExpression>& constantExpressionData = number.constantExpression();
        if (!constantExpressionData.has_value()) {
            return setStreamInFailedState(outputStream);
        }
        return constantExpressionData->lhsOperand && constantExpressionData->rhsOperand && appendToStream(outputStream, '(') && stringify(outputStream, *constantExpressionData->lhsOperand) && (additionalFormattingOptions.useWhitespaceBetweenOperandsOfBinaryOperation ? appendToStream(outputStream, " ") && stringify(outputStream, constantExpressionData->operation) && appendToStream(outputStream, " ") : stringify(outputStream, constantExpressionData->operation)) && stringify(outputStream, *constantExpressionData->rhsOperand) && appendToStream(outputStream, ')');
    }
    return false;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Variable::vec& variables, bool stringifyVariableTypeForEveryEntry) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    if (variables.empty()) {
        return true;
    }

    bool              stringificationSuccessful = !stringifyVariableTypeForEveryEntry ? appendIndentationPaddingSequence(outputStream, indentationSequence) : true;
    const std::size_t numModulesParameters      = variables.size();

    if (stringifyVariableTypeForEveryEntry) {
        for (std::size_t i = 0; stringificationSuccessful && i < numModulesParameters; ++i) {
            const auto& variable = variables.at(i);
            stringificationSuccessful &= variable != nullptr && (i != 0 ? appendToStream(outputStream, additionalFormattingOptions.useWhitespaceAfterAfterModuleParameterDeclaration ? ", " : ",") : true) && stringify(outputStream, *variable, true);
        }
    } else {
        for (std::size_t i = 0; stringificationSuccessful && i < numModulesParameters; ++i) {
            const auto& variable = variables.at(i);
            stringificationSuccessful &= variable != nullptr;
            const bool variableTypeGroupChange = i > 0 && (variable != nullptr && variables.at(i - 1) != nullptr && variable->type != variables.at(i - 1)->type);
            if (variableTypeGroupChange) {
                stringificationSuccessful &= appendNewlineToStream(outputStream) && appendIndentationPaddingSequence(outputStream, indentationSequence);
            } else {
                stringificationSuccessful &= i == 0 || appendToStream(outputStream, additionalFormattingOptions.useWhitespaceAfterAfterModuleParameterDeclaration ? ", " : ",");
            }
            stringificationSuccessful &= stringify(outputStream, *variable, i == 0 || variableTypeGroupChange);
        }
    }
    return stringificationSuccessful;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, const syrec::Statement::vec& statements) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    if (statements.empty()) {
        return false;
    }

    bool        stringificationSuccessful          = true;
    const auto& firstStatementWithSemicolonPostfix = statements.cbegin();
    const auto& lastStatementWithSemicolonPostfix  = std::prev(statements.cend());
    for (auto statementIterator = firstStatementWithSemicolonPostfix; stringificationSuccessful && statementIterator != lastStatementWithSemicolonPostfix; ++statementIterator) {
        stringificationSuccessful &= *statementIterator && appendIndentationPaddingSequence(outputStream, indentationSequence) && stringify(outputStream, *statementIterator) && appendToStream(outputStream, ';') && appendNewlineToStream(outputStream);
    }

    stringificationSuccessful &= statements.back() && appendIndentationPaddingSequence(outputStream, indentationSequence) && stringify(outputStream, statements.back());

    return stringificationSuccessful;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, syrec::Variable::Type variableType) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    switch (variableType) {
        case syrec::Variable::Type::In:
            outputStream << inVariableTypeIdent;
            break;
        case syrec::Variable::Type::Out:
            outputStream << outVariableTypeIdent;
            break;
        case syrec::Variable::Type::Inout:
            outputStream << inoutVariableTypeIdent;
            break;
        case syrec::Variable::Type::Wire:
            outputStream << wireVariableTypeIdent;
            break;
        case syrec::Variable::Type::State:
            outputStream << stateVariableTypeIdent;
            break;
        default:
            return setStreamInFailedState(outputStream);
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, syrec::BinaryExpression::BinaryOperation operation) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    switch (operation) {
        case syrec::BinaryExpression::BinaryOperation::Add:
            outputStream << "+";
            break;
        case syrec::BinaryExpression::BinaryOperation::BitwiseAnd:
            outputStream << "&";
            break;
        case syrec::BinaryExpression::BinaryOperation::BitwiseOr:
            outputStream << "|";
            break;
        case syrec::BinaryExpression::BinaryOperation::Divide:
            outputStream << "/";
            break;
        case syrec::BinaryExpression::BinaryOperation::Equals:
            outputStream << "=";
            break;
        case syrec::BinaryExpression::BinaryOperation::Exor:
            outputStream << "^";
            break;
        case syrec::BinaryExpression::BinaryOperation::FracDivide:
            outputStream << "*>";
            break;
        case syrec::BinaryExpression::BinaryOperation::GreaterEquals:
            outputStream << ">=";
            break;
        case syrec::BinaryExpression::BinaryOperation::GreaterThan:
            outputStream << ">";
            break;
        case syrec::BinaryExpression::BinaryOperation::LessEquals:
            outputStream << "<=";
            break;
        case syrec::BinaryExpression::BinaryOperation::LessThan:
            outputStream << "<";
            break;
        case syrec::BinaryExpression::BinaryOperation::LogicalAnd:
            outputStream << "&&";
            break;
        case syrec::BinaryExpression::BinaryOperation::LogicalOr:
            outputStream << "||";
            break;
        case syrec::BinaryExpression::BinaryOperation::Modulo:
            outputStream << "%";
            break;
        case syrec::BinaryExpression::BinaryOperation::Multiply:
            outputStream << "*";
            break;
        case syrec::BinaryExpression::BinaryOperation::NotEquals:
            outputStream << "!=";
            break;
        case syrec::BinaryExpression::BinaryOperation::Subtract:
            outputStream << "-";
            break;
        default:
            return setStreamInFailedState(outputStream);
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, syrec::UnaryStatement::UnaryOperation operation) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    switch (operation) {
        case syrec::UnaryStatement::UnaryOperation::Increment:
            outputStream << "++=";
            break;
        case syrec::UnaryStatement::UnaryOperation::Decrement:
            outputStream << "--=";
            break;
        case syrec::UnaryStatement::UnaryOperation::Invert:
            outputStream << "~=";
            break;
        default:
            return setStreamInFailedState(outputStream);
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, syrec::AssignStatement::AssignOperation operation) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    switch (operation) {
        case syrec::AssignStatement::AssignOperation::Add:
            outputStream << "+=";
            break;
        case syrec::AssignStatement::AssignOperation::Subtract:
            outputStream << "-=";
            break;
        case syrec::AssignStatement::AssignOperation::Exor:
            outputStream << "^=";
            break;
        default:
            return setStreamInFailedState(outputStream);
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, syrec::ShiftExpression::ShiftOperation operation) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    switch (operation) {
        case syrec::ShiftExpression::ShiftOperation::Left:
            outputStream << "<<";
            break;
        case syrec::ShiftExpression::ShiftOperation::Right:
            outputStream << ">>";
            break;
        default:
            return setStreamInFailedState(outputStream);
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringify(std::ostream& outputStream, syrec::Number::ConstantExpression::Operation operation) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    switch (operation) {
        case syrec::Number::ConstantExpression::Operation::Addition:
            outputStream << "+";
            break;
        case syrec::Number::ConstantExpression::Operation::Subtraction:
            outputStream << "-";
            break;
        case syrec::Number::ConstantExpression::Operation::Multiplication:
            outputStream << "*";
            break;
        case syrec::Number::ConstantExpression::Operation::Division:
            outputStream << "/";
            break;
        default:
            return setStreamInFailedState(outputStream);
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::stringifySkipStatement(std::ostream& outputStream) const {
    if (!outputStream.good() || skipKeywordIdent.empty()) {
        return setStreamInFailedState(outputStream);
    }

    outputStream << skipKeywordIdent;
    return true;
}

bool BaseSyrecIrEntityStringifier::appendNewlineToStream(std::ostream& outputStream) const {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    if (additionalFormattingOptions.optionalCustomNewlineCharacterSequence.has_value()) {
        outputStream << *additionalFormattingOptions.optionalCustomNewlineCharacterSequence;
    } else {
#if _WIN32
        outputStream << "\r\n";
#else
        outputStream << '\n';
#endif
    }
    return true;
}

bool BaseSyrecIrEntityStringifier::setStreamInFailedState(std::ostream& stream) {
    if (!stream.bad()) {
        stream.setstate(std::ios_base::failbit);
    }
    return false;
}

bool BaseSyrecIrEntityStringifier::appendIndentationPaddingSequence(std::ostream& outputStream, const std::string& indentationSequence) {
    return appendToStream(outputStream, indentationSequence);
}

bool BaseSyrecIrEntityStringifier::appendToStream(std::ostream& outputStream, const std::string& characterSequence) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    outputStream << characterSequence;
    return outputStream.good();
}

bool BaseSyrecIrEntityStringifier::appendToStream(std::ostream& outputStream, char character) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    outputStream << character;
    return outputStream.good();
}

bool BaseSyrecIrEntityStringifier::stringifyModuleCallVariant(std::ostream& outputStream, const std::string& moduleCallVariantKeyword, const syrec::Module& callTarget, const std::vector<std::string>& callerArguments) {
    if (!outputStream.good()) {
        return setStreamInFailedState(outputStream);
    }

    if (callTarget.name.empty() || callerArguments.size() != callTarget.parameters.size()) {
        return false;
    }

    if (callerArguments.empty()) {
        return appendToStream(outputStream, moduleCallVariantKeyword + " " + callTarget.name + "()");
    }

    bool        stringificationSuccessful      = appendToStream(outputStream, moduleCallVariantKeyword + " " + callTarget.name + "(");
    const auto& firstParameterWithCommaPostfix = callerArguments.cbegin();
    const auto& lastParameterWithCommaPostfix  = std::prev(callerArguments.cend());

    for (auto parameterIterator = firstParameterWithCommaPostfix; parameterIterator != lastParameterWithCommaPostfix && stringificationSuccessful; ++parameterIterator) {
        stringificationSuccessful &= !parameterIterator->empty() && appendToStream(outputStream, *parameterIterator + ", ");
    }

    stringificationSuccessful &= !callerArguments.back().empty() && appendToStream(outputStream, callerArguments.back());
    stringificationSuccessful &= appendToStream(outputStream, ')');
    return stringificationSuccessful;
}
