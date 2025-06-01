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

#include "CharStream.h"
#include "Lexer.h"
#include "Vocabulary.h"
#include "atn/ATN.h"
#include "atn/SerializedATNView.h"

#include <string>
#include <vector>

namespace syrec_parser {
    /**
     * @brief A non-thread-safe ANTLR-generated (v4.13.2) SyReC lexer
     */
    class TSyrecLexer: public antlr4::Lexer {
    public:
        explicit TSyrecLexer(antlr4::CharStream* input);
        ~TSyrecLexer() override;

        [[nodiscard]] std::string                     getGrammarFileName() const override;
        [[nodiscard]] const std::vector<std::string>& getRuleNames() const override;
        [[nodiscard]] const std::vector<std::string>& getChannelNames() const override;
        [[nodiscard]] const std::vector<std::string>& getModeNames() const override;
        [[nodiscard]] const antlr4::dfa::Vocabulary&  getVocabulary() const override;
        [[nodiscard]] antlr4::atn::SerializedATNView  getSerializedATN() const override;
        [[nodiscard]] const antlr4::atn::ATN&         getATN() const override;

        // By default the static state used to implement the lexer is lazily initialized during the first
        // call to the constructor. You can call this function if you wish to initialize the static state
        // ahead of time.
        static void initialize();
    };
} // namespace syrec_parser
