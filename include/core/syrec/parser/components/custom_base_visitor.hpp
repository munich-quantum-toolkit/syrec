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

#include "Token.h"
#include "core/syrec/expression.hpp"
#include "core/syrec/number.hpp"
#include "core/syrec/parser/utils/custom_error_messages.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/parser/utils/symbolTable/base_symbol_table.hpp"
#include "core/syrec/program.hpp"
#include "core/syrec/variable.hpp"

#include <charconv>
#include <cstdlib>
#include <fmt/format.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace syrec_parser {
    /**
     * The base class containing data structures and utility functions required in more specialized visitors.
     *
     * Note that this class does not derive from the TSyrecParserVisitor class, defining the potential visitor functions for the SyReC grammar, since we are providing
     * type-safe overloads for the visitor functions instead of relying on complex conversion from the std::any type to the expected return type of the visitor function.
     * The problem with the std::any type is that the user must know the exact type that is stored in the value of the std::any to be able to access it. This std::any_cast<T>
     * operation does not support polymorphism and other convenient behaviour that one can use std::optional<T>. Additionally, we can avoid the dynamic dispatch mechanism to
     * determine the correct visitor function (while still requiring dynamic_cast cascades) which now requires for future extensions of the grammar that the developer correctly
     * defines the handling of these new types in the visitors (instead of relying on the dynamic dispatch mechanism to determine which visitor function overload to call).
     *
     * An example:
     * struct Base { virtual std::string getName() = 0; };
     * struct Derived : Base { std::string getName() { return "Derived"; };
     *
     * std::any polymorphicReturn(int i) {
     *      if (i > 1)
     *          return std::make_shared<Base>();
     *      else if (!i)
     *          return std::make_shared<Derived>();
     *      return std::nullopt;
     * }
     *
     * void usePolymorphicReturn(){
     *      // Throws a std::bad_cast exception since the type stored in the std::any is std::shared_ptr<Derived>;
     *      const std::optional<std::shared_ptr<Base>> x = std::any_cast<std::shared_ptr<Base>>(polymorphicReturn(0);
     *      // Throws a std::bad_cast exception since the type stored in the std::any is std::shared_ptr<Derived>;
     *      const std::optional<std::shared_ptr<Derived>> x = std::any_cast<std::shared_ptr<Base>>(polymorphicReturn(0);
     * }
     */
    class CustomBaseVisitor {
    public:
        CustomBaseVisitor(const std::shared_ptr<ParserMessagesContainer>& sharedGeneratedMessageContainerInstance, const std::shared_ptr<utils::BaseSymbolTable>& sharedSymbolTableInstance, const syrec::ReadProgramSettings& parserConfiguration):
            sharedGeneratedMessageContainerInstance(sharedGeneratedMessageContainerInstance), symbolTable(sharedSymbolTableInstance), parserConfiguration(parserConfiguration) {}

    protected:
        static constexpr unsigned int DEFAULT_EXPRESSION_BITWIDTH   = 32;
        static constexpr unsigned int MAX_SUPPORTED_SIGNAL_BITWIDTH = 32;

        std::shared_ptr<ParserMessagesContainer> sharedGeneratedMessageContainerInstance;
        std::shared_ptr<utils::BaseSymbolTable>  symbolTable;
        syrec::ReadProgramSettings               parserConfiguration;

        [[nodiscard]] static Message::Position mapTokenPositionToMessagePosition(const antlr4::Token& token) {
            return Message::Position(token.getLine(), token.getCharPositionInLine());
        }

        [[nodiscard]] static std::optional<unsigned int> deserializeConstantFromString(const std::string_view& stringifiedConstantValue, bool* didDeserializationFailDueToOverflow) {
            std::string_view viewOfStringifiedConstantValue = stringifiedConstantValue;
            // Trim leading and trailing whitespaces from given std::string prior to the actual deserialization call
            const std::size_t numLeadingWhitespaces = viewOfStringifiedConstantValue.find_first_not_of(' ');
            viewOfStringifiedConstantValue.remove_prefix(numLeadingWhitespaces != std::string::npos ? numLeadingWhitespaces : 0);

            const std::size_t numTrailingWhitespaces = viewOfStringifiedConstantValue.find_last_not_of(' ');
            viewOfStringifiedConstantValue.remove_suffix(viewOfStringifiedConstantValue.size() - (numTrailingWhitespaces != std::string::npos ? (numTrailingWhitespaces + 1) : viewOfStringifiedConstantValue.size()));

            unsigned int constantValue = 0;
            // Instead of using std::stroul to deserialize an integer from a string (which requires a null-terminated string) we use the C++17 std::from_chars call usable with a std::string_view input and better error handling
            // in case of overflows or non-numeric characters being included in the input string
            auto [pointerToLastNonNumericCharacterInString, errorCode] = std::from_chars(viewOfStringifiedConstantValue.data(), viewOfStringifiedConstantValue.data() + viewOfStringifiedConstantValue.size(), constantValue);
            if (errorCode == std::errc::result_out_of_range || errorCode == std::errc::invalid_argument) {
                if (didDeserializationFailDueToOverflow != nullptr && errorCode == std::errc::result_out_of_range) {
                    *didDeserializationFailDueToOverflow = true;
                }
                return std::nullopt;
                // Check whether the whole string was processed by std::from_chars by checking whether the returned out pointer is equal to the end of the processed.
                // Otherwise, the provided input string contained non-numeric character (i.e. '123abc')
            }
            if (errorCode == std::errc() && pointerToLastNonNumericCharacterInString == (viewOfStringifiedConstantValue.data() + viewOfStringifiedConstantValue.size())) {
                return constantValue;
            }
            return std::nullopt;
        }

        [[nodiscard]] static std::optional<unsigned int> tryGetConstantValueOf(const syrec::Expression& expression) {
            if (const auto& expressionAsNumericOne = dynamic_cast<const syrec::NumericExpression*>(&expression); expressionAsNumericOne != nullptr && expressionAsNumericOne->value && expressionAsNumericOne->value) {
                return tryGetConstantValueOf(*expressionAsNumericOne->value);
            }
            return std::nullopt;
        }

        [[nodiscard]] static std::optional<unsigned int> tryGetConstantValueOf(const syrec::Number& number) {
            return number.isConstant() ? number.tryEvaluate({}) : std::nullopt;
        }

        [[nodiscard]] static std::optional<std::pair<unsigned int, unsigned int>> tryDetermineAccessedBitrangeOfVariableAccess(const syrec::VariableAccess& variableAccess) {
            if (!variableAccess.range.has_value()) {
                return std::make_pair(0, variableAccess.bitwidth() - 1);
            }

            if (!variableAccess.range->first || !variableAccess.range->second) {
                return std::nullopt;
            }

            const std::optional<unsigned int> accessedBitrangeStart = variableAccess.range->first->tryEvaluate({});
            const std::optional<unsigned int> accessedBitRangeEnd   = accessedBitrangeStart.has_value() ? variableAccess.range->second->tryEvaluate({}) : std::nullopt;
            if (!accessedBitRangeEnd.has_value()) {
                return std::nullopt;
            }
            return std::make_pair(*accessedBitrangeStart, *accessedBitRangeEnd);
        }

        [[nodiscard]] static unsigned int getLengthOfAccessedBitrange(const std::pair<unsigned int, unsigned int> accessedBitrange) {
            return (accessedBitrange.first > accessedBitrange.second ? accessedBitrange.first - accessedBitrange.second : accessedBitrange.second - accessedBitrange.first) + 1;
        }

        /**
         * @brief Build and record a semantic error of a specific type whose message template accepts one or more arguments
         * @tparam ...T The types of the arguments provided to the template parameter pack
         * @tparam semanticError The kind of semantic error to create
         * @param messagePosition The origin of the semantic error in the SyReC program
         * @param ...args User-provided arguments that will be used to replace the placeholds in the message template of the semantic error
         */
        template<SemanticError semanticError, typename... T>
        void recordSemanticError(Message::Position messagePosition, T&&... args) const {
            if (!sharedGeneratedMessageContainerInstance) {
                return;
            }

            static_assert(!getFormatForSemanticErrorMessage<semanticError>().empty(), "No format for message of semantic error found!");
            static_assert(!getIdentifierForSemanticError<semanticError>().empty(), "No identifiers for semantic error found!");

            constexpr std::string_view identifierForSemanticError = getIdentifierForSemanticError<semanticError>();
            // An informal requirement for the C++ code of the mqt-syrec library is to strongly discourage explicitly throwing exceptions to not degrage the performance of the library.
            // Thus the parser will record syntax as well as semantic errors in a STL container in the form of std::string messages create via the {fmt} library. We do make use of the
            // compile time checks provided by the {fmt} library to perform type-safety checks between the specified messages templates parameter types and the user-provided arguments,
            // but cannot guarantee that no exception is thrown during the conversion process of the user-provided arguments (i.e. for a datetime, etc.). Currently we are only using
            // std::string and other primitive types in the message templates (and thus no exception should be thrown here [if the prior stringification from a custom type to std::string succeeds]).
            // We recommend to also use the approach in the future in which the user must at first 'stringify'/transform his custom-types to corresponding std::string/primitive or any of the other
            // supported types supported by {fmt} (https://fmt.dev/11.1/api/#standard-library-types-formatting).
            sharedGeneratedMessageContainerInstance->recordMessage(std::make_unique<Message>(Message::Type::Error, std::string(identifierForSemanticError), messagePosition, fmt::format(FMT_STRING(getFormatForSemanticErrorMessage<semanticError>()), std::forward<T>(args)...)));
        }

        /**
         * @brief Build and record a semantic error of a specific type whose message template accepts no arguments
         * @tparam semanticError The kind of semantic error to create
         * @param messagePosition The origin of the semantic error in the SyReC program
         */
        template<SemanticError semanticError>
        void recordSemanticError(Message::Position messagePosition) const {
            if (!sharedGeneratedMessageContainerInstance) {
                return;
            }

            static_assert(!getFormatForSemanticErrorMessage<semanticError>().empty(), "No format for message of semantic error found!");
            static_assert(!getIdentifierForSemanticError<semanticError>().empty(), "No identifiers for semantic error found!");

            constexpr std::string_view identifierForSemanticError = getIdentifierForSemanticError<semanticError>();
            // An informal requirement for the C++ code of the mqt-syrec library is to strongly discourage explicitly throwing exceptions to not degrage the performance of the library.
            // Thus the parser will record syntax as well as semantic errors in a STL container in the form of std::string messages create via the {fmt} library. We do make use of the
            // compile time checks provided by the {fmt} library to perform type-safety checks between the specified messages templates parameter types and the user-provided arguments,
            // but cannot guarantee that no exception is thrown during the conversion process of the user-provided arguments (i.e. for a datetime, etc.). Currently we are only using
            // std::string and other primitive types in the message templates (and thus no exception should be thrown here [if the prior stringification from a custom type to std::string succeeds]).
            // We recommend to also use the approach in the future in which the user must at first 'stringify'/transform his custom-types to corresponding std::string/primitive or any of the other
            // supported types supported by {fmt} (https://fmt.dev/11.1/api/#standard-library-types-formatting).
            sharedGeneratedMessageContainerInstance->recordMessage(std::make_unique<Message>(Message::Type::Error, std::string(identifierForSemanticError), messagePosition, std::string(getFormatForSemanticErrorMessage<semanticError>())));
        }

        /**
         * @brief Record a custom error
         * @param messagePosition The origin of the semantic error in the SyReC program
         * @param errorMessage The text of the error message (can be empty)
         */
        void recordCustomError(Message::Position messagePosition, const std::string& errorMessage) const {
            sharedGeneratedMessageContainerInstance->recordMessage(std::make_unique<Message>(Message::Type::Error, "UNKNOWN", messagePosition, errorMessage));
        }
    };
} // namespace syrec_parser
