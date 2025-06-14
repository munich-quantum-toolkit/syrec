/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "TSyrecParser.h"

#include "ANTLRErrorStrategy.h" // NOLINT(misc-include-cleaner)
#include "NoViableAltException.h"
#include "Parser.h"
#include "ParserRuleContext.h"
#include "RecognitionException.h"
#include "TokenStream.h"
#include "Vocabulary.h"
#include "atn/ATN.h"
#include "atn/ATNDeserializer.h"
#include "atn/ParserATNSimulator.h"
#include "atn/ParserATNSimulatorOptions.h"
#include "atn/PredictionContextCache.h"
#include "atn/SerializedATNView.h"
#include "dfa/DFA.h"
#include "internal/Synchronization.h"
#include "support/CPPUtils.h"
#include "support/Casts.h"
#include "tree/TerminalNode.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace antlrcpp;
using namespace syrec_parser;
using namespace antlr4;

// Details on the internal mechanisms of ANTLR can be found in:
// - "The Definitive ANTLR 4 Reference" (ISBN-13: 978 - 1934356999)
// - "Adaptive LL(*) parsing: the power of dynamic analysis" (DOI: https://doi.org/10.1145/2714064.2660202)
namespace {
    struct TSyrecParserStaticData final {
        TSyrecParserStaticData(std::vector<std::string> ruleNames,
                               std::vector<std::string> literalNames,
                               std::vector<std::string> symbolicNames):
            ruleNames(std::move(ruleNames)),
            literalNames(std::move(literalNames)),
            symbolicNames(std::move(symbolicNames)),
            vocabulary(this->literalNames, this->symbolicNames) {}

        TSyrecParserStaticData(const TSyrecParserStaticData&)            = delete;
        TSyrecParserStaticData(TSyrecParserStaticData&&)                 = delete;
        TSyrecParserStaticData& operator=(const TSyrecParserStaticData&) = delete;
        TSyrecParserStaticData& operator=(TSyrecParserStaticData&&)      = delete;

        std::vector<dfa::DFA>          decisionToDFA;
        atn::PredictionContextCache    sharedContextCache;
        const std::vector<std::string> ruleNames;
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
    internal::OnceFlag                      parserSingletonInitializationSyncFlag; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    std::unique_ptr<TSyrecParserStaticData> parserSingletonStaticData = nullptr;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    /**
     * @brief Initialize the data structures used by the parser to implement the ANTLR ALL(*) parsing technique.
     *
     * The ALL(*) parsing technique utilizes an augmented recursive transition network (ATN) to resolve ambiguities/
     * determine how to continue in the grammar at a given non-terminal symbol in the input grammar while also utilizing
     * a deterministic finite automata (DFA) to cache previous decisions. For further details on the algorithm we refer
     * to: "Adaptive LL(*) parsing: the power of dynamic analysis" (DOI: https://doi.org/10.1145/2714064.2660202).
     */
    void initializeStaticParserData() {
        assert(parserSingletonStaticData == nullptr);
        // Currently we are assuming that the auto-generated string constants in the std::vector instances used to initialize the
        // TSyrecParserStaticData class should not be modified as they are used to debugging/visualization purposes (that is our
        // current assumption based on the Java documentation [e.g. https://www.antlr.org/api/Java/org/antlr/v4/runtime/Recognizer.html#getGrammarFileName()]
        // with the C++ implementation being assumed to be functionally equivalent). "Debugging purporses" could also mean resolving
        // errors in a user provided .syrec file using the generated syntax error messages of the lexer/parser.
        auto staticData = std::make_unique<TSyrecParserStaticData>(
                std::vector<std::string>{
                        "number", "program", "module", "parameterList", "parameter", "signalList",
                        "signalDeclaration", "statementList", "statement", "callStatement",
                        "loopVariableDefinition", "loopStepsizeDefinition", "forStatement",
                        "ifStatement", "unaryStatement", "assignStatement", "swapStatement",
                        "skipStatement", "signal", "expression", "binaryExpression", "unaryExpression",
                        "shiftExpression"},
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
        // Auto-generated constants that should not be changed except for when changes in the TSyrecParser.g4 file were made
        static std::array<int32_t, 2036> serializedATNSegment = {
                4, 1, 61, 235, 2, 0, 7, 0, 2, 1, 7, 1, 2, 2, 7, 2, 2, 3, 7, 3, 2, 4, 7, 4, 2, 5, 7, 5, 2, 6, 7, 6, 2,
                7, 7, 7, 2, 8, 7, 8, 2, 9, 7, 9, 2, 10, 7, 10, 2, 11, 7, 11, 2, 12, 7, 12, 2, 13, 7, 13, 2, 14, 7,
                14, 2, 15, 7, 15, 2, 16, 7, 16, 2, 17, 7, 17, 2, 18, 7, 18, 2, 19, 7, 19, 2, 20, 7, 20, 2, 21, 7,
                21, 2, 22, 7, 22, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 3, 0, 58, 8, 0, 1,
                1, 4, 1, 61, 8, 1, 11, 1, 12, 1, 62, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 1, 2, 3, 2, 71, 8, 2, 1, 2, 1, 2,
                5, 2, 75, 8, 2, 10, 2, 12, 2, 78, 9, 2, 1, 2, 1, 2, 1, 3, 1, 3, 1, 3, 5, 3, 85, 8, 3, 10, 3, 12, 3,
                88, 9, 3, 1, 4, 1, 4, 1, 4, 1, 5, 1, 5, 1, 5, 1, 5, 5, 5, 97, 8, 5, 10, 5, 12, 5, 100, 9, 5, 1, 6, 1,
                6, 1, 6, 1, 6, 5, 6, 106, 8, 6, 10, 6, 12, 6, 109, 9, 6, 1, 6, 1, 6, 1, 6, 3, 6, 114, 8, 6, 1, 7, 1,
                7, 1, 7, 5, 7, 119, 8, 7, 10, 7, 12, 7, 122, 9, 7, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 3, 8, 131,
                8, 8, 1, 9, 1, 9, 1, 9, 1, 9, 1, 9, 1, 9, 5, 9, 139, 8, 9, 10, 9, 12, 9, 142, 9, 9, 1, 9, 1, 9, 1, 10,
                1, 10, 1, 10, 1, 10, 1, 11, 1, 11, 3, 11, 152, 8, 11, 1, 11, 1, 11, 1, 12, 1, 12, 3, 12, 158, 8,
                12, 1, 12, 1, 12, 1, 12, 3, 12, 163, 8, 12, 1, 12, 1, 12, 3, 12, 167, 8, 12, 1, 12, 1, 12, 1, 12,
                1, 12, 1, 13, 1, 13, 1, 13, 1, 13, 1, 13, 1, 13, 1, 13, 1, 13, 1, 13, 1, 14, 1, 14, 1, 14, 1, 15,
                1, 15, 1, 15, 1, 15, 1, 16, 1, 16, 1, 16, 1, 16, 1, 17, 1, 17, 1, 18, 1, 18, 1, 18, 1, 18, 1, 18,
                5, 18, 200, 8, 18, 10, 18, 12, 18, 203, 9, 18, 1, 18, 1, 18, 1, 18, 1, 18, 3, 18, 209, 8, 18,
                3, 18, 211, 8, 18, 1, 19, 1, 19, 1, 19, 1, 19, 1, 19, 3, 19, 218, 8, 19, 1, 20, 1, 20, 1, 20, 1,
                20, 1, 20, 1, 20, 1, 21, 1, 21, 1, 21, 1, 22, 1, 22, 1, 22, 1, 22, 1, 22, 1, 22, 1, 22, 0, 0, 23,
                0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 0, 9, 2,
                0, 7, 9, 11, 11, 1, 0, 31, 33, 1, 0, 34, 35, 1, 0, 29, 30, 1, 0, 1, 3, 1, 0, 4, 6, 4, 0, 7, 12, 16,
                23, 25, 25, 27, 28, 2, 0, 24, 24, 26, 26, 1, 0, 13, 14, 240, 0, 57, 1, 0, 0, 0, 2, 60, 1, 0, 0,
                0, 4, 66, 1, 0, 0, 0, 6, 81, 1, 0, 0, 0, 8, 89, 1, 0, 0, 0, 10, 92, 1, 0, 0, 0, 12, 101, 1, 0, 0, 0,
                14, 115, 1, 0, 0, 0, 16, 130, 1, 0, 0, 0, 18, 132, 1, 0, 0, 0, 20, 145, 1, 0, 0, 0, 22, 149, 1,
                0, 0, 0, 24, 155, 1, 0, 0, 0, 26, 172, 1, 0, 0, 0, 28, 181, 1, 0, 0, 0, 30, 184, 1, 0, 0, 0, 32,
                188, 1, 0, 0, 0, 34, 192, 1, 0, 0, 0, 36, 194, 1, 0, 0, 0, 38, 217, 1, 0, 0, 0, 40, 219, 1, 0, 0,
                0, 42, 225, 1, 0, 0, 0, 44, 228, 1, 0, 0, 0, 46, 58, 5, 61, 0, 0, 47, 48, 5, 37, 0, 0, 48, 58, 5,
                60, 0, 0, 49, 50, 5, 36, 0, 0, 50, 58, 5, 60, 0, 0, 51, 52, 5, 40, 0, 0, 52, 53, 3, 0, 0, 0, 53,
                54, 7, 0, 0, 0, 54, 55, 3, 0, 0, 0, 55, 56, 5, 41, 0, 0, 56, 58, 1, 0, 0, 0, 57, 46, 1, 0, 0, 0, 57,
                47, 1, 0, 0, 0, 57, 49, 1, 0, 0, 0, 57, 51, 1, 0, 0, 0, 58, 1, 1, 0, 0, 0, 59, 61, 3, 4, 2, 0, 60,
                59, 1, 0, 0, 0, 61, 62, 1, 0, 0, 0, 62, 60, 1, 0, 0, 0, 62, 63, 1, 0, 0, 0, 63, 64, 1, 0, 0, 0, 64,
                65, 5, 0, 0, 1, 65, 3, 1, 0, 0, 0, 66, 67, 5, 44, 0, 0, 67, 68, 5, 60, 0, 0, 68, 70, 5, 40, 0, 0,
                69, 71, 3, 6, 3, 0, 70, 69, 1, 0, 0, 0, 70, 71, 1, 0, 0, 0, 71, 72, 1, 0, 0, 0, 72, 76, 5, 41, 0,
                0, 73, 75, 3, 10, 5, 0, 74, 73, 1, 0, 0, 0, 75, 78, 1, 0, 0, 0, 76, 74, 1, 0, 0, 0, 76, 77, 1, 0,
                0, 0, 77, 79, 1, 0, 0, 0, 78, 76, 1, 0, 0, 0, 79, 80, 3, 14, 7, 0, 80, 5, 1, 0, 0, 0, 81, 86, 3, 8,
                4, 0, 82, 83, 5, 39, 0, 0, 83, 85, 3, 8, 4, 0, 84, 82, 1, 0, 0, 0, 85, 88, 1, 0, 0, 0, 86, 84, 1,
                0, 0, 0, 86, 87, 1, 0, 0, 0, 87, 7, 1, 0, 0, 0, 88, 86, 1, 0, 0, 0, 89, 90, 7, 1, 0, 0, 90, 91, 3,
                12, 6, 0, 91, 9, 1, 0, 0, 0, 92, 93, 7, 2, 0, 0, 93, 98, 3, 12, 6, 0, 94, 95, 5, 39, 0, 0, 95, 97,
                3, 12, 6, 0, 96, 94, 1, 0, 0, 0, 97, 100, 1, 0, 0, 0, 98, 96, 1, 0, 0, 0, 98, 99, 1, 0, 0, 0, 99,
                11, 1, 0, 0, 0, 100, 98, 1, 0, 0, 0, 101, 107, 5, 60, 0, 0, 102, 103, 5, 42, 0, 0, 103, 104, 5,
                61, 0, 0, 104, 106, 5, 43, 0, 0, 105, 102, 1, 0, 0, 0, 106, 109, 1, 0, 0, 0, 107, 105, 1, 0, 0,
                0, 107, 108, 1, 0, 0, 0, 108, 113, 1, 0, 0, 0, 109, 107, 1, 0, 0, 0, 110, 111, 5, 40, 0, 0, 111,
                112, 5, 61, 0, 0, 112, 114, 5, 41, 0, 0, 113, 110, 1, 0, 0, 0, 113, 114, 1, 0, 0, 0, 114, 13,
                1, 0, 0, 0, 115, 120, 3, 16, 8, 0, 116, 117, 5, 38, 0, 0, 117, 119, 3, 16, 8, 0, 118, 116, 1,
                0, 0, 0, 119, 122, 1, 0, 0, 0, 120, 118, 1, 0, 0, 0, 120, 121, 1, 0, 0, 0, 121, 15, 1, 0, 0, 0,
                122, 120, 1, 0, 0, 0, 123, 131, 3, 18, 9, 0, 124, 131, 3, 24, 12, 0, 125, 131, 3, 26, 13, 0,
                126, 131, 3, 28, 14, 0, 127, 131, 3, 30, 15, 0, 128, 131, 3, 32, 16, 0, 129, 131, 3, 34, 17,
                0, 130, 123, 1, 0, 0, 0, 130, 124, 1, 0, 0, 0, 130, 125, 1, 0, 0, 0, 130, 126, 1, 0, 0, 0, 130,
                127, 1, 0, 0, 0, 130, 128, 1, 0, 0, 0, 130, 129, 1, 0, 0, 0, 131, 17, 1, 0, 0, 0, 132, 133, 7,
                3, 0, 0, 133, 134, 5, 60, 0, 0, 134, 135, 5, 40, 0, 0, 135, 140, 5, 60, 0, 0, 136, 137, 5, 39,
                0, 0, 137, 139, 5, 60, 0, 0, 138, 136, 1, 0, 0, 0, 139, 142, 1, 0, 0, 0, 140, 138, 1, 0, 0, 0,
                140, 141, 1, 0, 0, 0, 141, 143, 1, 0, 0, 0, 142, 140, 1, 0, 0, 0, 143, 144, 5, 41, 0, 0, 144,
                19, 1, 0, 0, 0, 145, 146, 5, 36, 0, 0, 146, 147, 5, 60, 0, 0, 147, 148, 5, 20, 0, 0, 148, 21,
                1, 0, 0, 0, 149, 151, 5, 48, 0, 0, 150, 152, 5, 8, 0, 0, 151, 150, 1, 0, 0, 0, 151, 152, 1, 0,
                0, 0, 152, 153, 1, 0, 0, 0, 153, 154, 3, 0, 0, 0, 154, 23, 1, 0, 0, 0, 155, 162, 5, 45, 0, 0, 156,
                158, 3, 20, 10, 0, 157, 156, 1, 0, 0, 0, 157, 158, 1, 0, 0, 0, 158, 159, 1, 0, 0, 0, 159, 160,
                3, 0, 0, 0, 160, 161, 5, 47, 0, 0, 161, 163, 1, 0, 0, 0, 162, 157, 1, 0, 0, 0, 162, 163, 1, 0,
                0, 0, 163, 164, 1, 0, 0, 0, 164, 166, 3, 0, 0, 0, 165, 167, 3, 22, 11, 0, 166, 165, 1, 0, 0, 0,
                166, 167, 1, 0, 0, 0, 167, 168, 1, 0, 0, 0, 168, 169, 5, 46, 0, 0, 169, 170, 3, 14, 7, 0, 170,
                171, 5, 49, 0, 0, 171, 25, 1, 0, 0, 0, 172, 173, 5, 50, 0, 0, 173, 174, 3, 38, 19, 0, 174, 175,
                5, 51, 0, 0, 175, 176, 3, 14, 7, 0, 176, 177, 5, 52, 0, 0, 177, 178, 3, 14, 7, 0, 178, 179, 5,
                53, 0, 0, 179, 180, 3, 38, 19, 0, 180, 27, 1, 0, 0, 0, 181, 182, 7, 4, 0, 0, 182, 183, 3, 36,
                18, 0, 183, 29, 1, 0, 0, 0, 184, 185, 3, 36, 18, 0, 185, 186, 7, 5, 0, 0, 186, 187, 3, 38, 19,
                0, 187, 31, 1, 0, 0, 0, 188, 189, 3, 36, 18, 0, 189, 190, 5, 15, 0, 0, 190, 191, 3, 36, 18, 0,
                191, 33, 1, 0, 0, 0, 192, 193, 5, 54, 0, 0, 193, 35, 1, 0, 0, 0, 194, 201, 5, 60, 0, 0, 195, 196,
                5, 42, 0, 0, 196, 197, 3, 38, 19, 0, 197, 198, 5, 43, 0, 0, 198, 200, 1, 0, 0, 0, 199, 195, 1,
                0, 0, 0, 200, 203, 1, 0, 0, 0, 201, 199, 1, 0, 0, 0, 201, 202, 1, 0, 0, 0, 202, 210, 1, 0, 0, 0,
                203, 201, 1, 0, 0, 0, 204, 205, 5, 55, 0, 0, 205, 208, 3, 0, 0, 0, 206, 207, 5, 56, 0, 0, 207,
                209, 3, 0, 0, 0, 208, 206, 1, 0, 0, 0, 208, 209, 1, 0, 0, 0, 209, 211, 1, 0, 0, 0, 210, 204, 1,
                0, 0, 0, 210, 211, 1, 0, 0, 0, 211, 37, 1, 0, 0, 0, 212, 218, 3, 0, 0, 0, 213, 218, 3, 36, 18,
                0, 214, 218, 3, 40, 20, 0, 215, 218, 3, 42, 21, 0, 216, 218, 3, 44, 22, 0, 217, 212, 1, 0, 0,
                0, 217, 213, 1, 0, 0, 0, 217, 214, 1, 0, 0, 0, 217, 215, 1, 0, 0, 0, 217, 216, 1, 0, 0, 0, 218,
                39, 1, 0, 0, 0, 219, 220, 5, 40, 0, 0, 220, 221, 3, 38, 19, 0, 221, 222, 7, 6, 0, 0, 222, 223,
                3, 38, 19, 0, 223, 224, 5, 41, 0, 0, 224, 41, 1, 0, 0, 0, 225, 226, 7, 7, 0, 0, 226, 227, 3, 38,
                19, 0, 227, 43, 1, 0, 0, 0, 228, 229, 5, 40, 0, 0, 229, 230, 3, 38, 19, 0, 230, 231, 7, 8, 0,
                0, 231, 232, 3, 0, 0, 0, 232, 233, 5, 41, 0, 0, 233, 45, 1, 0, 0, 0, 19, 57, 62, 70, 76, 86, 98,
                107, 113, 120, 130, 140, 151, 157, 162, 166, 201, 208, 210, 217};
        staticData->serializedATN = atn::SerializedATNView(serializedATNSegment.data(), serializedATNSegment.size());

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
        parserSingletonStaticData = std::move(staticData);
    }

} // namespace

TSyrecParser::TSyrecParser(TokenStream* input):
    TSyrecParser(input, atn::ParserATNSimulatorOptions()) {}

TSyrecParser::TSyrecParser(TokenStream* input, const atn::ParserATNSimulatorOptions& options):
    Parser(input) {
    initialize();
    // .clang-tidy checks warn that using raw pointers instead of one of the smart pointer alternatives defined by the STL might lead to memory leaks, etc. if not handled with care.
    // We cannot resolve all references to the raw pointer with its smart pointer alternative since the many references are defined in third-party code whose source files do not live in this solution (and are fetched at configure time)
    _interpreter = new atn::ParserATNSimulator(this, *parserSingletonStaticData->atn, parserSingletonStaticData->decisionToDFA, parserSingletonStaticData->sharedContextCache, options); // NOLINT
}

TSyrecParser::~TSyrecParser() {
    delete _interpreter;
}

const atn::ATN& TSyrecParser::getATN() const {
    return *parserSingletonStaticData->atn;
}

std::string TSyrecParser::getGrammarFileName() const {
    return "TSyrecParser.g4";
}

const std::vector<std::string>& TSyrecParser::getRuleNames() const {
    return parserSingletonStaticData->ruleNames;
}

const dfa::Vocabulary& TSyrecParser::getVocabulary() const {
    return parserSingletonStaticData->vocabulary;
}

atn::SerializedATNView TSyrecParser::getSerializedATN() const {
    return parserSingletonStaticData->serializedATN;
}

//----------------- NumberContext ------------------------------------------------------------------

TSyrecParser::NumberContext::NumberContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

size_t TSyrecParser::NumberContext::getRuleIndex() const {
    return RuleNumber;
}

void TSyrecParser::NumberContext::copyFrom(NumberContext* ctx) {
    ParserRuleContext::copyFrom(ctx);
}

//----------------- NumberFromSignalwidthContext ------------------------------------------------------------------

tree::TerminalNode* TSyrecParser::NumberFromSignalwidthContext::literalSignalWidthPrefix() const {
    return getToken(SignalWidthPrefix, 0);
}

tree::TerminalNode* TSyrecParser::NumberFromSignalwidthContext::literalIdent() const {
    return getToken(Ident, 0);
}

TSyrecParser::NumberFromSignalwidthContext::NumberFromSignalwidthContext(NumberContext* ctx) {
    copyFrom(ctx);
}
//----------------- NumberFromLoopVariableContext ------------------------------------------------------------------

tree::TerminalNode* TSyrecParser::NumberFromLoopVariableContext::literalLoopVariablePrefix() const {
    return getToken(LoopVariablePrefix, 0);
}

tree::TerminalNode* TSyrecParser::NumberFromLoopVariableContext::literalIdent() const {
    return getToken(Ident, 0);
}

TSyrecParser::NumberFromLoopVariableContext::NumberFromLoopVariableContext(NumberContext* ctx) {
    copyFrom(ctx);
}
//----------------- NumberFromConstantContext ------------------------------------------------------------------

tree::TerminalNode* TSyrecParser::NumberFromConstantContext::literalInt() const {
    return getToken(Int, 0);
}

TSyrecParser::NumberFromConstantContext::NumberFromConstantContext(NumberContext* ctx) {
    copyFrom(ctx);
}
//----------------- NumberFromExpressionContext ------------------------------------------------------------------

std::vector<TSyrecParser::NumberContext*> TSyrecParser::NumberFromExpressionContext::number() const {
    return getRuleContexts<NumberContext>();
}

tree::TerminalNode* TSyrecParser::NumberFromExpressionContext::literalOpPlus() const {
    return getToken(OpPlus, 0);
}

tree::TerminalNode* TSyrecParser::NumberFromExpressionContext::literalOpMinus() const {
    return getToken(OpMinus, 0);
}

tree::TerminalNode* TSyrecParser::NumberFromExpressionContext::literalOpMultiply() const {
    return getToken(OpMultiply, 0);
}

tree::TerminalNode* TSyrecParser::NumberFromExpressionContext::literalOpDivision() const {
    return getToken(OpDivision, 0);
}

TSyrecParser::NumberFromExpressionContext::NumberFromExpressionContext(NumberContext* ctx) {
    copyFrom(ctx);
}

TSyrecParser::NumberContext* TSyrecParser::number() {
    auto* localCtx = _tracker.createInstance<NumberContext>(_ctx, getState());
    enterRule(localCtx, 0, RuleNumber);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        setState(57);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
            case Int: {
                localCtx = _tracker.createInstance<NumberFromConstantContext>(localCtx);
                enterOuterAlt(localCtx, 1);
                setState(46);
                match(Int);
                break;
            }

            case SignalWidthPrefix: {
                localCtx = _tracker.createInstance<NumberFromSignalwidthContext>(localCtx);
                enterOuterAlt(localCtx, 2);
                setState(47);
                match(SignalWidthPrefix);
                setState(48);
                match(Ident);
                break;
            }

            case LoopVariablePrefix: {
                localCtx = _tracker.createInstance<NumberFromLoopVariableContext>(localCtx);
                enterOuterAlt(localCtx, 3);
                setState(49);
                match(LoopVariablePrefix);
                setState(50);
                match(Ident);
                break;
            }

            case OpenRBracket: {
                size_t lookahead = 0;
                localCtx         = _tracker.createInstance<NumberFromExpressionContext>(localCtx);
                enterOuterAlt(localCtx, 4);
                setState(51);
                match(OpenRBracket);
                setState(52);
                antlrcpp::downCast<NumberFromExpressionContext*>(localCtx)->lhsOperand = number();
                setState(53);
                antlrcpp::downCast<NumberFromExpressionContext*>(localCtx)->op = _input->LT(1);
                lookahead                                                      = _input->LA(1);
                if (!((((lookahead & ~0x3fULL) == 0) && ((1ULL << lookahead) & 2944) != 0))) {
                    antlrcpp::downCast<NumberFromExpressionContext*>(localCtx)->op = _errHandler->recoverInline(this);
                } else {
                    _errHandler->reportMatch(this);
                    consume();
                }
                setState(54);
                antlrcpp::downCast<NumberFromExpressionContext*>(localCtx)->rhsOperand = number();
                setState(55);
                match(CloseRBracket);
                break;
            }

            default:
                throw NoViableAltException(this);
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ProgramContext ------------------------------------------------------------------

TSyrecParser::ProgramContext::ProgramContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

std::vector<TSyrecParser::ModuleContext*> TSyrecParser::ProgramContext::module() const {
    return getRuleContexts<ModuleContext>();
}

size_t TSyrecParser::ProgramContext::getRuleIndex() const {
    return RuleProgram;
}

TSyrecParser::ProgramContext* TSyrecParser::program() {
    auto* localCtx = _tracker.createInstance<ProgramContext>(_ctx, getState());
    enterRule(localCtx, 2, RuleProgram);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(60);
        _errHandler->sync(this);

        size_t lookahead = KeywordModule;
        while (lookahead == KeywordModule) {
            setState(59);
            module();
            setState(62);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }
        setState(64);
        match(TSyrecParser::EOF);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ModuleContext ------------------------------------------------------------------

TSyrecParser::ModuleContext::ModuleContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::ModuleContext::literalIdent() const {
    return getToken(Ident, 0);
}

TSyrecParser::StatementListContext* TSyrecParser::ModuleContext::statementList() const {
    return getRuleContext<StatementListContext>(0);
}

TSyrecParser::ParameterListContext* TSyrecParser::ModuleContext::parameterList() const {
    return getRuleContext<ParameterListContext>(0);
}

std::vector<TSyrecParser::SignalListContext*> TSyrecParser::ModuleContext::signalList() const {
    return getRuleContexts<SignalListContext>();
}

size_t TSyrecParser::ModuleContext::getRuleIndex() const {
    return RuleModule;
}

TSyrecParser::ModuleContext* TSyrecParser::module() {
    auto* localCtx = _tracker.createInstance<ModuleContext>(_ctx, getState());
    enterRule(localCtx, 4, RuleModule);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        size_t lookahead = 0;
        enterOuterAlt(localCtx, 1);
        setState(66);
        match(KeywordModule);
        setState(67);
        match(Ident);
        setState(68);
        match(OpenRBracket);
        setState(70);
        _errHandler->sync(this);

        lookahead = _input->LA(1);
        if ((((lookahead & ~0x3fULL) == 0) && ((1ULL << lookahead) & 15032385536) != 0)) {
            setState(69);
            parameterList();
        }
        setState(72);
        match(CloseRBracket);
        setState(76);
        _errHandler->sync(this);
        lookahead = _input->LA(1);
        while (lookahead == VarTypeWire || lookahead == VarTypeState) {
            setState(73);
            signalList();
            setState(78);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }
        setState(79);
        statementList();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ParameterListContext ------------------------------------------------------------------

TSyrecParser::ParameterListContext::ParameterListContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

std::vector<TSyrecParser::ParameterContext*> TSyrecParser::ParameterListContext::parameter() const {
    return getRuleContexts<ParameterContext>();
}

size_t TSyrecParser::ParameterListContext::getRuleIndex() const {
    return RuleParameterList;
}

TSyrecParser::ParameterListContext* TSyrecParser::parameterList() {
    auto* localCtx = _tracker.createInstance<ParameterListContext>(_ctx, getState());
    enterRule(localCtx, 6, RuleParameterList);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(81);
        parameter();
        setState(86);
        _errHandler->sync(this);
        std::size_t lookahead = _input->LA(1);
        while (lookahead == ParameterDelimiter) {
            setState(82);
            match(ParameterDelimiter);
            setState(83);
            parameter();
            setState(88);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ParameterContext ------------------------------------------------------------------

TSyrecParser::ParameterContext::ParameterContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::SignalDeclarationContext* TSyrecParser::ParameterContext::signalDeclaration() const {
    return getRuleContext<SignalDeclarationContext>(0);
}

tree::TerminalNode* TSyrecParser::ParameterContext::literalVarTypeIn() const {
    return getToken(VarTypeIn, 0);
}

tree::TerminalNode* TSyrecParser::ParameterContext::literalVarTypeOut() const {
    return getToken(VarTypeOut, 0);
}

tree::TerminalNode* TSyrecParser::ParameterContext::literalVarTypeInout() const {
    return getToken(VarTypeInout, 0);
}

size_t TSyrecParser::ParameterContext::getRuleIndex() const {
    return RuleParameter;
}

TSyrecParser::ParameterContext* TSyrecParser::parameter() {
    auto* localCtx = _tracker.createInstance<ParameterContext>(_ctx, getState());
    enterRule(localCtx, 8, RuleParameter);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(89);
        const std::size_t lookahead = _input->LA(1);
        if (!((((lookahead & ~0x3fULL) == 0) && ((1ULL << lookahead) & 15032385536) != 0))) {
            _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(90);
        signalDeclaration();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- SignalListContext ------------------------------------------------------------------

TSyrecParser::SignalListContext::SignalListContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

std::vector<TSyrecParser::SignalDeclarationContext*> TSyrecParser::SignalListContext::signalDeclaration() const {
    return getRuleContexts<SignalDeclarationContext>();
}

tree::TerminalNode* TSyrecParser::SignalListContext::literalVarTypeWire() const {
    return getToken(VarTypeWire, 0);
}

tree::TerminalNode* TSyrecParser::SignalListContext::literalVarTypeState() const {
    return getToken(VarTypeState, 0);
}

size_t TSyrecParser::SignalListContext::getRuleIndex() const {
    return RuleSignalList;
}

TSyrecParser::SignalListContext* TSyrecParser::signalList() {
    auto* localCtx = _tracker.createInstance<SignalListContext>(_ctx, getState());
    enterRule(localCtx, 10, RuleSignalList);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(92);
        std::size_t lookahead = _input->LA(1);
        if (lookahead != VarTypeWire && lookahead != VarTypeState) {
            _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(93);
        signalDeclaration();
        setState(98);
        _errHandler->sync(this);
        lookahead = _input->LA(1);
        while (lookahead == ParameterDelimiter) {
            setState(94);
            match(ParameterDelimiter);
            setState(95);
            signalDeclaration();
            setState(100);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- SignalDeclarationContext ------------------------------------------------------------------

TSyrecParser::SignalDeclarationContext::SignalDeclarationContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::SignalDeclarationContext::literalIdent() const {
    return getToken(Ident, 0);
}

size_t TSyrecParser::SignalDeclarationContext::getRuleIndex() const {
    return RuleSignalDeclaration;
}

TSyrecParser::SignalDeclarationContext* TSyrecParser::signalDeclaration() {
    auto* localCtx = _tracker.createInstance<SignalDeclarationContext>(_ctx, getState());
    enterRule(localCtx, 12, RuleSignalDeclaration);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(101);
        match(Ident);
        setState(107);
        _errHandler->sync(this);
        std::size_t lookahead = _input->LA(1);
        while (lookahead == OpenSBracket) {
            setState(102);
            match(OpenSBracket);
            setState(103);
            antlrcpp::downCast<SignalDeclarationContext*>(localCtx)->intToken = match(Int);
            antlrcpp::downCast<SignalDeclarationContext*>(localCtx)->dimensionTokens.push_back(antlrcpp::downCast<SignalDeclarationContext*>(localCtx)->intToken);
            setState(104);
            match(CloseSBracket);
            setState(109);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }
        setState(113);
        _errHandler->sync(this);

        lookahead = _input->LA(1);
        if (lookahead == OpenRBracket) {
            setState(110);
            match(OpenRBracket);
            setState(111);
            antlrcpp::downCast<SignalDeclarationContext*>(localCtx)->signalWidthToken = match(Int);
            setState(112);
            match(CloseRBracket);
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- StatementListContext ------------------------------------------------------------------

TSyrecParser::StatementListContext::StatementListContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

size_t TSyrecParser::StatementListContext::getRuleIndex() const {
    return RuleStatementList;
}

TSyrecParser::StatementListContext* TSyrecParser::statementList() {
    auto* localCtx = _tracker.createInstance<StatementListContext>(_ctx, getState());
    enterRule(localCtx, 14, RuleStatementList);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(115);
        antlrcpp::downCast<StatementListContext*>(localCtx)->statementContext = statement();
        antlrcpp::downCast<StatementListContext*>(localCtx)->stmts.push_back(antlrcpp::downCast<StatementListContext*>(localCtx)->statementContext);
        setState(120);
        _errHandler->sync(this);
        std::size_t lookahead = _input->LA(1);
        while (lookahead == StatementDelimiter) {
            setState(116);
            match(StatementDelimiter);
            setState(117);
            antlrcpp::downCast<StatementListContext*>(localCtx)->statementContext = statement();
            antlrcpp::downCast<StatementListContext*>(localCtx)->stmts.push_back(antlrcpp::downCast<StatementListContext*>(localCtx)->statementContext);
            setState(122);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- StatementContext ------------------------------------------------------------------

TSyrecParser::StatementContext::StatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::CallStatementContext* TSyrecParser::StatementContext::callStatement() const {
    return getRuleContext<CallStatementContext>(0);
}

TSyrecParser::ForStatementContext* TSyrecParser::StatementContext::forStatement() const {
    return getRuleContext<ForStatementContext>(0);
}

TSyrecParser::IfStatementContext* TSyrecParser::StatementContext::ifStatement() const {
    return getRuleContext<IfStatementContext>(0);
}

TSyrecParser::UnaryStatementContext* TSyrecParser::StatementContext::unaryStatement() const {
    return getRuleContext<UnaryStatementContext>(0);
}

TSyrecParser::AssignStatementContext* TSyrecParser::StatementContext::assignStatement() const {
    return getRuleContext<AssignStatementContext>(0);
}

TSyrecParser::SwapStatementContext* TSyrecParser::StatementContext::swapStatement() const {
    return getRuleContext<SwapStatementContext>(0);
}

TSyrecParser::SkipStatementContext* TSyrecParser::StatementContext::skipStatement() const {
    return getRuleContext<SkipStatementContext>(0);
}

size_t TSyrecParser::StatementContext::getRuleIndex() const {
    return RuleStatement;
}

TSyrecParser::StatementContext* TSyrecParser::statement() {
    auto* localCtx = _tracker.createInstance<StatementContext>(_ctx, getState());
    enterRule(localCtx, 16, RuleStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        setState(130);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 9, _ctx)) {
            case 1: {
                enterOuterAlt(localCtx, 1);
                setState(123);
                callStatement();
                break;
            }
            case 2: {
                enterOuterAlt(localCtx, 2);
                setState(124);
                forStatement();
                break;
            }
            case 3: {
                enterOuterAlt(localCtx, 3);
                setState(125);
                ifStatement();
                break;
            }
            case 4: {
                enterOuterAlt(localCtx, 4);
                setState(126);
                unaryStatement();
                break;
            }
            case 5: {
                enterOuterAlt(localCtx, 5);
                setState(127);
                assignStatement();
                break;
            }
            case 6: {
                enterOuterAlt(localCtx, 6);
                setState(128);
                swapStatement();
                break;
            }
            case 7: {
                enterOuterAlt(localCtx, 7);
                setState(129);
                skipStatement();
                break;
            }
            default:
                break;
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- CallStatementContext ------------------------------------------------------------------

TSyrecParser::CallStatementContext::CallStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::CallStatementContext::literalOpCall() const {
    return getToken(OpCall, 0);
}

tree::TerminalNode* TSyrecParser::CallStatementContext::literalOpUncall() const {
    return getToken(OpUncall, 0);
}

size_t TSyrecParser::CallStatementContext::getRuleIndex() const {
    return RuleCallStatement;
}

TSyrecParser::CallStatementContext* TSyrecParser::callStatement() {
    auto* localCtx = _tracker.createInstance<CallStatementContext>(_ctx, getState());
    enterRule(localCtx, 18, RuleCallStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(132);
        std::size_t lookahead = _input->LA(1);
        if (lookahead != OpCall && lookahead != OpUncall) {
            _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(133);
        antlrcpp::downCast<CallStatementContext*>(localCtx)->moduleIdent = match(Ident);
        setState(134);
        match(OpenRBracket);
        setState(135);
        antlrcpp::downCast<CallStatementContext*>(localCtx)->identToken = match(Ident);
        antlrcpp::downCast<CallStatementContext*>(localCtx)->callerArguments.push_back(antlrcpp::downCast<CallStatementContext*>(localCtx)->identToken);
        setState(140);
        _errHandler->sync(this);
        lookahead = _input->LA(1);
        while (lookahead == ParameterDelimiter) {
            setState(136);
            match(ParameterDelimiter);
            setState(137);
            antlrcpp::downCast<CallStatementContext*>(localCtx)->identToken = match(Ident);
            antlrcpp::downCast<CallStatementContext*>(localCtx)->callerArguments.push_back(antlrcpp::downCast<CallStatementContext*>(localCtx)->identToken);
            setState(142);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }
        setState(143);
        match(CloseRBracket);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- LoopVariableDefinitionContext ------------------------------------------------------------------

TSyrecParser::LoopVariableDefinitionContext::LoopVariableDefinitionContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::LoopVariableDefinitionContext::literalIdent() const {
    return getToken(Ident, 0);
}

size_t TSyrecParser::LoopVariableDefinitionContext::getRuleIndex() const {
    return RuleLoopVariableDefinition;
}

TSyrecParser::LoopVariableDefinitionContext* TSyrecParser::loopVariableDefinition() {
    auto* localCtx = _tracker.createInstance<LoopVariableDefinitionContext>(_ctx, getState());
    enterRule(localCtx, 20, RuleLoopVariableDefinition);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(145);
        match(LoopVariablePrefix);
        setState(146);
        antlrcpp::downCast<LoopVariableDefinitionContext*>(localCtx)->variableIdent = match(Ident);
        setState(147);
        match(OpEqual);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- LoopStepsizeDefinitionContext ------------------------------------------------------------------

TSyrecParser::LoopStepsizeDefinitionContext::LoopStepsizeDefinitionContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::NumberContext* TSyrecParser::LoopStepsizeDefinitionContext::number() const {
    return getRuleContext<NumberContext>(0);
}

tree::TerminalNode* TSyrecParser::LoopStepsizeDefinitionContext::literalOpMinus() const {
    return getToken(OpMinus, 0);
}

size_t TSyrecParser::LoopStepsizeDefinitionContext::getRuleIndex() const {
    return RuleLoopStepsizeDefinition;
}

TSyrecParser::LoopStepsizeDefinitionContext* TSyrecParser::loopStepsizeDefinition() {
    auto* localCtx = _tracker.createInstance<LoopStepsizeDefinitionContext>(_ctx, getState());
    enterRule(localCtx, 22, RuleLoopStepsizeDefinition);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(149);
        match(KeywordStep);
        setState(151);
        _errHandler->sync(this);
        if (const std::size_t lookahead = _input->LA(1); lookahead == OpMinus) {
            setState(150);
            match(OpMinus);
        }
        setState(153);
        number();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ForStatementContext ------------------------------------------------------------------

TSyrecParser::ForStatementContext::ForStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::ForStatementContext::literalKeywordFor() const {
    return getToken(KeywordFor, 0);
}

TSyrecParser::StatementListContext* TSyrecParser::ForStatementContext::statementList() const {
    return getRuleContext<StatementListContext>(0);
}

TSyrecParser::LoopStepsizeDefinitionContext* TSyrecParser::ForStatementContext::loopStepsizeDefinition() const {
    return getRuleContext<LoopStepsizeDefinitionContext>(0);
}

TSyrecParser::LoopVariableDefinitionContext* TSyrecParser::ForStatementContext::loopVariableDefinition() const {
    return getRuleContext<LoopVariableDefinitionContext>(0);
}

size_t TSyrecParser::ForStatementContext::getRuleIndex() const {
    return RuleForStatement;
}

TSyrecParser::ForStatementContext* TSyrecParser::forStatement() {
    auto* localCtx = _tracker.createInstance<ForStatementContext>(_ctx, getState());
    enterRule(localCtx, 24, RuleForStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(155);
        match(KeywordFor);
        setState(162);
        _errHandler->sync(this);

        if (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 13, _ctx) == 1) {
            setState(157);
            _errHandler->sync(this);

            if (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 12, _ctx) == 1) {
                setState(156);
                loopVariableDefinition();
            }
            setState(159);
            antlrcpp::downCast<ForStatementContext*>(localCtx)->startValue = number();
            setState(160);
            match(KeywordTo);
        }
        setState(164);
        antlrcpp::downCast<ForStatementContext*>(localCtx)->endValue = number();
        setState(166);
        _errHandler->sync(this);

        if (const std::size_t lookahead = _input->LA(1); lookahead == KeywordStep) {
            setState(165);
            loopStepsizeDefinition();
        }
        setState(168);
        match(KeywordDo);
        setState(169);
        statementList();
        setState(170);
        match(KeywordRof);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- IfStatementContext ------------------------------------------------------------------

TSyrecParser::IfStatementContext::IfStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

size_t TSyrecParser::IfStatementContext::getRuleIndex() const {
    return RuleIfStatement;
}

TSyrecParser::IfStatementContext* TSyrecParser::ifStatement() {
    auto* localCtx = _tracker.createInstance<IfStatementContext>(_ctx, getState());
    enterRule(localCtx, 26, RuleIfStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(172);
        match(KeywordIf);
        setState(173);
        antlrcpp::downCast<IfStatementContext*>(localCtx)->guardCondition = expression();
        setState(174);
        match(KeywordThen);
        setState(175);
        antlrcpp::downCast<IfStatementContext*>(localCtx)->trueBranchStmts = statementList();
        setState(176);
        match(KeywordElse);
        setState(177);
        antlrcpp::downCast<IfStatementContext*>(localCtx)->falseBranchStmts = statementList();
        setState(178);
        match(KeywordFi);
        setState(179);
        antlrcpp::downCast<IfStatementContext*>(localCtx)->matchingGuardExpression = expression();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- UnaryStatementContext ------------------------------------------------------------------

TSyrecParser::UnaryStatementContext::UnaryStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::SignalContext* TSyrecParser::UnaryStatementContext::signal() const {
    return getRuleContext<SignalContext>(0);
}

tree::TerminalNode* TSyrecParser::UnaryStatementContext::literalOpInvertAssign() const {
    return getToken(OpInvertAssign, 0);
}

tree::TerminalNode* TSyrecParser::UnaryStatementContext::literalOpIncrementAssign() const {
    return getToken(OpIncrementAssign, 0);
}

tree::TerminalNode* TSyrecParser::UnaryStatementContext::literalOpDecrementAssign() const {
    return getToken(OpDecrementAssign, 0);
}

size_t TSyrecParser::UnaryStatementContext::getRuleIndex() const {
    return RuleUnaryStatement;
}

TSyrecParser::UnaryStatementContext* TSyrecParser::unaryStatement() {
    auto* localCtx = _tracker.createInstance<UnaryStatementContext>(_ctx, getState());
    enterRule(localCtx, 28, RuleUnaryStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(181);
        antlrcpp::downCast<UnaryStatementContext*>(localCtx)->unaryOp = _input->LT(1);
        const std::size_t lookahead                                   = _input->LA(1);
        if (!((((lookahead & ~0x3fULL) == 0) && ((1ULL << lookahead) & 14) != 0))) {
            antlrcpp::downCast<UnaryStatementContext*>(localCtx)->unaryOp = _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(182);
        signal();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- AssignStatementContext ------------------------------------------------------------------

TSyrecParser::AssignStatementContext::AssignStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::SignalContext* TSyrecParser::AssignStatementContext::signal() const {
    return getRuleContext<SignalContext>(0);
}

TSyrecParser::ExpressionContext* TSyrecParser::AssignStatementContext::expression() const {
    return getRuleContext<ExpressionContext>(0);
}

tree::TerminalNode* TSyrecParser::AssignStatementContext::literalOpAddAssign() const {
    return getToken(OpAddAssign, 0);
}

tree::TerminalNode* TSyrecParser::AssignStatementContext::literalOpSubAssign() const {
    return getToken(OpSubAssign, 0);
}

tree::TerminalNode* TSyrecParser::AssignStatementContext::literalOpXorAssign() const {
    return getToken(OpXorAssign, 0);
}

size_t TSyrecParser::AssignStatementContext::getRuleIndex() const {
    return RuleAssignStatement;
}

TSyrecParser::AssignStatementContext* TSyrecParser::assignStatement() {
    auto* localCtx = _tracker.createInstance<AssignStatementContext>(_ctx, getState());
    enterRule(localCtx, 30, RuleAssignStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(184);
        signal();
        setState(185);
        antlrcpp::downCast<AssignStatementContext*>(localCtx)->assignmentOp = _input->LT(1);
        const std::size_t lookahead                                         = _input->LA(1);
        if (!((((lookahead & ~0x3fULL) == 0) && ((1ULL << lookahead) & 112) != 0))) {
            antlrcpp::downCast<AssignStatementContext*>(localCtx)->assignmentOp = _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(186);
        expression();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- SwapStatementContext ------------------------------------------------------------------

TSyrecParser::SwapStatementContext::SwapStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

size_t TSyrecParser::SwapStatementContext::getRuleIndex() const {
    return RuleSwapStatement;
}

TSyrecParser::SwapStatementContext* TSyrecParser::swapStatement() {
    auto* localCtx = _tracker.createInstance<SwapStatementContext>(_ctx, getState());
    enterRule(localCtx, 32, RuleSwapStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(188);
        antlrcpp::downCast<SwapStatementContext*>(localCtx)->lhsOperand = signal();
        setState(189);
        match(OpSwap);
        setState(190);
        antlrcpp::downCast<SwapStatementContext*>(localCtx)->rhsOperand = signal();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- SkipStatementContext ------------------------------------------------------------------

TSyrecParser::SkipStatementContext::SkipStatementContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

size_t TSyrecParser::SkipStatementContext::getRuleIndex() const {
    return RuleSkipStatement;
}

TSyrecParser::SkipStatementContext* TSyrecParser::skipStatement() {
    auto* localCtx = _tracker.createInstance<SkipStatementContext>(_ctx, getState());
    enterRule(localCtx, 34, RuleSkipStatement);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(192);
        match(KeywordSkip);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- SignalContext ------------------------------------------------------------------

TSyrecParser::SignalContext::SignalContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::SignalContext::literalIdent() const {
    return getToken(Ident, 0);
}

size_t TSyrecParser::SignalContext::getRuleIndex() const {
    return RuleSignal;
}

TSyrecParser::SignalContext* TSyrecParser::signal() {
    auto* localCtx = _tracker.createInstance<SignalContext>(_ctx, getState());
    enterRule(localCtx, 36, RuleSignal);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(194);
        match(Ident);
        setState(201);
        _errHandler->sync(this);
        std::size_t lookahead = _input->LA(1);
        while (lookahead == OpenSBracket) {
            setState(195);
            match(OpenSBracket);
            setState(196);
            antlrcpp::downCast<SignalContext*>(localCtx)->expressionContext = expression();
            antlrcpp::downCast<SignalContext*>(localCtx)->accessedDimensions.push_back(antlrcpp::downCast<SignalContext*>(localCtx)->expressionContext);
            setState(197);
            match(CloseSBracket);
            setState(203);
            _errHandler->sync(this);
            lookahead = _input->LA(1);
        }
        setState(210);
        _errHandler->sync(this);

        lookahead = _input->LA(1);
        if (lookahead == BitrangeStartPrefix) {
            setState(204);
            match(BitrangeStartPrefix);
            setState(205);
            antlrcpp::downCast<SignalContext*>(localCtx)->bitStart = number();
            setState(208);
            _errHandler->sync(this);

            lookahead = _input->LA(1);
            if (lookahead == BitrangEndPrefix) {
                setState(206);
                match(BitrangEndPrefix);
                setState(207);
                antlrcpp::downCast<SignalContext*>(localCtx)->bitRangeEnd = number();
            }
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ExpressionContext ------------------------------------------------------------------

TSyrecParser::ExpressionContext::ExpressionContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

size_t TSyrecParser::ExpressionContext::getRuleIndex() const {
    return RuleExpression;
}

void TSyrecParser::ExpressionContext::copyFrom(ExpressionContext* ctx) {
    ParserRuleContext::copyFrom(ctx);
}

//----------------- ExpressionFromSignalContext ------------------------------------------------------------------

TSyrecParser::SignalContext* TSyrecParser::ExpressionFromSignalContext::signal() const {
    return getRuleContext<SignalContext>(0);
}

TSyrecParser::ExpressionFromSignalContext::ExpressionFromSignalContext(ExpressionContext* ctx) {
    copyFrom(ctx);
}
//----------------- ExpressionFromBinaryExpressionContext ------------------------------------------------------------------

TSyrecParser::BinaryExpressionContext* TSyrecParser::ExpressionFromBinaryExpressionContext::binaryExpression() const {
    return getRuleContext<BinaryExpressionContext>(0);
}

TSyrecParser::ExpressionFromBinaryExpressionContext::ExpressionFromBinaryExpressionContext(ExpressionContext* ctx) {
    copyFrom(ctx);
}
//----------------- ExpressionFromNumberContext ------------------------------------------------------------------

TSyrecParser::NumberContext* TSyrecParser::ExpressionFromNumberContext::number() const {
    return getRuleContext<NumberContext>(0);
}

TSyrecParser::ExpressionFromNumberContext::ExpressionFromNumberContext(ExpressionContext* ctx) {
    copyFrom(ctx);
}
//----------------- ExpressionFromUnaryExpressionContext ------------------------------------------------------------------

TSyrecParser::UnaryExpressionContext* TSyrecParser::ExpressionFromUnaryExpressionContext::unaryExpression() const {
    return getRuleContext<UnaryExpressionContext>(0);
}

TSyrecParser::ExpressionFromUnaryExpressionContext::ExpressionFromUnaryExpressionContext(ExpressionContext* ctx) {
    copyFrom(ctx);
}
//----------------- ExpressionFromShiftExpressionContext ------------------------------------------------------------------

TSyrecParser::ShiftExpressionContext* TSyrecParser::ExpressionFromShiftExpressionContext::shiftExpression() const {
    return getRuleContext<ShiftExpressionContext>(0);
}

TSyrecParser::ExpressionFromShiftExpressionContext::ExpressionFromShiftExpressionContext(ExpressionContext* ctx) {
    copyFrom(ctx);
}

TSyrecParser::ExpressionContext* TSyrecParser::expression() {
    auto* localCtx = _tracker.createInstance<ExpressionContext>(_ctx, getState());
    enterRule(localCtx, 38, RuleExpression);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        setState(217);
        _errHandler->sync(this);
        switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 18, _ctx)) {
            case 1: {
                localCtx = _tracker.createInstance<ExpressionFromNumberContext>(localCtx);
                enterOuterAlt(localCtx, 1);
                setState(212);
                number();
                break;
            }

            case 2: {
                localCtx = _tracker.createInstance<ExpressionFromSignalContext>(localCtx);
                enterOuterAlt(localCtx, 2);
                setState(213);
                signal();
                break;
            }

            case 3: {
                localCtx = _tracker.createInstance<ExpressionFromBinaryExpressionContext>(localCtx);
                enterOuterAlt(localCtx, 3);
                setState(214);
                binaryExpression();
                break;
            }

            case 4: {
                localCtx = _tracker.createInstance<ExpressionFromUnaryExpressionContext>(localCtx);
                enterOuterAlt(localCtx, 4);
                setState(215);
                unaryExpression();
                break;
            }

            case 5: {
                localCtx = _tracker.createInstance<ExpressionFromShiftExpressionContext>(localCtx);
                enterOuterAlt(localCtx, 5);
                setState(216);
                shiftExpression();
                break;
            }

            default:
                break;
        }

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- BinaryExpressionContext ------------------------------------------------------------------

TSyrecParser::BinaryExpressionContext::BinaryExpressionContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpPlus() const {
    return getToken(OpPlus, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpMinus() const {
    return getToken(OpMinus, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpMultiply() const {
    return getToken(OpMultiply, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpDivision() const {
    return getToken(OpDivision, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpModulo() const {
    return getToken(OpModulo, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpUpperBitMultiply() const {
    return getToken(OpUpperBitMultiply, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpLogicalAnd() const {
    return getToken(OpLogicalAnd, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpLogicalOr() const {
    return getToken(OpLogicalOr, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpBitwiseAnd() const {
    return getToken(OpBitwiseAnd, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpBitwiseOr() const {
    return getToken(OpBitwiseOr, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpBitwiseXor() const {
    return getToken(OpBitwiseXor, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpLessThan() const {
    return getToken(OpLessThan, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpGreaterThan() const {
    return getToken(OpGreaterThan, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpEqual() const {
    return getToken(OpEqual, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpNotEqual() const {
    return getToken(OpNotEqual, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpLessOrEqual() const {
    return getToken(OpLessOrEqual, 0);
}

tree::TerminalNode* TSyrecParser::BinaryExpressionContext::literalOpGreaterOrEqual() const {
    return getToken(OpGreaterOrEqual, 0);
}

size_t TSyrecParser::BinaryExpressionContext::getRuleIndex() const {
    return RuleBinaryExpression;
}

TSyrecParser::BinaryExpressionContext* TSyrecParser::binaryExpression() {
    auto* localCtx = _tracker.createInstance<BinaryExpressionContext>(_ctx, getState());
    enterRule(localCtx, 40, RuleBinaryExpression);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(219);
        match(OpenRBracket);
        setState(220);
        antlrcpp::downCast<BinaryExpressionContext*>(localCtx)->lhsOperand = expression();
        setState(221);
        antlrcpp::downCast<BinaryExpressionContext*>(localCtx)->binaryOperation = _input->LT(1);
        const std::size_t lookahead                                             = _input->LA(1);
        if (!((((lookahead & ~0x3fULL) == 0) && ((1ULL << lookahead) & 452927360) != 0))) {
            antlrcpp::downCast<BinaryExpressionContext*>(localCtx)->binaryOperation = _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(222);
        antlrcpp::downCast<BinaryExpressionContext*>(localCtx)->rhsOperand = expression();
        setState(223);
        match(CloseRBracket);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- UnaryExpressionContext ------------------------------------------------------------------

TSyrecParser::UnaryExpressionContext::UnaryExpressionContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::ExpressionContext* TSyrecParser::UnaryExpressionContext::expression() const {
    return getRuleContext<ExpressionContext>(0);
}

tree::TerminalNode* TSyrecParser::UnaryExpressionContext::literalOpLogicalNegation() const {
    return getToken(OpLogicalNegation, 0);
}

tree::TerminalNode* TSyrecParser::UnaryExpressionContext::literalOpBitwiseNegation() const {
    return getToken(OpBitwiseNegation, 0);
}

size_t TSyrecParser::UnaryExpressionContext::getRuleIndex() const {
    return RuleUnaryExpression;
}

TSyrecParser::UnaryExpressionContext* TSyrecParser::unaryExpression() {
    auto* localCtx = _tracker.createInstance<UnaryExpressionContext>(_ctx, getState());
    enterRule(localCtx, 42, RuleUnaryExpression);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(225);
        antlrcpp::downCast<UnaryExpressionContext*>(localCtx)->unaryOperation = _input->LT(1);
        const std::size_t lookahead                                           = _input->LA(1);
        if (lookahead != OpLogicalNegation && lookahead != OpBitwiseNegation) {
            antlrcpp::downCast<UnaryExpressionContext*>(localCtx)->unaryOperation = _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(226);
        expression();

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

//----------------- ShiftExpressionContext ------------------------------------------------------------------

TSyrecParser::ShiftExpressionContext::ShiftExpressionContext(ParserRuleContext* parent, size_t invokingState):
    ParserRuleContext(parent, invokingState) {
}

TSyrecParser::ExpressionContext* TSyrecParser::ShiftExpressionContext::expression() const {
    return getRuleContext<ExpressionContext>(0);
}

TSyrecParser::NumberContext* TSyrecParser::ShiftExpressionContext::number() const {
    return getRuleContext<NumberContext>(0);
}

tree::TerminalNode* TSyrecParser::ShiftExpressionContext::literalOpRightShift() const {
    return getToken(OpRightShift, 0);
}

tree::TerminalNode* TSyrecParser::ShiftExpressionContext::literalOpLeftShift() const {
    return getToken(OpLeftShift, 0);
}

size_t TSyrecParser::ShiftExpressionContext::getRuleIndex() const {
    return RuleShiftExpression;
}

TSyrecParser::ShiftExpressionContext* TSyrecParser::shiftExpression() {
    auto* localCtx = _tracker.createInstance<ShiftExpressionContext>(_ctx, getState());
    enterRule(localCtx, 44, RuleShiftExpression);

    // We are assuming that the parser will be built with the standard >= C++17
    auto onExit = finally([&] {
        exitRule();
    });

    try {
        enterOuterAlt(localCtx, 1);
        setState(228);
        match(OpenRBracket);
        setState(229);
        expression();
        setState(230);
        antlrcpp::downCast<ShiftExpressionContext*>(localCtx)->shiftOperation = _input->LT(1);
        if (const std::size_t lookahead = _input->LA(1); lookahead != OpLeftShift && lookahead != OpRightShift) {
            antlrcpp::downCast<ShiftExpressionContext*>(localCtx)->shiftOperation = _errHandler->recoverInline(this);
        } else {
            _errHandler->reportMatch(this);
            consume();
        }
        setState(231);
        number();
        setState(232);
        match(CloseRBracket);

    } catch (RecognitionException& e) {
        _errHandler->reportError(this, e);
        localCtx->exception = std::current_exception();
        _errHandler->recover(this, localCtx->exception);
    }

    return localCtx;
}

void TSyrecParser::initialize() {
    call_once(parserSingletonInitializationSyncFlag, initializeStaticParserData);
}
