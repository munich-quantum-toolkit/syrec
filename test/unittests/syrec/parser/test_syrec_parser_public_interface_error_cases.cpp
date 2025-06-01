/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/syrec/parser/utils/parser_messages_container.hpp"
#include "core/syrec/program.hpp"

#include <gtest/gtest.h>
#include <string>

namespace {
    constexpr auto PATH_TO_SYREC_CIRCUITS = "./circuits";

    class SyrecParserPublicInterfaceErrorCasesTestFixture: public testing::Test {
    protected:
        syrec::Program parserInstance;

        /**
         * @brief Check that parsing SyReC program from ill-formed path does not crash parser.
         *
         * Since both parser as well as file-handling errors are combined in the return value of the read(...) functions of the parser public interface
         * with the format of the latter not being specified, our check whether the processing of an ill-formed file input was OK
         * is reduced to a check whether the read(...) call did not throw and returned an non-empty error.
         *
         * @param pathToBeProcessedFile The path to the SyReC circuit to process.
         */
        void assertReadFromFileDoesNotCrashParser(const std::string& pathToBeProcessedFile) {
            std::string foundErrors;
            ASSERT_NO_FATAL_FAILURE(foundErrors = parserInstance.read(pathToBeProcessedFile)) << "Processing of not existing file should not crash parser";
            ASSERT_FALSE(foundErrors.empty());
        }
    };
} // namespace

TEST_F(SyrecParserPublicInterfaceErrorCasesTestFixture, ReadProgramFromNotExistingPathCausesError) {
    assertReadFromFileDoesNotCrashParser("/notExistingPath/notExistingFile.src");
}

TEST_F(SyrecParserPublicInterfaceErrorCasesTestFixture, ReadProgramFromNotExistingFileCausesError) {
    const std::string pathToNotExistingFile = PATH_TO_SYREC_CIRCUITS + std::string("/notExistingCircuit.src");
    assertReadFromFileDoesNotCrashParser(pathToNotExistingFile);
}

TEST_F(SyrecParserPublicInterfaceErrorCasesTestFixture, ReadProgramFromMalformedPathCausesError) {
    assertReadFromFileDoesNotCrashParser("-notExistingPath/notExistingFile.src");
}

TEST_F(SyrecParserPublicInterfaceErrorCasesTestFixture, ReadProgramFromEmptyPathCausesError) {
    assertReadFromFileDoesNotCrashParser("");
}

TEST_F(SyrecParserPublicInterfaceErrorCasesTestFixture, ReadProgramFromMalformedFilenameCausesError) {
    const std::string pathToExistingFileWithMalformedFilename = PATH_TO_SYREC_CIRCUITS + std::string("/alu_2-src");
    assertReadFromFileDoesNotCrashParser(pathToExistingFileWithMalformedFilename);
}

TEST_F(SyrecParserPublicInterfaceErrorCasesTestFixture, ReadFromEmptyStringCausesError) {
    const std::string foundErrors   = parserInstance.readFromString("");
    const std::string expectedError = syrec_parser::Message(syrec_parser::Message::Type::Error, "SYNTAX", syrec_parser::Message::Position(1, 0), "mismatched input '<EOF>' expecting 'module'").stringify();
    ASSERT_EQ(expectedError, foundErrors);
}
