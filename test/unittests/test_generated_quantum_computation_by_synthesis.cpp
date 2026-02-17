/*
 * Copyright (c) 2023 - 2026 Chair for Design Automation, TUM
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "algorithms/synthesis/internal_qubit_label_builder.hpp"
#include "algorithms/synthesis/syrec_cost_aware_synthesis.hpp"
#include "algorithms/synthesis/syrec_line_aware_synthesis.hpp"
#include "core/annotatable_quantum_computation.hpp"
#include "core/syrec/program.hpp"
#include "ir/Definitions.hpp"
#include "ir/Register.hpp"

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

using namespace syrec;

// The current tests do not cover the following functionality (when referring to all generated qubits we include the qubits generated for intermediate results and assume that tests requiring ancillary qubits for intermediate results exist):
// - Fetching the internal qubit labels of all generated qubits of the synthesized circuit
// - Fetching the user declared qubit labels of all generated qubits of the synthesized circuit
// - Fetching the inlined qubit information of all generated qubits of the synthesized circuit
// - Fetching the inlined qubit information of all generated qubits of the synthesized circuit if the required flag in the configurable options is not enabled

template<typename T>
class GeneratedQuantumComputationBySynthesisTestFixture: public testing::Test {
public:
    struct ExpectedDataOfGeneratedQReg {
        std::string                                    expectedQRegLabel;
        AnnotatableQuantumComputation::QubitIndexRange expectedCoveredQubitRange;
        AnnotatableQuantumComputation::QubitType       expectedTypeOfQubitsStoredInQReg;

        explicit ExpectedDataOfGeneratedQReg(std::string expectedQRegLabel, const AnnotatableQuantumComputation::QubitIndexRange expectedCoveredQubitRange, const AnnotatableQuantumComputation::QubitType expectedTypeOfQubitsStoredInQReg):
            expectedQRegLabel(std::move(expectedQRegLabel)), expectedCoveredQubitRange(expectedCoveredQubitRange), expectedTypeOfQubitsStoredInQReg(expectedTypeOfQubitsStoredInQReg) {}
    };

    void SetUp() override {
        static_assert(std::is_same_v<T, CostAwareSynthesis> || std::is_same_v<T, LineAwareSynthesis>);
    }

    AnnotatableQuantumComputation annotatableQuantumComputation;

    static void parseInputCircuitPerformSynthesisAndAssertSuccess(const std::string_view& stringifiedSyrecProgram, AnnotatableQuantumComputation& annotatableQuantumComputation) {
        Program syrecProgramInstance;

        std::string errorsOfReadInputCircuit;
        ASSERT_NO_FATAL_FAILURE(errorsOfReadInputCircuit = syrecProgramInstance.readFromString(stringifiedSyrecProgram, syrec::ConfigurableOptions()));
        ASSERT_TRUE(errorsOfReadInputCircuit.empty()) << "Expected no errors in input circuits but actually found the following: " << errorsOfReadInputCircuit;

        if constexpr (std::is_same_v<T, CostAwareSynthesis>) {
            ASSERT_TRUE(CostAwareSynthesis::synthesize(annotatableQuantumComputation, syrecProgramInstance)) << "Cost aware synthesis failed!";
        } else {
            ASSERT_TRUE(LineAwareSynthesis::synthesize(annotatableQuantumComputation, syrecProgramInstance)) << "Line aware synthesis failed!";
        }
    }

    static void assertGeneratedQuantumRegisterDataMatchesExpectedOne(const AnnotatableQuantumComputation& annotatableQuantumComputation, const ExpectedDataOfGeneratedQReg& expectedQRegData) {
        const auto& iteratorToMatchingQRegForLabel = annotatableQuantumComputation.getQuantumRegisters().find(expectedQRegData.expectedQRegLabel);
        ASSERT_NE(iteratorToMatchingQRegForLabel, annotatableQuantumComputation.getQuantumRegisters().cend()) << "Could not find matching quantum register with expected label " << expectedQRegData.expectedQRegLabel;

        const qc::QuantumRegister& matchingQRegForLabel = iteratorToMatchingQRegForLabel->second;
        ASSERT_EQ(matchingQRegForLabel.getStartIndex(), expectedQRegData.expectedCoveredQubitRange.firstQubitIndex);
        ASSERT_EQ(matchingQRegForLabel.getEndIndex(), expectedQRegData.expectedCoveredQubitRange.lastQubitIndex);
        ASSERT_NO_FATAL_FAILURE(assertQubitsInRangeAreCorrectlyMarkedAsQubitsOfType(annotatableQuantumComputation, expectedQRegData.expectedCoveredQubitRange, expectedQRegData.expectedTypeOfQubitsStoredInQReg));
    }

    static void assertQubitsInRangeAreCorrectlyMarkedAsQubitsOfType(const AnnotatableQuantumComputation& annotatableQuantumComputation, const AnnotatableQuantumComputation::QubitIndexRange& qubitIndexRangeToCheck, const AnnotatableQuantumComputation::QubitType expectedQubitType) {
        const bool shouldQubitsBeMarkedAsGarbage   = expectedQubitType == AnnotatableQuantumComputation::QubitType::Garbage;
        const bool shouldQubitsBeMarkedAsAncillary = expectedQubitType == AnnotatableQuantumComputation::QubitType::Ancillary;

        for (qc::Qubit qubit = qubitIndexRangeToCheck.firstQubitIndex; qubit <= qubitIndexRangeToCheck.lastQubitIndex; ++qubit) {
            ASSERT_EQ(shouldQubitsBeMarkedAsAncillary | shouldQubitsBeMarkedAsGarbage, annotatableQuantumComputation.logicalQubitIsGarbage(qubit)) << "Expected is garbage classification of qubit: " << shouldQubitsBeMarkedAsGarbage << " but was actually: " << annotatableQuantumComputation.logicalQubitIsGarbage(qubit);
            ASSERT_EQ(shouldQubitsBeMarkedAsAncillary, annotatableQuantumComputation.logicalQubitIsAncillary(qubit)) << "Expected is ancillary classification of qubit: " << shouldQubitsBeMarkedAsAncillary << " but was actually: " << annotatableQuantumComputation.logicalQubitIsAncillary(qubit);
        }
    }
};
TYPED_TEST_SUITE_P(GeneratedQuantumComputationBySynthesisTestFixture);

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithInParameters) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module main(in a(4), in b[2][3](2)) skip", this->annotatableQuantumComputation));
    ASSERT_EQ(2U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(16U, this->annotatableQuantumComputation.getNqubits());

    const auto firstInParameterExpectedQRegData  = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("a", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto secondInParameterExpectedQRegData = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("b", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 15U}), AnnotatableQuantumComputation::QubitType::Garbage);
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, firstInParameterExpectedQRegData));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, secondInParameterExpectedQRegData));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithOutParameters) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module main(out a(4), out b[2][3](2)) skip", this->annotatableQuantumComputation));
    ASSERT_EQ(2U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(16U, this->annotatableQuantumComputation.getNqubits());

    const auto firstOutParameterExpectedQRegData  = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("a", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto secondOutParameterExpectedQRegData = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("b", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 15U}), AnnotatableQuantumComputation::QubitType::Data);
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, firstOutParameterExpectedQRegData));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, secondOutParameterExpectedQRegData));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithInoutParameters) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module main(inout a(4), inout b[2][3](2)) skip", this->annotatableQuantumComputation));
    ASSERT_EQ(2U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(16U, this->annotatableQuantumComputation.getNqubits());

    const auto firstInoutParameterExpectedQRegData  = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("a", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto secondInoutParameterExpectedQRegData = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("b", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 15U}), AnnotatableQuantumComputation::QubitType::Data);
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, firstInoutParameterExpectedQRegData));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, secondInoutParameterExpectedQRegData));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithLocalVariablesOfTypeWire) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module main() wire a(4) wire b[2][3](2) skip", this->annotatableQuantumComputation));
    ASSERT_EQ(2U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(16U, this->annotatableQuantumComputation.getNqubits());

    const auto firstLocalVariableOfTypeWireExpectedQRegData  = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(0), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Ancillary);
    const auto secondLocalVariableOfTypeWireExpectedQRegData = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(1), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 15U}), AnnotatableQuantumComputation::QubitType::Ancillary);
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, firstLocalVariableOfTypeWireExpectedQRegData));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, secondLocalVariableOfTypeWireExpectedQRegData));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithLocalVariablesOfTypeState) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module main() state a(4) state b[2][3](2) skip", this->annotatableQuantumComputation));
    ASSERT_EQ(2U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(16U, this->annotatableQuantumComputation.getNqubits());

    const auto firstLocalVariableOfTypeStateExpectedQRegData  = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(0), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto secondLocalVariableOfTypeStateExpectedQRegData = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(1), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 15U}), AnnotatableQuantumComputation::QubitType::Data);
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, firstLocalVariableOfTypeStateExpectedQRegData));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, secondLocalVariableOfTypeStateExpectedQRegData));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramRequiringAncillaryQubitsForIntermediateResults) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module main(inout a(4), in b(4), in c(4)) a += ((b * c) / c)", this->annotatableQuantumComputation));
    ASSERT_EQ(4U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(24U, this->annotatableQuantumComputation.getNqubits());

    const auto expectedQRegDataOfParameterA                     = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("a", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto expectedQRegDataOfParameterB                     = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("b", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfParameterC                     = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("c", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 11U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfQRegStoringIntermediateResults = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildAncillaryQubitLabel(3), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 23U}), AnnotatableQuantumComputation::QubitType::Ancillary);

    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterA));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterB));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterC));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfQRegStoringIntermediateResults));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleCallWithCalledModuleHavingLocalVariables) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module calledModule(inout a(4), in b(4)) wire c(4) a += b; a += c module main(inout mod(4), in operand(4)) call calledModule(mod, operand)", this->annotatableQuantumComputation));
    ASSERT_EQ(3U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(12U, this->annotatableQuantumComputation.getNqubits());

    const auto expectedQRegDataOfParameterMod                 = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("mod", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto expectedQRegDataOfParameterOperand             = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("operand", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfLocalVariableCOfCalledModule = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(2), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 11U}), AnnotatableQuantumComputation::QubitType::Ancillary);

    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterMod));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterOperand));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfLocalVariableCOfCalledModule));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleUncallWithUncalledModuleHavingLocalVariables) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module calledModule(inout a(4), in b(4)) wire c(4) a += b; a += c module main(inout mod(4), in operand(4)) uncall calledModule(mod, operand)", this->annotatableQuantumComputation));
    ASSERT_EQ(3U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(12U, this->annotatableQuantumComputation.getNqubits());

    const auto expectedQRegDataOfParameterMod                   = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("mod", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto expectedQRegDataOfParameterOperand               = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("operand", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfLocalVariableCOfUncalledModule = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildNonAncillaryQubitLabel(2), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 11U}), AnnotatableQuantumComputation::QubitType::Ancillary);

    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterMod));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterOperand));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfLocalVariableCOfUncalledModule));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleCallWithCalledModuleRequiringAncillaryQubitsForIntermediateResults) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module calledModule(inout a(4), in b(4), in c(4)) a += ((b * c) / c) module main(inout result(4), in operandL(4), in operandR(4)) call calledModule(result, operandL, operandR)", this->annotatableQuantumComputation));
    ASSERT_EQ(4U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(24U, this->annotatableQuantumComputation.getNqubits());

    const auto expectedQRegDataOfParameterResult               = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("result", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto expectedQRegDataOfParameterOperandL             = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("operandL", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfParameterOperandR             = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("operandR", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 11U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfQRegStoringIntermediateResult = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildAncillaryQubitLabel(3), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 23U}), AnnotatableQuantumComputation::QubitType::Ancillary);

    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterResult));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterOperandL));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterOperandR));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfQRegStoringIntermediateResult));
}

TYPED_TEST_P(GeneratedQuantumComputationBySynthesisTestFixture, CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleUncallWithUncalledModuleRequiringAncillaryQubitsForIntermediateResults) {
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::parseInputCircuitPerformSynthesisAndAssertSuccess("module calledModule(inout a(4), in b(4), in c(4)) a += ((b * c) / c) module main(inout result(4), in operandL(4), in operandR(4)) uncall calledModule(result, operandL, operandR)", this->annotatableQuantumComputation));
    ASSERT_EQ(4U, this->annotatableQuantumComputation.getQuantumRegisters().size());
    ASSERT_EQ(24U, this->annotatableQuantumComputation.getNqubits());

    const auto expectedQRegDataOfParameterResult               = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("result", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 0U, .lastQubitIndex = 3U}), AnnotatableQuantumComputation::QubitType::Data);
    const auto expectedQRegDataOfParameterOperandL             = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("operandL", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 4U, .lastQubitIndex = 7U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfParameterOperandR             = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg("operandR", AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 8U, .lastQubitIndex = 11U}), AnnotatableQuantumComputation::QubitType::Garbage);
    const auto expectedQRegDataOfQRegStoringIntermediateResult = typename GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::ExpectedDataOfGeneratedQReg(InternalQubitLabelBuilder::buildAncillaryQubitLabel(3), AnnotatableQuantumComputation::QubitIndexRange({.firstQubitIndex = 12U, .lastQubitIndex = 23U}), AnnotatableQuantumComputation::QubitType::Ancillary);

    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterResult));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterOperandL));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfParameterOperandR));
    ASSERT_NO_FATAL_FAILURE(GeneratedQuantumComputationBySynthesisTestFixture<TypeParam>::assertGeneratedQuantumRegisterDataMatchesExpectedOne(this->annotatableQuantumComputation, expectedQRegDataOfQRegStoringIntermediateResult));
}

REGISTER_TYPED_TEST_SUITE_P(GeneratedQuantumComputationBySynthesisTestFixture,
                            CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithInParameters,
                            CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithOutParameters,
                            CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithInoutParameters,
                            CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithLocalVariablesOfTypeWire,
                            CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramWithLocalVariablesOfTypeState,
                            CheckGeneratedQuantumRegistersForSingleModuleSyrecProgramRequiringAncillaryQubitsForIntermediateResults,
                            CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleCallWithCalledModuleHavingLocalVariables,
                            CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleUncallWithUncalledModuleHavingLocalVariables,
                            CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleCallWithCalledModuleRequiringAncillaryQubitsForIntermediateResults,
                            CheckGeneratedQuantumRegistersForSyrecProgramContainingModuleUncallWithUncalledModuleRequiringAncillaryQubitsForIntermediateResults);

using SynthesizerTypes = testing::Types<CostAwareSynthesis, LineAwareSynthesis>;
INSTANTIATE_TYPED_TEST_SUITE_P(GeneratedQuantumComputationBySynthesisTests, GeneratedQuantumComputationBySynthesisTestFixture, SynthesizerTypes, );
