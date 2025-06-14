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

#include "BaseErrorListener.h"
#include "Recognizer.h"
#include "Token.h"
#include "core/syrec/parser/utils/parser_messages_container.hpp"

#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace syrec_parser {
    class CustomErrorListener: public antlr4::BaseErrorListener {
    public:
        explicit CustomErrorListener(std::shared_ptr<ParserMessagesContainer> sharedMessagesContainerInstance):
            sharedMessagesContainerInstance(std::move(sharedMessagesContainerInstance)) {}

        void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, std::size_t line,
                         std::size_t charPositionInLine, const std::string& msg, std::exception_ptr e) override;

    protected:
        std::shared_ptr<ParserMessagesContainer> sharedMessagesContainerInstance;
    };
} // namespace syrec_parser
