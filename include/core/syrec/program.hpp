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

#include "core/syrec/module.hpp"
#include "parser/utils/syrec_operation_utils.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace syrec {
    struct ReadProgramSettings {
        explicit ReadProgramSettings(unsigned bitwidth = 32U, utils::IntegerConstantTruncationOperation integerConstantTruncationOperation = utils::IntegerConstantTruncationOperation::BitwiseAnd,
                                     bool allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess = false):
            defaultBitwidth(bitwidth), integerConstantTruncationOperation(integerConstantTruncationOperation),
            allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess(allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess) {}

        /**
         * @brief Defines the default variable bitwidth used by the SyReC parser for variables whose bitwidth specification was omitted.
         */
        unsigned defaultBitwidth;

        /**
         * @brief The operation used by the parser to perform the truncation of constant values.
         *
         * @details When are constant values truncated in the parser: <br>
         * I.   The expected bitwidth of the operands of a binary expression is known and only one operand of the binary expression evaluates to a constant. <br>
         * II.  The expected bitwidth of the operands of a shift expression is known and the to be shifted value (i.e. the left hand side operand of the shift expression (evaluates to a constant). <br>
         * III. The expected bitwidth of the operands of an assignment is known and the right hand side of the assignment evaluates to a constant. <br><br>
         * In the following cases truncation of constant values is not performed: <br>
         * I.   The shift amount (i.e. the right hand side operand of a shift expression). <br>
         * II.  The bitwidth of the operand of a binary/shift expression is not known. <br>
         * III. The bitwidth of the operands of an assignment are not known. <br>
         * IV.  During the evaluation of unary/binary/shift expressions containing only constant values.
         */
        utils::IntegerConstantTruncationOperation integerConstantTruncationOperation;

        /**
         * @brief Is access on the assigned to variable parts allowed in the dimension access components of variable accesses defined in the operands of the assignment.
         *
         * @details An issue encountered during the implementation of the parser was to define whether the variable parts of the be assigned variable can be used in its own dimension access (self-reference) <br>
         * or in the one on the other side of the assignment. <br>
         * An example for a 'self-reference' could be the simple assignment:  <br>
         * I.       module main(inout a[2](4)) ++= a[a[0]]  <br>
         * While a reference on the other side of an assignment could look like:  <br>
         * II.      module main(inout a[2](4), out b[2](4)) a[b[0]] <=> b[0] or a[0] += b[a[0]]  <br><br>
         * To guarantee the reversibility of an assignment, the parser checks that the assigned to variable parts are not accessed on the other side of an assignment thus an assignment of the form:  <br>
         * III.     module main(inout a[2](4)) a[0] += (a[0] + 2)  <br>
         * is not allowed, while the reversibility of the examples I. and II. depend on the implemented synthesis of the expressions defining the accessed value of the dimensions. Thus, the user needs to define whether variable accesses  <br>
         * as shown in the examples I. and II. are allowed via the corresponding flag in the parser configuration. However, the parser can only verify this check if all indices of the assigned to variable access evaluate to constant values with  <br>
         * the same requirement for the indices for any potentially violating variable access in a dimension access (thus the parser will only report cases where it can prove that an overlap exists).  <br>
         * Examples that are not reported as overlaps are:  <br>
         * I.      module main(inout a[2](4)) ++= a[a[0]]  <br>
         * II.     module main(inout a[2](4)) for $i = 0 to 1 do a[$i] += a[0] rof  <br>
         * III.    module main(inout a[2](4)) for $i = 0 to 1 do a[0] += a[$i] rof
         */
        bool allowAccessOnAssignedToVariablePartsInDimensionAccessOfVariableAccess;
    };

    class Program {
    public:
        Program() = default;

        void addModule(const Module::ptr& module) {
            modulesVec.emplace_back(module);
        }

        [[nodiscard]] const Module::vec& modules() const {
            return modulesVec;
        }

        [[nodiscard]] Module::ptr findModule(const std::string& name) const {
            for (const Module::ptr& p: modulesVec) {
                if (p->name == name) {
                    return p;
                }
            }

            return {};
        }

        /**
         * @brief Read and parse a SyReC program from a file.
         *
         * This function call performs both the lexical parsing
         * as well as the semantic analysis of the program which
         * creates the corresponding C++ constructs for the
         * program.
         *
         * @param filename Defines where the SyReC program to process is located.
         * @param settings The configuration to use by the parser.
         * @return A std::string containing the list of errors found during the processing of the file or the parsing of the SyReC program.
         */
        std::string read(const std::string& filename, ReadProgramSettings settings = ReadProgramSettings{});

        /**
         * @brief Read and parse a SyReC program from a string.
         *
         * This function call performs both the lexical parsing
         * as well as the semantic analysis of the program which
         * creates the corresponding C++ constructs for the
         * program.
         *
         * @param stringifiedProgram A stringified SyReC program string.
         * @param settings The configuration to use by the parser.
         * @return A std::string containing the list of errors found during the parsing of the SyReC program.
         */
        std::string readFromString(const std::string_view& stringifiedProgram, ReadProgramSettings settings = ReadProgramSettings{});

    private:
        Module::vec modulesVec;

        /**
        * @brief Parser for a SyReC program
        *
        * This function call performs both the lexical parsing
        * as well as the semantic analysis of the program which
        * creates the corresponding C++ constructs for the
        * program.
        *
        * @param filename File-name to parse from
        * @param settings Settings
        * @param error Error Message, in case the function returns false
        *
        * @return true if parsing was successful, otherwise false
        */
        bool                                            readFile(const std::string& filename, ReadProgramSettings settings, std::string& error);
        bool                                            readProgramFromString(const std::string_view& content, const ReadProgramSettings& settings, std::string&);
        [[nodiscard]] static std::optional<std::string> tryReadFileContent(const std::string& filename, std::string* foundFileHandlingErrors);
    };

} // namespace syrec
