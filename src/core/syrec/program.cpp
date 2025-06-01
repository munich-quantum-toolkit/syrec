/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/program.hpp"

#include "ANTLRInputStream.h"
#include "CommonTokenStream.h"
#include "TSyrecLexer.h"
#include "TSyrecParser.h"
#include "core/syrec/parser/components/custom_error_listener.hpp"
#include "core/syrec/parser/components/custom_module_visitor.hpp"
#include "core/syrec/parser/utils/parser_messages_container.hpp"

#include <cstddef>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace syrec {
    std::string Program::read(const std::string& filename, const ReadProgramSettings settings) {
        std::string foundErrorWhileReadingFileContent;
        if (const std::optional<std::string> readFileContent = tryReadFileContent(filename, &foundErrorWhileReadingFileContent); readFileContent.has_value() && foundErrorWhileReadingFileContent.empty()) {
            readProgramFromString(*readFileContent, settings, foundErrorWhileReadingFileContent);
        }
        return foundErrorWhileReadingFileContent;
    }

    std::string Program::readFromString(const std::string_view& stringifiedProgram, const ReadProgramSettings settings) {
        std::string foundErrorWhileReadingFileContent;
        readProgramFromString(stringifiedProgram, settings, foundErrorWhileReadingFileContent);
        return foundErrorWhileReadingFileContent;
    }

    bool Program::readFile(const std::string& filename, const ReadProgramSettings settings, std::string& error) {
        error = read(filename, settings);
        return error.empty();
    }

    bool Program::readProgramFromString(const std::string_view& content, const ReadProgramSettings& settings, std::string& error) {
        antlr4::ANTLRInputStream   input(content);
        syrec_parser::TSyrecLexer  lexer(&input);
        antlr4::CommonTokenStream  tokens(&lexer);
        syrec_parser::TSyrecParser antlrParser(&tokens);

        auto       parserMessageGenerator = std::make_shared<syrec_parser::ParserMessagesContainer>();
        const auto customVisitor          = std::make_unique<syrec_parser::CustomModuleVisitor>(parserMessageGenerator, settings);
        const auto customErrorListener    = std::make_unique<syrec_parser::CustomErrorListener>(parserMessageGenerator);
        lexer.addErrorListener(customErrorListener.get());
        antlrParser.addErrorListener(customErrorListener.get());

        const std::optional<std::shared_ptr<Program>> parsedSyrecProgram = customVisitor->parseProgram(antlrParser.program());

        lexer.removeErrorListener(customErrorListener.get());
        antlrParser.removeErrorListener(customErrorListener.get());

        // In some cases the parser generates semantic errors at positions that were already processed or prior to already recorded errors (i.e. index out of range errors are reported during
        // the processing of the operands of an binary expression while an overlap between the left and right-hand side of an assignment can only be reported if the full expression on the right-hand
        // side of assignment were processed.
        // Since the parser currently only generates errors, sorting of the recorded error messages is sufficient.
        parserMessageGenerator->sortRecordedMessagesOfTypeInAscendingOrder(syrec_parser::Message::Type::Error);
        if (const auto& generatedErrorMessages = parserMessageGenerator->getMessagesOfType(syrec_parser::Message::Type::Error); !generatedErrorMessages.empty()) {
            std::stringstream concatenatedErrorMessageContainer;
            for (std::size_t i = 0; i < generatedErrorMessages.size() - 1; ++i) {
                concatenatedErrorMessageContainer << generatedErrorMessages.at(i)->stringify();
#if _WIN32
                concatenatedErrorMessageContainer << "\r\n";
#else
                concatenatedErrorMessageContainer << "\n";
#endif
            }
            concatenatedErrorMessageContainer << generatedErrorMessages.back()->stringify();
            error = concatenatedErrorMessageContainer.str();
            return false;
        }
        if (parsedSyrecProgram.has_value() && *parsedSyrecProgram != nullptr) {
            modulesVec = parsedSyrecProgram->get()->modulesVec;
        }
        return true;
    }

    std::optional<std::string> Program::tryReadFileContent(const std::string& filename, std::string* foundFileHandlingErrors) {
        // We cannot pass the filename as a std::string_view since the std::ifstream constructor or any of the underlying functions
        // will expect a null-terminated std::string argument to function correctly.
        if (std::ifstream inputFileStream(filename, std::ifstream::in | std::ifstream::binary); inputFileStream.is_open()) {
            inputFileStream.ignore(std::numeric_limits<std::streamsize>::max());
            const std::streamsize fileContentLength = inputFileStream.gcount();
            inputFileStream.clear(); //  Since ignore will have set eof.
            inputFileStream.seekg(0, std::ios_base::beg);

            std::string fileContentBuffer(static_cast<std::string::size_type>(fileContentLength), ' ');
            inputFileStream.read(fileContentBuffer.data(), fileContentLength);
            if (!inputFileStream.bad()) {
                return std::make_optional(fileContentBuffer);
            }
            if (foundFileHandlingErrors != nullptr) {
                *foundFileHandlingErrors = "Error while reading content from file @ " + std::string(filename);
            }
        } else if (foundFileHandlingErrors != nullptr) {
            *foundFileHandlingErrors = "Cannot open given circuit file @ " + std::string(filename);
        }
        return std::nullopt;
    }
} // namespace syrec
