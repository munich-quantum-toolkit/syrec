/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "TSyrecLexer.h"

#include "CharStream.h"
#include "Lexer.h"
#include "Vocabulary.h"
#include "atn/ATN.h"
#include "atn/ATNDeserializer.h"
#include "atn/LexerATNSimulator.h"
#include "atn/PredictionContextCache.h"
#include "atn/SerializedATNView.h"
#include "dfa/DFA.h"
#include "internal/Synchronization.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace syrec_parser;
using namespace antlr4;

// Details on the internal mechanisms of ANTLR can be found in:
// - "The Definitive ANTLR 4 Reference" (ISBN-13: 978 - 1934356999)
// - "Adaptive LL(*) parsing: the power of dynamic analysis" (DOI: https://doi.org/10.1145/2714064.2660202)
namespace {
    struct TSyrecLexerStaticData final {
        TSyrecLexerStaticData(std::vector<std::string> ruleNames,
                              std::vector<std::string> channelNames,
                              std::vector<std::string> modeNames,
                              std::vector<std::string> literalNames,
                              std::vector<std::string> symbolicNames):
            ruleNames(std::move(ruleNames)),
            channelNames(std::move(channelNames)),
            modeNames(std::move(modeNames)), literalNames(std::move(literalNames)),
            symbolicNames(std::move(symbolicNames)),
            vocabulary(this->literalNames, this->symbolicNames) {}

        TSyrecLexerStaticData(const TSyrecLexerStaticData&)            = delete;
        TSyrecLexerStaticData(TSyrecLexerStaticData&&)                 = delete;
        TSyrecLexerStaticData& operator=(const TSyrecLexerStaticData&) = delete;
        TSyrecLexerStaticData& operator=(TSyrecLexerStaticData&&)      = delete;

        std::vector<dfa::DFA>          decisionToDFA;
        atn::PredictionContextCache    sharedContextCache;
        const std::vector<std::string> ruleNames;
        const std::vector<std::string> channelNames;
        const std::vector<std::string> modeNames;
        const std::vector<std::string> literalNames;
        const std::vector<std::string> symbolicNames;
        const dfa::Vocabulary          vocabulary;
        atn::SerializedATNView         serializedATN;
        std::unique_ptr<atn::ATN>      atn;
    };

    // Both of these variables are global static variables and thus the .clang-tidy check is correct in warning about their usage but
    // since they are declared in an anonymous namespace they are also local to the compilation unit and thus not accessible outside of this source file.
    // The remaining multithreading issues (as mentioned in the cpp-core-guidelines [https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#i2-avoid-non-const-global-variables])
    // that could arise for global static variables are resolved by using the synchronization mechanism via antlr4::internal::OnceFlag for a thread-safe initialization
    // of the static data instance.
    internal::OnceFlag                     lexerInitializationSyncFlag; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    std::unique_ptr<TSyrecLexerStaticData> lexerStaticData = nullptr;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    /**
     * @brief Initialize the data structures used by the lexer to implement the ANTLR ALL(*) parsing technique.
     *
     * The ALL(*) parsing technique utilizes an augmented recursive transition network (ATN) to resolve ambiguities/
     * determine how to continue in the grammar at a given non-terminal symbol in the input grammar while also utilizing
     * a deterministic finite automata (DFA) to cache previous decisions. For further details on the algorithm we refer
     * to: "Adaptive LL(*) parsing: the power of dynamic analysis" (DOI: https://doi.org/10.1145/2714064.2660202).
     */
    void initializeStaticLexerData() {
        assert(lexerStaticData == nullptr);

        // Currently we are assuming that the auto-generated string constants in the std::vector instances used to initialize the
        // TSyrecLexerStaticData class should not be modified as they are used to debugging/visualization purposes (that is our
        // current assumption based on the Java documentation [e.g. https://www.antlr.org/api/Java/org/antlr/v4/runtime/Recognizer.html#getGrammarFileName()]
        // with the C++ implementation being assumed to be functionally equivalent). "Debugging purporses" could also mean resolving
        // errors in a user provided .syrec file using the generated syntax error messages of the lexer/parser.
        auto staticData = std::make_unique<TSyrecLexerStaticData>(
                std::vector<std::string>{
                        "OP_INCREMENT_ASSIGN", "OP_DECREMENT_ASSIGN", "OP_INVERT_ASSIGN",
                        "OP_ADD_ASSIGN", "OP_SUB_ASSIGN", "OP_XOR_ASSIGN", "OP_PLUS", "OP_MINUS",
                        "OP_MULTIPLY", "OP_UPPER_BIT_MULTIPLY", "OP_DIVISION", "OP_MODULO",
                        "OP_LEFT_SHIFT", "OP_RIGHT_SHIFT", "OP_SWAP", "OP_GREATER_OR_EQUAL",
                        "OP_LESS_OR_EQUAL", "OP_GREATER_THAN", "OP_LESS_THAN", "OP_EQUAL",
                        "OP_NOT_EQUAL", "OP_LOGICAL_AND", "OP_LOGICAL_OR", "OP_LOGICAL_NEGATION",
                        "OP_BITWISE_AND", "OP_BITWISE_NEGATION", "OP_BITWISE_OR", "OP_BITWISE_XOR",
                        "OP_CALL", "OP_UNCALL", "VAR_TYPE_IN", "VAR_TYPE_OUT", "VAR_TYPE_INOUT",
                        "VAR_TYPE_WIRE", "VAR_TYPE_STATE", "LOOP_VARIABLE_PREFIX", "SIGNAL_WIDTH_PREFIX",
                        "STATEMENT_DELIMITER", "PARAMETER_DELIMITER", "OPEN_RBRACKET", "CLOSE_RBRACKET",
                        "OPEN_SBRACKET", "CLOSE_SBRACKET", "KEYWORD_MODULE", "KEYWORD_FOR",
                        "KEYWORD_DO", "KEYWORD_TO", "KEYWORD_STEP", "KEYWORD_ROF", "KEYWORD_IF",
                        "KEYWORD_THEN", "KEYWORD_ELSE", "KEYWORD_FI", "KEYWORD_SKIP", "BITRANGE_START_PREFIX",
                        "BITRANGE_END_PREFIX", "SKIPABLEWHITSPACES", "LINE_COMMENT", "MULTI_LINE_COMMENT",
                        "LETTER", "DIGIT", "IDENT", "INT"},
                std::vector<std::string>{
                        "DEFAULT_TOKEN_CHANNEL", "HIDDEN"},
                std::vector<std::string>{
                        "DEFAULT_MODE"},
                std::vector<std::string>{
                        "", "'++='", "'--='", "'~='", "'+='", "'-='", "'^='", "'+'", "'-'",
                        "'*'", "'*>'", "'/'", "'%'", "'<<'", "'>>'", "'<=>'", "'>='", "'<='",
                        "'>'", "'<'", "'='", "'!='", "'&&'", "'||'", "'!'", "'&'", "'~'",
                        "'|'", "'^'", "'call'", "'uncall'", "'in'", "'out'", "'inout'", "'wire'",
                        "'state'", "'$'", "'#'", "';'", "','", "'('", "')'", "'['", "']'",
                        "'module'", "'for'", "'do'", "'to'", "'step'", "'rof'", "'if'", "'then'",
                        "'else'", "'fi'", "'skip'", "'.'", "':'"},
                std::vector<std::string>{
                        "", "OP_INCREMENT_ASSIGN", "OP_DECREMENT_ASSIGN", "OP_INVERT_ASSIGN",
                        "OP_ADD_ASSIGN", "OP_SUB_ASSIGN", "OP_XOR_ASSIGN", "OP_PLUS", "OP_MINUS",
                        "OP_MULTIPLY", "OP_UPPER_BIT_MULTIPLY", "OP_DIVISION", "OP_MODULO",
                        "OP_LEFT_SHIFT", "OP_RIGHT_SHIFT", "OP_SWAP", "OP_GREATER_OR_EQUAL",
                        "OP_LESS_OR_EQUAL", "OP_GREATER_THAN", "OP_LESS_THAN", "OP_EQUAL",
                        "OP_NOT_EQUAL", "OP_LOGICAL_AND", "OP_LOGICAL_OR", "OP_LOGICAL_NEGATION",
                        "OP_BITWISE_AND", "OP_BITWISE_NEGATION", "OP_BITWISE_OR", "OP_BITWISE_XOR",
                        "OP_CALL", "OP_UNCALL", "VAR_TYPE_IN", "VAR_TYPE_OUT", "VAR_TYPE_INOUT",
                        "VAR_TYPE_WIRE", "VAR_TYPE_STATE", "LOOP_VARIABLE_PREFIX", "SIGNAL_WIDTH_PREFIX",
                        "STATEMENT_DELIMITER", "PARAMETER_DELIMITER", "OPEN_RBRACKET", "CLOSE_RBRACKET",
                        "OPEN_SBRACKET", "CLOSE_SBRACKET", "KEYWORD_MODULE", "KEYWORD_FOR",
                        "KEYWORD_DO", "KEYWORD_TO", "KEYWORD_STEP", "KEYWORD_ROF", "KEYWORD_IF",
                        "KEYWORD_THEN", "KEYWORD_ELSE", "KEYWORD_FI", "KEYWORD_SKIP", "BITRANGE_START_PREFIX",
                        "BITRANGE_END_PREFIX", "SKIPABLEWHITSPACES", "LINE_COMMENT", "MULTI_LINE_COMMENT",
                        "IDENT", "INT"});
        // Auto-generated constants that should not be changed except for when changes in the TSyrecLexer.g4 file were made
        static std::array<int32_t, 3093> serializedAtnSegment{
                4, 0, 61, 360, 6, -1, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 2, 2, 3, 7, 3, 2, 4, 7, 4, 2, 5, 7, 5, 2, 6, 7,
                6, 2, 7, 7, 7, 2, 8, 7, 8, 2, 9, 7, 9, 2, 10, 7, 10, 2, 11, 7, 11, 2, 12, 7, 12, 2, 13, 7, 13, 2, 14,
                7, 14, 2, 15, 7, 15, 2, 16, 7, 16, 2, 17, 7, 17, 2, 18, 7, 18, 2, 19, 7, 19, 2, 20, 7, 20, 2, 21,
                7, 21, 2, 22, 7, 22, 2, 23, 7, 23, 2, 24, 7, 24, 2, 25, 7, 25, 2, 26, 7, 26, 2, 27, 7, 27, 2, 28,
                7, 28, 2, 29, 7, 29, 2, 30, 7, 30, 2, 31, 7, 31, 2, 32, 7, 32, 2, 33, 7, 33, 2, 34, 7, 34, 2, 35,
                7, 35, 2, 36, 7, 36, 2, 37, 7, 37, 2, 38, 7, 38, 2, 39, 7, 39, 2, 40, 7, 40, 2, 41, 7, 41, 2, 42,
                7, 42, 2, 43, 7, 43, 2, 44, 7, 44, 2, 45, 7, 45, 2, 46, 7, 46, 2, 47, 7, 47, 2, 48, 7, 48, 2, 49,
                7, 49, 2, 50, 7, 50, 2, 51, 7, 51, 2, 52, 7, 52, 2, 53, 7, 53, 2, 54, 7, 54, 2, 55, 7, 55, 2, 56,
                7, 56, 2, 57, 7, 57, 2, 58, 7, 58, 2, 59, 7, 59, 2, 60, 7, 60, 2, 61, 7, 61, 2, 62, 7, 62, 1, 0,
                1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 1, 3, 1, 3, 1, 3, 1, 4, 1, 4, 1, 4, 1, 5, 1,
                5, 1, 5, 1, 6, 1, 6, 1, 7, 1, 7, 1, 8, 1, 8, 1, 9, 1, 9, 1, 9, 1, 10, 1, 10, 1, 11, 1, 11, 1, 12, 1,
                12, 1, 12, 1, 13, 1, 13, 1, 13, 1, 14, 1, 14, 1, 14, 1, 14, 1, 15, 1, 15, 1, 15, 1, 16, 1, 16, 1,
                16, 1, 17, 1, 17, 1, 18, 1, 18, 1, 19, 1, 19, 1, 20, 1, 20, 1, 20, 1, 21, 1, 21, 1, 21, 1, 22, 1,
                22, 1, 22, 1, 23, 1, 23, 1, 24, 1, 24, 1, 25, 1, 25, 1, 26, 1, 26, 1, 27, 1, 27, 1, 28, 1, 28, 1,
                28, 1, 28, 1, 28, 1, 29, 1, 29, 1, 29, 1, 29, 1, 29, 1, 29, 1, 29, 1, 30, 1, 30, 1, 30, 1, 31, 1,
                31, 1, 31, 1, 31, 1, 32, 1, 32, 1, 32, 1, 32, 1, 32, 1, 32, 1, 33, 1, 33, 1, 33, 1, 33, 1, 33, 1,
                34, 1, 34, 1, 34, 1, 34, 1, 34, 1, 34, 1, 35, 1, 35, 1, 36, 1, 36, 1, 37, 1, 37, 1, 38, 1, 38, 1,
                39, 1, 39, 1, 40, 1, 40, 1, 41, 1, 41, 1, 42, 1, 42, 1, 43, 1, 43, 1, 43, 1, 43, 1, 43, 1, 43, 1,
                43, 1, 44, 1, 44, 1, 44, 1, 44, 1, 45, 1, 45, 1, 45, 1, 46, 1, 46, 1, 46, 1, 47, 1, 47, 1, 47, 1,
                47, 1, 47, 1, 48, 1, 48, 1, 48, 1, 48, 1, 49, 1, 49, 1, 49, 1, 50, 1, 50, 1, 50, 1, 50, 1, 50, 1,
                51, 1, 51, 1, 51, 1, 51, 1, 51, 1, 52, 1, 52, 1, 52, 1, 53, 1, 53, 1, 53, 1, 53, 1, 53, 1, 54, 1,
                54, 1, 55, 1, 55, 1, 56, 4, 56, 306, 8, 56, 11, 56, 12, 56, 307, 1, 56, 1, 56, 1, 57, 1, 57, 1,
                57, 1, 57, 5, 57, 316, 8, 57, 10, 57, 12, 57, 319, 9, 57, 1, 57, 3, 57, 322, 8, 57, 1, 57, 1,
                57, 1, 58, 1, 58, 1, 58, 1, 58, 5, 58, 330, 8, 58, 10, 58, 12, 58, 333, 9, 58, 1, 58, 1, 58, 1,
                58, 1, 58, 1, 58, 1, 59, 1, 59, 1, 60, 1, 60, 1, 61, 1, 61, 3, 61, 346, 8, 61, 1, 61, 1, 61, 1,
                61, 5, 61, 351, 8, 61, 10, 61, 12, 61, 354, 9, 61, 1, 62, 4, 62, 357, 8, 62, 11, 62, 12, 62,
                358, 2, 317, 331, 0, 63, 1, 1, 3, 2, 5, 3, 7, 4, 9, 5, 11, 6, 13, 7, 15, 8, 17, 9, 19, 10, 21, 11,
                23, 12, 25, 13, 27, 14, 29, 15, 31, 16, 33, 17, 35, 18, 37, 19, 39, 20, 41, 21, 43, 22, 45,
                23, 47, 24, 49, 25, 51, 26, 53, 27, 55, 28, 57, 29, 59, 30, 61, 31, 63, 32, 65, 33, 67, 34,
                69, 35, 71, 36, 73, 37, 75, 38, 77, 39, 79, 40, 81, 41, 83, 42, 85, 43, 87, 44, 89, 45, 91,
                46, 93, 47, 95, 48, 97, 49, 99, 50, 101, 51, 103, 52, 105, 53, 107, 54, 109, 55, 111, 56,
                113, 57, 115, 58, 117, 59, 119, 0, 121, 0, 123, 60, 125, 61, 1, 0, 3, 3, 0, 9, 10, 13, 13, 32,
                32, 1, 1, 10, 10, 2, 0, 65, 90, 97, 122, 365, 0, 1, 1, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 5, 1, 0, 0, 0,
                0, 7, 1, 0, 0, 0, 0, 9, 1, 0, 0, 0, 0, 11, 1, 0, 0, 0, 0, 13, 1, 0, 0, 0, 0, 15, 1, 0, 0, 0, 0, 17, 1,
                0, 0, 0, 0, 19, 1, 0, 0, 0, 0, 21, 1, 0, 0, 0, 0, 23, 1, 0, 0, 0, 0, 25, 1, 0, 0, 0, 0, 27, 1, 0, 0,
                0, 0, 29, 1, 0, 0, 0, 0, 31, 1, 0, 0, 0, 0, 33, 1, 0, 0, 0, 0, 35, 1, 0, 0, 0, 0, 37, 1, 0, 0, 0, 0,
                39, 1, 0, 0, 0, 0, 41, 1, 0, 0, 0, 0, 43, 1, 0, 0, 0, 0, 45, 1, 0, 0, 0, 0, 47, 1, 0, 0, 0, 0, 49, 1,
                0, 0, 0, 0, 51, 1, 0, 0, 0, 0, 53, 1, 0, 0, 0, 0, 55, 1, 0, 0, 0, 0, 57, 1, 0, 0, 0, 0, 59, 1, 0, 0,
                0, 0, 61, 1, 0, 0, 0, 0, 63, 1, 0, 0, 0, 0, 65, 1, 0, 0, 0, 0, 67, 1, 0, 0, 0, 0, 69, 1, 0, 0, 0, 0,
                71, 1, 0, 0, 0, 0, 73, 1, 0, 0, 0, 0, 75, 1, 0, 0, 0, 0, 77, 1, 0, 0, 0, 0, 79, 1, 0, 0, 0, 0, 81, 1,
                0, 0, 0, 0, 83, 1, 0, 0, 0, 0, 85, 1, 0, 0, 0, 0, 87, 1, 0, 0, 0, 0, 89, 1, 0, 0, 0, 0, 91, 1, 0, 0,
                0, 0, 93, 1, 0, 0, 0, 0, 95, 1, 0, 0, 0, 0, 97, 1, 0, 0, 0, 0, 99, 1, 0, 0, 0, 0, 101, 1, 0, 0, 0, 0,
                103, 1, 0, 0, 0, 0, 105, 1, 0, 0, 0, 0, 107, 1, 0, 0, 0, 0, 109, 1, 0, 0, 0, 0, 111, 1, 0, 0, 0, 0,
                113, 1, 0, 0, 0, 0, 115, 1, 0, 0, 0, 0, 117, 1, 0, 0, 0, 0, 123, 1, 0, 0, 0, 0, 125, 1, 0, 0, 0, 1,
                127, 1, 0, 0, 0, 3, 131, 1, 0, 0, 0, 5, 135, 1, 0, 0, 0, 7, 138, 1, 0, 0, 0, 9, 141, 1, 0, 0, 0, 11,
                144, 1, 0, 0, 0, 13, 147, 1, 0, 0, 0, 15, 149, 1, 0, 0, 0, 17, 151, 1, 0, 0, 0, 19, 153, 1, 0, 0,
                0, 21, 156, 1, 0, 0, 0, 23, 158, 1, 0, 0, 0, 25, 160, 1, 0, 0, 0, 27, 163, 1, 0, 0, 0, 29, 166,
                1, 0, 0, 0, 31, 170, 1, 0, 0, 0, 33, 173, 1, 0, 0, 0, 35, 176, 1, 0, 0, 0, 37, 178, 1, 0, 0, 0, 39,
                180, 1, 0, 0, 0, 41, 182, 1, 0, 0, 0, 43, 185, 1, 0, 0, 0, 45, 188, 1, 0, 0, 0, 47, 191, 1, 0, 0,
                0, 49, 193, 1, 0, 0, 0, 51, 195, 1, 0, 0, 0, 53, 197, 1, 0, 0, 0, 55, 199, 1, 0, 0, 0, 57, 201,
                1, 0, 0, 0, 59, 206, 1, 0, 0, 0, 61, 213, 1, 0, 0, 0, 63, 216, 1, 0, 0, 0, 65, 220, 1, 0, 0, 0, 67,
                226, 1, 0, 0, 0, 69, 231, 1, 0, 0, 0, 71, 237, 1, 0, 0, 0, 73, 239, 1, 0, 0, 0, 75, 241, 1, 0, 0,
                0, 77, 243, 1, 0, 0, 0, 79, 245, 1, 0, 0, 0, 81, 247, 1, 0, 0, 0, 83, 249, 1, 0, 0, 0, 85, 251,
                1, 0, 0, 0, 87, 253, 1, 0, 0, 0, 89, 260, 1, 0, 0, 0, 91, 264, 1, 0, 0, 0, 93, 267, 1, 0, 0, 0, 95,
                270, 1, 0, 0, 0, 97, 275, 1, 0, 0, 0, 99, 279, 1, 0, 0, 0, 101, 282, 1, 0, 0, 0, 103, 287, 1, 0,
                0, 0, 105, 292, 1, 0, 0, 0, 107, 295, 1, 0, 0, 0, 109, 300, 1, 0, 0, 0, 111, 302, 1, 0, 0, 0, 113,
                305, 1, 0, 0, 0, 115, 311, 1, 0, 0, 0, 117, 325, 1, 0, 0, 0, 119, 339, 1, 0, 0, 0, 121, 341, 1,
                0, 0, 0, 123, 345, 1, 0, 0, 0, 125, 356, 1, 0, 0, 0, 127, 128, 5, 43, 0, 0, 128, 129, 5, 43, 0,
                0, 129, 130, 5, 61, 0, 0, 130, 2, 1, 0, 0, 0, 131, 132, 5, 45, 0, 0, 132, 133, 5, 45, 0, 0, 133,
                134, 5, 61, 0, 0, 134, 4, 1, 0, 0, 0, 135, 136, 5, 126, 0, 0, 136, 137, 5, 61, 0, 0, 137, 6, 1,
                0, 0, 0, 138, 139, 5, 43, 0, 0, 139, 140, 5, 61, 0, 0, 140, 8, 1, 0, 0, 0, 141, 142, 5, 45, 0,
                0, 142, 143, 5, 61, 0, 0, 143, 10, 1, 0, 0, 0, 144, 145, 5, 94, 0, 0, 145, 146, 5, 61, 0, 0, 146,
                12, 1, 0, 0, 0, 147, 148, 5, 43, 0, 0, 148, 14, 1, 0, 0, 0, 149, 150, 5, 45, 0, 0, 150, 16, 1,
                0, 0, 0, 151, 152, 5, 42, 0, 0, 152, 18, 1, 0, 0, 0, 153, 154, 5, 42, 0, 0, 154, 155, 5, 62, 0,
                0, 155, 20, 1, 0, 0, 0, 156, 157, 5, 47, 0, 0, 157, 22, 1, 0, 0, 0, 158, 159, 5, 37, 0, 0, 159,
                24, 1, 0, 0, 0, 160, 161, 5, 60, 0, 0, 161, 162, 5, 60, 0, 0, 162, 26, 1, 0, 0, 0, 163, 164, 5,
                62, 0, 0, 164, 165, 5, 62, 0, 0, 165, 28, 1, 0, 0, 0, 166, 167, 5, 60, 0, 0, 167, 168, 5, 61,
                0, 0, 168, 169, 5, 62, 0, 0, 169, 30, 1, 0, 0, 0, 170, 171, 5, 62, 0, 0, 171, 172, 5, 61, 0, 0,
                172, 32, 1, 0, 0, 0, 173, 174, 5, 60, 0, 0, 174, 175, 5, 61, 0, 0, 175, 34, 1, 0, 0, 0, 176, 177,
                5, 62, 0, 0, 177, 36, 1, 0, 0, 0, 178, 179, 5, 60, 0, 0, 179, 38, 1, 0, 0, 0, 180, 181, 5, 61,
                0, 0, 181, 40, 1, 0, 0, 0, 182, 183, 5, 33, 0, 0, 183, 184, 5, 61, 0, 0, 184, 42, 1, 0, 0, 0, 185,
                186, 5, 38, 0, 0, 186, 187, 5, 38, 0, 0, 187, 44, 1, 0, 0, 0, 188, 189, 5, 124, 0, 0, 189, 190,
                5, 124, 0, 0, 190, 46, 1, 0, 0, 0, 191, 192, 5, 33, 0, 0, 192, 48, 1, 0, 0, 0, 193, 194, 5, 38,
                0, 0, 194, 50, 1, 0, 0, 0, 195, 196, 5, 126, 0, 0, 196, 52, 1, 0, 0, 0, 197, 198, 5, 124, 0, 0,
                198, 54, 1, 0, 0, 0, 199, 200, 5, 94, 0, 0, 200, 56, 1, 0, 0, 0, 201, 202, 5, 99, 0, 0, 202, 203,
                5, 97, 0, 0, 203, 204, 5, 108, 0, 0, 204, 205, 5, 108, 0, 0, 205, 58, 1, 0, 0, 0, 206, 207, 5,
                117, 0, 0, 207, 208, 5, 110, 0, 0, 208, 209, 5, 99, 0, 0, 209, 210, 5, 97, 0, 0, 210, 211, 5,
                108, 0, 0, 211, 212, 5, 108, 0, 0, 212, 60, 1, 0, 0, 0, 213, 214, 5, 105, 0, 0, 214, 215, 5,
                110, 0, 0, 215, 62, 1, 0, 0, 0, 216, 217, 5, 111, 0, 0, 217, 218, 5, 117, 0, 0, 218, 219, 5,
                116, 0, 0, 219, 64, 1, 0, 0, 0, 220, 221, 5, 105, 0, 0, 221, 222, 5, 110, 0, 0, 222, 223, 5,
                111, 0, 0, 223, 224, 5, 117, 0, 0, 224, 225, 5, 116, 0, 0, 225, 66, 1, 0, 0, 0, 226, 227, 5,
                119, 0, 0, 227, 228, 5, 105, 0, 0, 228, 229, 5, 114, 0, 0, 229, 230, 5, 101, 0, 0, 230, 68,
                1, 0, 0, 0, 231, 232, 5, 115, 0, 0, 232, 233, 5, 116, 0, 0, 233, 234, 5, 97, 0, 0, 234, 235,
                5, 116, 0, 0, 235, 236, 5, 101, 0, 0, 236, 70, 1, 0, 0, 0, 237, 238, 5, 36, 0, 0, 238, 72, 1,
                0, 0, 0, 239, 240, 5, 35, 0, 0, 240, 74, 1, 0, 0, 0, 241, 242, 5, 59, 0, 0, 242, 76, 1, 0, 0, 0,
                243, 244, 5, 44, 0, 0, 244, 78, 1, 0, 0, 0, 245, 246, 5, 40, 0, 0, 246, 80, 1, 0, 0, 0, 247, 248,
                5, 41, 0, 0, 248, 82, 1, 0, 0, 0, 249, 250, 5, 91, 0, 0, 250, 84, 1, 0, 0, 0, 251, 252, 5, 93,
                0, 0, 252, 86, 1, 0, 0, 0, 253, 254, 5, 109, 0, 0, 254, 255, 5, 111, 0, 0, 255, 256, 5, 100,
                0, 0, 256, 257, 5, 117, 0, 0, 257, 258, 5, 108, 0, 0, 258, 259, 5, 101, 0, 0, 259, 88, 1, 0,
                0, 0, 260, 261, 5, 102, 0, 0, 261, 262, 5, 111, 0, 0, 262, 263, 5, 114, 0, 0, 263, 90, 1, 0,
                0, 0, 264, 265, 5, 100, 0, 0, 265, 266, 5, 111, 0, 0, 266, 92, 1, 0, 0, 0, 267, 268, 5, 116,
                0, 0, 268, 269, 5, 111, 0, 0, 269, 94, 1, 0, 0, 0, 270, 271, 5, 115, 0, 0, 271, 272, 5, 116,
                0, 0, 272, 273, 5, 101, 0, 0, 273, 274, 5, 112, 0, 0, 274, 96, 1, 0, 0, 0, 275, 276, 5, 114,
                0, 0, 276, 277, 5, 111, 0, 0, 277, 278, 5, 102, 0, 0, 278, 98, 1, 0, 0, 0, 279, 280, 5, 105,
                0, 0, 280, 281, 5, 102, 0, 0, 281, 100, 1, 0, 0, 0, 282, 283, 5, 116, 0, 0, 283, 284, 5, 104,
                0, 0, 284, 285, 5, 101, 0, 0, 285, 286, 5, 110, 0, 0, 286, 102, 1, 0, 0, 0, 287, 288, 5, 101,
                0, 0, 288, 289, 5, 108, 0, 0, 289, 290, 5, 115, 0, 0, 290, 291, 5, 101, 0, 0, 291, 104, 1, 0,
                0, 0, 292, 293, 5, 102, 0, 0, 293, 294, 5, 105, 0, 0, 294, 106, 1, 0, 0, 0, 295, 296, 5, 115,
                0, 0, 296, 297, 5, 107, 0, 0, 297, 298, 5, 105, 0, 0, 298, 299, 5, 112, 0, 0, 299, 108, 1, 0,
                0, 0, 300, 301, 5, 46, 0, 0, 301, 110, 1, 0, 0, 0, 302, 303, 5, 58, 0, 0, 303, 112, 1, 0, 0, 0,
                304, 306, 7, 0, 0, 0, 305, 304, 1, 0, 0, 0, 306, 307, 1, 0, 0, 0, 307, 305, 1, 0, 0, 0, 307, 308,
                1, 0, 0, 0, 308, 309, 1, 0, 0, 0, 309, 310, 6, 56, 0, 0, 310, 114, 1, 0, 0, 0, 311, 312, 5, 47,
                0, 0, 312, 313, 5, 47, 0, 0, 313, 317, 1, 0, 0, 0, 314, 316, 9, 0, 0, 0, 315, 314, 1, 0, 0, 0,
                316, 319, 1, 0, 0, 0, 317, 318, 1, 0, 0, 0, 317, 315, 1, 0, 0, 0, 318, 321, 1, 0, 0, 0, 319, 317,
                1, 0, 0, 0, 320, 322, 7, 1, 0, 0, 321, 320, 1, 0, 0, 0, 322, 323, 1, 0, 0, 0, 323, 324, 6, 57,
                0, 0, 324, 116, 1, 0, 0, 0, 325, 326, 5, 47, 0, 0, 326, 327, 5, 42, 0, 0, 327, 331, 1, 0, 0, 0,
                328, 330, 9, 0, 0, 0, 329, 328, 1, 0, 0, 0, 330, 333, 1, 0, 0, 0, 331, 332, 1, 0, 0, 0, 331, 329,
                1, 0, 0, 0, 332, 334, 1, 0, 0, 0, 333, 331, 1, 0, 0, 0, 334, 335, 5, 42, 0, 0, 335, 336, 5, 47,
                0, 0, 336, 337, 1, 0, 0, 0, 337, 338, 6, 58, 0, 0, 338, 118, 1, 0, 0, 0, 339, 340, 7, 2, 0, 0,
                340, 120, 1, 0, 0, 0, 341, 342, 2, 48, 57, 0, 342, 122, 1, 0, 0, 0, 343, 346, 5, 95, 0, 0, 344,
                346, 3, 119, 59, 0, 345, 343, 1, 0, 0, 0, 345, 344, 1, 0, 0, 0, 346, 352, 1, 0, 0, 0, 347, 351,
                5, 95, 0, 0, 348, 351, 3, 119, 59, 0, 349, 351, 3, 121, 60, 0, 350, 347, 1, 0, 0, 0, 350, 348,
                1, 0, 0, 0, 350, 349, 1, 0, 0, 0, 351, 354, 1, 0, 0, 0, 352, 350, 1, 0, 0, 0, 352, 353, 1, 0, 0,
                0, 353, 124, 1, 0, 0, 0, 354, 352, 1, 0, 0, 0, 355, 357, 3, 121, 60, 0, 356, 355, 1, 0, 0, 0,
                357, 358, 1, 0, 0, 0, 358, 356, 1, 0, 0, 0, 358, 359, 1, 0, 0, 0, 359, 126, 1, 0, 0, 0, 9, 0, 307,
                317, 321, 331, 345, 350, 352, 358, 1, 0, 1, 0};
        staticData->serializedATN = atn::SerializedATNView(serializedAtnSegment.data(), serializedAtnSegment.size());

        const atn::ATNDeserializer deserializer;
        // Build the augmented transition network (ATN) data structure from the serialized ATN segments generated by the
        // invocation of the ANTLR .jar binary.
        staticData->atn = deserializer.deserialize(staticData->serializedATN);

        // Initialization of the second data structure, the deterministic finite automatas (DFAs), used in the ALL(*) technique.
        const size_t count = staticData->atn->getNumberOfDecisions();
        staticData->decisionToDFA.reserve(count);
        for (size_t i = 0; i < count; i++) {
            staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
        }
        lexerStaticData = std::move(staticData);
    }
} // namespace

TSyrecLexer::TSyrecLexer(CharStream* input):
    Lexer(input) {
    initialize();
    // .clang-tidy checks warn that using raw pointers instead of one of the smart pointer alternatives defined by the STL might lead to memory leaks, etc. if not handled with care.
    // We cannot resolve all references to the raw pointer with its smart pointer alternative since the many references are defined in third-party code whose source files do not live in this solution (and are fetched at configure time)
    _interpreter = new atn::LexerATNSimulator(this, *lexerStaticData->atn, lexerStaticData->decisionToDFA, lexerStaticData->sharedContextCache); // NOLINT
}

TSyrecLexer::~TSyrecLexer() {
    delete _interpreter;
}

std::string TSyrecLexer::getGrammarFileName() const {
    return "TSyrecLexer.g4";
}

const std::vector<std::string>& TSyrecLexer::getRuleNames() const {
    return lexerStaticData->ruleNames;
}

const std::vector<std::string>& TSyrecLexer::getChannelNames() const {
    return lexerStaticData->channelNames;
}

const std::vector<std::string>& TSyrecLexer::getModeNames() const {
    return lexerStaticData->modeNames;
}

const dfa::Vocabulary& TSyrecLexer::getVocabulary() const {
    return lexerStaticData->vocabulary;
}

atn::SerializedATNView TSyrecLexer::getSerializedATN() const {
    return lexerStaticData->serializedATN;
}

const atn::ATN& TSyrecLexer::getATN() const {
    return *lexerStaticData->atn;
}

void TSyrecLexer::initialize() {
    call_once(lexerInitializationSyncFlag, initializeStaticLexerData);
}
