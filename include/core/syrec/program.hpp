/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once

#include "core/configurable_options.hpp"
#include "core/syrec/module.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace syrec {
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
        std::string read(const std::string& filename, const ConfigurableOptions& settings = ConfigurableOptions{});

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
        std::string readFromString(const std::string_view& stringifiedProgram, const ConfigurableOptions& settings = ConfigurableOptions{});

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
        bool                                            readFile(const std::string& filename, const ConfigurableOptions& settings, std::string& error);
        bool                                            readProgramFromString(const std::string_view& content, const ConfigurableOptions& settings, std::string&);
        [[nodiscard]] static std::optional<std::string> tryReadFileContent(const std::string& filename, std::string* foundFileHandlingErrors);
    };

} // namespace syrec
