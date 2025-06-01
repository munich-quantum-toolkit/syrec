/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/components/custom_error_listener.hpp"

#include "Recognizer.h"
#include "Token.h"
#include "core/syrec/parser/utils/parser_messages_container.hpp"

#include <cstddef>
#include <exception>
#include <memory>
#include <string>

using namespace syrec_parser;

void CustomErrorListener::syntaxError([[maybe_unused]] antlr4::Recognizer* recognizer, [[maybe_unused]] antlr4::Token* offendingSymbol, std::size_t line, std::size_t charPositionInLine, const std::string& msg, [[maybe_unused]] std::exception_ptr e) {
    if (!sharedMessagesContainerInstance) {
        return;
    }
    sharedMessagesContainerInstance->recordMessage(std::make_unique<Message>(Message::Type::Error, "SYNTAX", Message::Position(line, charPositionInLine), msg));
}
