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

#include <cstdint>
#include <string_view>

namespace syrec_parser {
    enum class SemanticError : std::uint8_t {
        NoVariableMatchingIdentifier,
        NoModuleMatchingIdentifier,
        IndexOfAccessedValueForDimensionOutOfRange,
        TooManyDimensionsAccessed,
        TooFewDimensionsAccessed,
        IndexOfAccessedBitOutOfRange,
        DuplicateVariableDeclaration,
        DuplicateModuleDeclaration,
        AssignmentToReadonlyVariable,
        ExpressionEvaluationFailedDueToDivisionByZero,
        IfGuardExpressionMismatch,
        NoModuleMatchingCallSignature,
        ExpressionBitwidthMismatches,
        OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue,
        DuplicateMainModuleDefinition,
        ValueOverflowDueToNoImplicitTruncationPerformed,
        CannotCallMainModule,
        ValueOfLoopVariableNotUsableInItsInitialValueDeclaration,
        ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts,
        SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts,
        DeclaredVariableBitwidthTooLarge,
        InfiniteLoopDetected,
        UnhandledOperationFromGrammarInParser,
        VariableBitwidthEqualToZero,
        NumberOfValuesOfDimensionEqualToZero
    };

    /// Get the identifier associated with a given semantic error
    /// @tparam semanticError The semantic error
    /// @return The identifier for the semantic error or an empty string if none exists.
    template<SemanticError semanticError>
    [[nodiscard]] constexpr std::string_view getIdentifierForSemanticError() {
        // Note: This function can only be implemented as a constexpr if the semantic error is provided as a template argument and not as a function parameter.
        switch (semanticError) {
            case SemanticError::NoVariableMatchingIdentifier:
                return "SEM01";
            case SemanticError::NoModuleMatchingIdentifier:
                return "SEM02";
            case SemanticError::IndexOfAccessedValueForDimensionOutOfRange:
                return "SEM03";
            case SemanticError::TooManyDimensionsAccessed:
                return "SEM04";
            case SemanticError::TooFewDimensionsAccessed:
                return "SEM05";
            case SemanticError::IndexOfAccessedBitOutOfRange:
                return "SEM06";
            case SemanticError::DuplicateVariableDeclaration:
                return "SEM07";
            case SemanticError::DuplicateModuleDeclaration:
                return "SEM08";
            case SemanticError::AssignmentToReadonlyVariable:
                return "SEM09";
            case SemanticError::ExpressionEvaluationFailedDueToDivisionByZero:
                return "SEM10";
            case SemanticError::IfGuardExpressionMismatch:
                return "SEM11";
            case SemanticError::NoModuleMatchingCallSignature:
                return "SEM12";
            case SemanticError::ExpressionBitwidthMismatches:
                return "SEM13";
            case SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue:
                return "SEM14";
            case SemanticError::DuplicateMainModuleDefinition:
                return "SEM15";
            case SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed:
                return "SEM16";
            case SemanticError::CannotCallMainModule:
                return "SEM17";
            case SemanticError::ValueOfLoopVariableNotUsableInItsInitialValueDeclaration:
                return "SEM18";
            case SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts:
                return "SEM19";
            case SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts:
                return "SEM20";
            case SemanticError::DeclaredVariableBitwidthTooLarge:
                return "SEM21";
            case SemanticError::InfiniteLoopDetected:
                return "SEM22";
            case SemanticError::UnhandledOperationFromGrammarInParser:
                return "SEM23";
            case SemanticError::VariableBitwidthEqualToZero:
                return "SEM25";
            case SemanticError::NumberOfValuesOfDimensionEqualToZero:
                return "SEM26";
            default:
                return "";
        }
    }

    /// Get the format of the error message associated with a given semantic error.
    /// @tparam semanticError The semantic error
    /// @return The format of the error message or an empty string if none exists.
    template<SemanticError semanticError>
    [[nodiscard]] constexpr std::string_view getFormatForSemanticErrorMessage() {
        // Note: This function can only be implemented as a constexpr if the semantic error is provided as a template argument and not as a function parameter.
        // It seems that we cannot use named arguments in the format string due to them not being supported in combination with the compile time format string checks (https://github.com/fmtlib/fmt/blob/master/doc/api.md#named-arguments)
        switch (semanticError) {
            case SemanticError::NoVariableMatchingIdentifier:
                return "No variable matching identifier '{:s}'";
            case SemanticError::NoModuleMatchingIdentifier:
                return "No module matching defined identifier '{:s}'";
            case SemanticError::IndexOfAccessedValueForDimensionOutOfRange:
                return "Defined index value {:d} was out of range for dimension {:d} which was defined to contain {:d} values. Please use zero-based indices";
            case SemanticError::TooManyDimensionsAccessed:
                return "Access on N dimensional signal must be correctly specified, user specified access on {:d} dimensions while the accessed variable was declared with having {:d} dimensions";
            case SemanticError::TooFewDimensionsAccessed:
                return "Access on N dimensional signal must be fully specified, user specified access on {:d} dimensions while the accessed variable was declared with having {:d} dimensions";
            case SemanticError::IndexOfAccessedBitOutOfRange:
                return "Accessed bit {:d} was out of range, accessed variable was declared with a bitrange of {:d}. Please use zero-based indices";
            case SemanticError::DuplicateVariableDeclaration:
                return "Duplicate variable declaration sharing same identifier '{:s}'";
            case SemanticError::DuplicateModuleDeclaration:
                return "Duplicate module declaration with identifier '{:s}' sharing same signature";
            case SemanticError::AssignmentToReadonlyVariable:
                return "No assignment to readonly variable '{:s}' possible";
            case SemanticError::ExpressionEvaluationFailedDueToDivisionByZero:
                return "Expression evaluation failed due to a division by zero";
            case SemanticError::IfGuardExpressionMismatch:
                return "Guard and closing guard expression of IfStatement did not match";
            case SemanticError::NoModuleMatchingCallSignature:
                return "No module matching user provided call signature";
            case SemanticError::ExpressionBitwidthMismatches:
                return "Expected operand to have a bitwidth of {:d} while it actually had a bitwidth of {:d}";
            case SemanticError::OmittingDimensionAccessOnlyPossibleFor1DSignalWithSingleValue:
                return "Omitting explicit access on value of dimension is only possible for 1D signal containing a single value";
            case SemanticError::DuplicateMainModuleDefinition:
                return "Module with identifier 'main' can only be defined once";
            case SemanticError::ValueOverflowDueToNoImplicitTruncationPerformed:
                return "Implicit truncation of values is disabled at this point of the program thus value ({:s}) that was large than the maximum storable value of {:d} lead to an overflow";
            case SemanticError::CannotCallMainModule:
                return "Cannot call 'main' module of SyReC program";
            case SemanticError::ValueOfLoopVariableNotUsableInItsInitialValueDeclaration:
                return "Loop variable {:s} cannot be used in the definition of its initial value";
            case SemanticError::ReversibilityOfStatementNotPossibleDueToAccessOnRestrictedVariableParts:
                return "Overlap with restricted variable parts (formatted as: dimension access [shown as tuples of the form (dimensionIdx, value of dimension)] | overlapping bit) = '{:s}' detected preventing the inversion of the current statement";
            case SemanticError::SynthesisOfExpressionPotentiallyNotPossibleDueToAccessOnRestrictedVariableParts:
                return "Overlap with restricted variable parts (formatted as: dimension access [shown as tuples of the form (dimensionIdx, value of dimension)] | overlapping bit) = '{:s}' detected potentially preventing the synthesis of the current expression";
            case SemanticError::DeclaredVariableBitwidthTooLarge:
                return "Declared variable bitwidth {:d} exceeds the maximum supported variable bitwidth of {:d} bits";
            case SemanticError::InfiniteLoopDetected:
                return "Loop iterating from start value {:d} to {:d} with a stepsize of {:d} will perform an infinite number of iterations";
            case SemanticError::UnhandledOperationFromGrammarInParser:
                return "While the grammar accepted the operation '{:s}', the parser could not handle it! This is most likely an error in the latter";
            case SemanticError::VariableBitwidthEqualToZero:
                return "Variable bitwidth must be larger than zero";
            case SemanticError::NumberOfValuesOfDimensionEqualToZero:
                return "Number of values of dimension {:d} must be larger than zero";
            default:
                return "";
        }
    }
} // namespace syrec_parser
