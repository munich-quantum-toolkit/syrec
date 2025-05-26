/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/annotatable_quantum_computation.hpp"
#include "ir/Definitions.hpp"
#include "ir/operations/Control.hpp"
#include "ir/operations/OpType.hpp"
#include "ir/operations/Operation.hpp"
#include "ir/operations/StandardOperation.hpp"

#include <cstddef>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The current tests do not cover the functionality:
// * set- and get constant/garbage/input/output lines
// * adding and getting lines of the circuit
// * the stringification of the supported gate types
// ** (Gate::toQasm() will generate outputs that are not supported by the QASM standard without extra definitions and only supported by MQT::Core)
// * the stringification of the whole circuit to either a string or file

using namespace syrec;

const static std::string DEFAULT_QUBIT_LABEL_PREFIX = "qubit";

class AnnotatedQuantumComputationTestsFixture: public testing::Test {
protected:
    std::unique_ptr<AnnotatableQuantumComputation> annotatedQuantumComputation;

    void SetUp() override {
        annotatedQuantumComputation = std::make_unique<AnnotatableQuantumComputation>();
    }

    static void assertThatOperationsOfQuantumComputationAreEqualToSequence(const AnnotatableQuantumComputation& annotatedQuantumComputation, const std::vector<std::unique_ptr<qc::Operation>>& expectedQuantumOperations) {
        const std::size_t expectedNumOperations      = expectedQuantumOperations.size();
        const std::size_t actualNumQuantumOperations = annotatedQuantumComputation.getNindividualOps();
        ASSERT_EQ(expectedNumOperations, actualNumQuantumOperations) << "Expected that annotated quantum computation contains " << std::to_string(expectedNumOperations) << " quantum operations but actually contained " << std::to_string(actualNumQuantumOperations) << " quantum operations";

        auto expectedQuantumOperationsIterator = expectedQuantumOperations.begin();
        for (std::size_t i = 0; i < expectedNumOperations; ++i) {
            auto const* actualQuantumOperation = annotatedQuantumComputation.getQuantumOperation(i);
            ASSERT_THAT(actualQuantumOperation, testing::NotNull());
            const auto& expectedQuantumOperation = *expectedQuantumOperationsIterator;
            ASSERT_THAT(expectedQuantumOperation, testing::NotNull());
            ASSERT_TRUE(expectedQuantumOperation->equals(*actualQuantumOperation));
            ++expectedQuantumOperationsIterator; // NOLINT (cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

    static void assertThatAnnotationsOfQuantumOperationAreEqualTo(const AnnotatableQuantumComputation& annotatedQuantumComputation, std::size_t indexOfQuantumOperationInQuantumComputation, const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup& expectedAnnotationsOfQuantumComputation) {
        ASSERT_TRUE(indexOfQuantumOperationInQuantumComputation < annotatedQuantumComputation.getNindividualOps());
        const auto& actualAnnotationsOfQuantumOperation = annotatedQuantumComputation.getAnnotationsOfQuantumOperation(indexOfQuantumOperationInQuantumComputation);
        for (const auto& [expectedAnnotationKey, expectedAnnotationValue]: expectedAnnotationsOfQuantumComputation) {
            ASSERT_TRUE(actualAnnotationsOfQuantumOperation.count(expectedAnnotationKey) != 0) << "Expected annotation with key '" << expectedAnnotationKey << "' was not found";
            const auto& actualAnnotationValue = actualAnnotationsOfQuantumOperation.at(expectedAnnotationKey);
            ASSERT_EQ(expectedAnnotationValue, actualAnnotationValue) << "Value for annotation with key '" << expectedAnnotationKey << "' did not match, expected: " << expectedAnnotationValue << " but was actually " << actualAnnotationValue;
        }
    }

    static void assertAdditionOfNonAncillaryQubitForIndexSucceeds(AnnotatableQuantumComputation& annotatedQuantumComputation, const qc::Qubit expectedQubitIndex) {
        const std::optional<qc::Qubit> actualQubitIndex = annotatedQuantumComputation.addNonAncillaryQubit(DEFAULT_QUBIT_LABEL_PREFIX + std::to_string(expectedQubitIndex), false);
        ASSERT_TRUE(actualQubitIndex.has_value());
        ASSERT_EQ(expectedQubitIndex, *actualQubitIndex);
    }
};

// BEGIN Adding qubit types
TEST_F(AnnotatedQuantumComputationTestsFixture, AddNonAncillaryQubitThatIsNotGarbage) {
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddNonAncillaryQubitThatIsGarbage) {
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", true);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({true}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddNonAncillaryQubitWithEmptyLabelNotPossible) {
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("", false);
    ASSERT_FALSE(qubitIndex.has_value());

    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
    ASSERT_EQ(0, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_TRUE(annotatedQuantumComputation->getGarbage().empty());
    ASSERT_TRUE(annotatedQuantumComputation->getAncillary().empty());
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddNonAncillaryQubitWithDuplicateLabelNotPossible) {
    const std::string              qubitLabel = "nonAncillary";
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit(qubitLabel, false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfQubitWithDuplicateLabel = annotatedQuantumComputation->addNonAncillaryQubit(qubitLabel, true);
    ASSERT_FALSE(indexOfQubitWithDuplicateLabel.has_value());

    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddNonAncillaryQubitWithLabelMatchingAncillaryQubitLabel) {
    const std::string              qubitLabel = "ancillary";
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit(qubitLabel, false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*qubitIndex}));
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfQubitWithDuplicateLabel = annotatedQuantumComputation->addNonAncillaryQubit(qubitLabel, true);
    ASSERT_FALSE(indexOfQubitWithDuplicateLabel.has_value());

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*qubitIndex}));
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddNonAncillaryQubitAfterAnyQubitWasSetAncillaryNotPossible) {
    const std::optional<qc::Qubit> ancillaryQubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(0, *ancillaryQubitIndex);

    const std::optional<qc::Qubit> nonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", false);
    ASSERT_TRUE(nonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *nonAncillaryQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*ancillaryQubitIndex));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfQubitAfterAnyQubitWasSetAncillary = annotatedQuantumComputation->addNonAncillaryQubit("otherLabel", false);
    ASSERT_FALSE(indexOfQubitAfterAnyQubitWasSetAncillary.has_value());

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddAncillaryQubitWithInitialStateZero) {
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*qubitIndex}));
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddAncillaryQubitWithInitialStateOne) {
    const std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", true);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*qubitIndex}));
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false}));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputationOperations;
    expectedQuantumComputationOperations.emplace_back(std::make_unique<qc::StandardOperation>(*qubitIndex, qc::OpType::X));
    ASSERT_NO_FATAL_FAILURE(assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputationOperations));
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddAncillaryQubitWithEmptyLabelNotPossible) {
    const std::optional<qc::Qubit> ancillaryQubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(0, *ancillaryQubitIndex);

    const std::optional<qc::Qubit> nonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", false);
    ASSERT_TRUE(nonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *nonAncillaryQubitIndex);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfAncillaryQubitWithEmptyLabel = annotatedQuantumComputation->addPreliminaryAncillaryQubit("", false);
    ASSERT_FALSE(indexOfAncillaryQubitWithEmptyLabel.has_value());

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddAncillaryQubitWithDuplicateLabelNotPossible) {
    const std::string              ancillaryQubitLabel = "ancillary";
    const std::optional<qc::Qubit> ancillaryQubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit(ancillaryQubitLabel, false);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(0, *ancillaryQubitIndex);

    const std::optional<qc::Qubit> nonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", false);
    ASSERT_TRUE(nonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *nonAncillaryQubitIndex);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfAncillaryQubitWithDuplicateLabel = annotatedQuantumComputation->addPreliminaryAncillaryQubit(ancillaryQubitLabel, false);
    ASSERT_FALSE(indexOfAncillaryQubitWithDuplicateLabel.has_value());

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddAncillaryQubitWithLabelMatchingNonAncillaryQubitLabel) {
    const std::string              ancillaryQubitLabel    = "ancillary";
    const std::string              nonAncillaryQubitLabel = "nonAncillary";
    const std::optional<qc::Qubit> ancillaryQubitIndex    = annotatedQuantumComputation->addPreliminaryAncillaryQubit(ancillaryQubitLabel, false);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(0, *ancillaryQubitIndex);

    const std::optional<qc::Qubit> nonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit(nonAncillaryQubitLabel, false);
    ASSERT_TRUE(nonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *nonAncillaryQubitIndex);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfAncillaryQubitWithDuplicateLabel = annotatedQuantumComputation->addPreliminaryAncillaryQubit(nonAncillaryQubitLabel, true);
    ASSERT_FALSE(indexOfAncillaryQubitWithDuplicateLabel.has_value());

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddAncillaryQubitAfterAnyQubitWasSetAncillaryNotPossible) {
    const std::optional<qc::Qubit> ancillaryQubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubitIndex.has_value());
    ASSERT_EQ(0, *ancillaryQubitIndex);

    const std::optional<qc::Qubit> nonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", false);
    ASSERT_TRUE(nonAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *nonAncillaryQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*ancillaryQubitIndex));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());

    const std::optional<qc::Qubit> indexOfQubitAfterAnyQubitWasSetAncillary = annotatedQuantumComputation->addPreliminaryAncillaryQubit("otherLabel", false);
    ASSERT_FALSE(indexOfQubitAfterAnyQubitWasSetAncillary.has_value());

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubitIndex}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    ASSERT_THAT(annotatedQuantumComputation->getGarbage(), testing::ElementsAreArray({false, false}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false}));
    ASSERT_EQ(0, annotatedQuantumComputation->getNindividualOps());
}
// END Adding qubit types

// BEGIN getAddedAncillaryQubitIndices tests
TEST_F(AnnotatedQuantumComputationTestsFixture, GetAddedAncillaryQubitIndicesInEmptyQuantumComputation) {
    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, GetAddedAncillaryQubitIndicesWithoutAncillaryQubits) {
    qc::Qubit expectedAddedQubitIndex = 0;

    std::optional<qc::Qubit> actualAddedQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_1", false);
    ASSERT_TRUE(actualAddedQubitIndex.has_value());
    ASSERT_EQ(expectedAddedQubitIndex, *actualAddedQubitIndex);
    ++expectedAddedQubitIndex;

    actualAddedQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_2", false);
    ASSERT_TRUE(actualAddedQubitIndex.has_value());
    ASSERT_EQ(expectedAddedQubitIndex, *actualAddedQubitIndex);
    ++expectedAddedQubitIndex;

    actualAddedQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_3", false);
    ASSERT_TRUE(actualAddedQubitIndex.has_value());
    ASSERT_EQ(expectedAddedQubitIndex, *actualAddedQubitIndex);
    ++expectedAddedQubitIndex;

    ASSERT_TRUE(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices().empty());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, GetAddedAncillaryQubitIndices) {
    const std::optional<qc::Qubit> firstNonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_1", false);
    ASSERT_TRUE(firstNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(0, *firstNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> firstAncillaryQubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("Ancillary_1", false);
    ASSERT_TRUE(firstAncillaryQubitIndex.has_value());
    ASSERT_EQ(1, *firstAncillaryQubitIndex);

    const std::optional<qc::Qubit> secondNonAncillaryQubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_2", false);
    ASSERT_TRUE(secondNonAncillaryQubitIndex.has_value());
    ASSERT_EQ(2, *secondNonAncillaryQubitIndex);

    const std::optional<qc::Qubit> secondAncillaryQubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("Ancillary_2", true);
    ASSERT_TRUE(secondAncillaryQubitIndex.has_value());
    ASSERT_EQ(3, *secondAncillaryQubitIndex);

    const auto& ancillaryQubitIndices = annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices();
    ASSERT_THAT(ancillaryQubitIndices, testing::UnorderedElementsAreArray({*firstAncillaryQubitIndex, *secondAncillaryQubitIndex}));
}
// BEGIN getAddedAncillaryQubitIndices tests

// BEGIN setQubitAsAncillary tests
TEST_F(AnnotatedQuantumComputationTestsFixture, SetAncillaryQubitAsAncillary) {
    const std::optional<qc::Qubit> nonAncillaryQubit = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", true);
    ASSERT_TRUE(nonAncillaryQubit.has_value() && *nonAncillaryQubit == 0);
    const std::optional<qc::Qubit> ancillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubit.has_value() && *ancillaryQubit == 1);
    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*ancillaryQubit));

    const auto& ancillaryQubitIndices = annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices();
    ASSERT_THAT(ancillaryQubitIndices, testing::UnorderedElementsAreArray({*ancillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, true}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetNonAncillaryQubitAsAncillary) {
    const std::optional<qc::Qubit> nonAncillaryQubit = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", true);
    ASSERT_TRUE(nonAncillaryQubit.has_value() && *nonAncillaryQubit == 0);
    const std::optional<qc::Qubit> ancillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubit.has_value() && *ancillaryQubit == 1);
    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*nonAncillaryQubit));

    const auto& ancillaryQubitIndices = annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices();
    ASSERT_THAT(ancillaryQubitIndices, testing::UnorderedElementsAreArray({*ancillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetUnknownQubitAsAncillary) {
    const std::optional<qc::Qubit> nonAncillaryQubit = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", true);
    ASSERT_TRUE(nonAncillaryQubit.has_value() && *nonAncillaryQubit == 0);
    const std::optional<qc::Qubit> ancillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubit.has_value() && *ancillaryQubit == 1);

    constexpr qc::Qubit unknownQubitIndex = 2;
    ASSERT_FALSE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(unknownQubitIndex));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false}));
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetQubitAlreadySetAsAncillary) {
    const std::optional<qc::Qubit> ancillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillary", false);
    ASSERT_TRUE(ancillaryQubit.has_value() && *ancillaryQubit == 0);

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*ancillaryQubit));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true}));
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*ancillaryQubit));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*ancillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true}));
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetMultipleQubitsAsAncillary) {
    const std::optional<qc::Qubit> nonAncillaryQubit = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", true);
    ASSERT_TRUE(nonAncillaryQubit.has_value() && *nonAncillaryQubit == 0);

    const std::optional<qc::Qubit> firstAncillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillaryOne", false);
    ASSERT_TRUE(firstAncillaryQubit.has_value() && *firstAncillaryQubit == 1);

    const std::optional<qc::Qubit> secondAncillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillaryTwo", false);
    ASSERT_TRUE(secondAncillaryQubit.has_value() && *secondAncillaryQubit == 2);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*firstAncillaryQubit, *secondAncillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false, false}));
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*firstAncillaryQubit));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*firstAncillaryQubit, *secondAncillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, true, false}));
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*secondAncillaryQubit));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*firstAncillaryQubit, *secondAncillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, true, true}));
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(2, annotatedQuantumComputation->getNancillae());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddingFurtherQubitsAfterSetQubitToAncillaryDidNotSucceedPossible) {
    const std::optional<qc::Qubit> firstAncillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillaryOne", false);
    ASSERT_TRUE(firstAncillaryQubit.has_value() && *firstAncillaryQubit == 0);

    ASSERT_FALSE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(static_cast<qc::Qubit>(100)));

    const std::optional<qc::Qubit> nonAncillaryQubit = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary", true);
    ASSERT_TRUE(nonAncillaryQubit.has_value() && *nonAncillaryQubit == 1);

    const std::optional<qc::Qubit> secondAncillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("ancillaryTwo", false);
    ASSERT_TRUE(secondAncillaryQubit.has_value() && *secondAncillaryQubit == 2);

    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*firstAncillaryQubit, *secondAncillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({false, false, false}));
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(0, annotatedQuantumComputation->getNancillae());

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*firstAncillaryQubit));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*firstAncillaryQubit, *secondAncillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false, false}));
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNancillae());

    const std::optional<qc::Qubit> indexOfNotAddableAncillaryQubit = annotatedQuantumComputation->addPreliminaryAncillaryQubit("otherQubitLabel", false);
    ASSERT_FALSE(indexOfNotAddableAncillaryQubit.has_value());

    const std::optional<qc::Qubit> indexOfNotAddableNonAncillaryQubit = annotatedQuantumComputation->addNonAncillaryQubit("otherQubitLabel", false);
    ASSERT_FALSE(indexOfNotAddableNonAncillaryQubit.has_value());

    ASSERT_TRUE(annotatedQuantumComputation->promotePreliminaryAncillaryQubitToDefinitiveAncillary(*secondAncillaryQubit));
    ASSERT_THAT(annotatedQuantumComputation->getAddedPreliminaryAncillaryQubitIndices(), testing::UnorderedElementsAreArray({*firstAncillaryQubit, *secondAncillaryQubit}));
    ASSERT_THAT(annotatedQuantumComputation->getAncillary(), testing::ElementsAreArray({true, false, true}));
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());
    ASSERT_EQ(1, annotatedQuantumComputation->getNgarbageQubits());
    ASSERT_EQ(2, annotatedQuantumComputation->getNancillae());
}
// BEGIN setQubitAsAncillary tests

// BEGIN getNqubits tests
TEST_F(AnnotatedQuantumComputationTestsFixture, GetNqubitsInEmptyQuantumComputation) {
    ASSERT_EQ(0, annotatedQuantumComputation->getNqubits());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, GetNqubits) {
    std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_1", false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);
    ASSERT_EQ(1, annotatedQuantumComputation->getNqubits());

    qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("Ancillary_1", false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(1, *qubitIndex);
    ASSERT_EQ(2, annotatedQuantumComputation->getNqubits());

    qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit("nonAncillary_2", false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(2, *qubitIndex);
    ASSERT_EQ(3, annotatedQuantumComputation->getNqubits());

    qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit("Ancillary_2", true);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(3, *qubitIndex);
    ASSERT_EQ(4, annotatedQuantumComputation->getNqubits());
}
// END getNqubits tests

// BEGIN getQubitLabels tests
TEST_F(AnnotatedQuantumComputationTestsFixture, GetQubitLabelsInEmptyQuantumComputation) {
    ASSERT_TRUE(annotatedQuantumComputation->getQubitLabels().empty());
}

TEST_F(AnnotatedQuantumComputationTestsFixture, GetQubitLabels) {
    const std::vector<std::string> expectedQubitLabels = {"nonAncillary_1", "Ancillary_1", "nonAcillary_2", "Ancillary_2"};

    std::optional<qc::Qubit> qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit(expectedQubitLabels[0], false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(0, *qubitIndex);

    qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit(expectedQubitLabels[1], false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(1, *qubitIndex);

    qubitIndex = annotatedQuantumComputation->addNonAncillaryQubit(expectedQubitLabels[2], false);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(2, *qubitIndex);

    qubitIndex = annotatedQuantumComputation->addPreliminaryAncillaryQubit(expectedQubitLabels[3], true);
    ASSERT_TRUE(qubitIndex.has_value());
    ASSERT_EQ(3, *qubitIndex);

    const auto& actualQubitLabels = annotatedQuantumComputation->getQubitLabels();
    ASSERT_THAT(actualQubitLabels, testing::ElementsAreArray(expectedQubitLabels));
}
// BEGIN getQubitLabels tests

// BEGIN AddXGate tests
TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGate) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 2;
    constexpr qc::Qubit expectedTargetQubitIndex     = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithUnknownControlQubit) {
    constexpr qc::Qubit expectedUnknownControlQubitIndex = 2;
    constexpr qc::Qubit expectedKnownControlQubitIndex   = 1;
    constexpr qc::Qubit expectedTargetQubitIndex         = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedKnownControlQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedUnknownControlQubitIndex, expectedKnownControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedKnownControlQubitIndex, expectedUnknownControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithDuplicateControlQubitPossible) {
    constexpr qc::Qubit expectedControlQubitIndex = 1;
    constexpr qc::Qubit expectedTargetQubitIndex  = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndex);
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndex, expectedControlQubitIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithTargetLineBeingEqualToEitherControlQubitNotPossible) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexTwo));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithUnknownTargetLine) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    constexpr qc::Qubit unknownQubitIndex            = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, unknownQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = 3;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = 4;
    constexpr qc::Qubit expectedGateTargetQubitIndex     = 5;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateControlQubitOneIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateControlQubitTwoIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithTargetLineMatchingActiveControlQubitInAnyParentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = 2;
    constexpr qc::Qubit expectedGateControlQubitTwo      = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateControlQubitOneIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateControlQubitTwo);

    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexTwo;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwo, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithControlQubitsBeingDisabledInCurrentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedGateControlQubitIndex = 2;
    constexpr qc::Qubit expectedTargetQubitIndex      = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateControlQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));
    auto expectedOperationForToffoliGateWithBothControlQubitsDeregistered = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForToffoliGateWithBothControlQubitsDeregistered));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedGateControlQubitIndex, expectedTargetQubitIndex));
    auto expectedOperationForToffoliGateWithFirstControlQubitsDeregistered = std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex, expectedControlQubitIndexOne}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForToffoliGateWithFirstControlQubitsDeregistered));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedGateControlQubitIndex, expectedTargetQubitIndex));
    auto expectedOperationForToffoliGateWithSecondControlQubitsDeregistered = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedGateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForToffoliGateWithSecondControlQubitsDeregistered));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithScopeActivatingDeactivatedControlQubitOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedTargetQubitIndex = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithDeactivationOfControlQubitPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    constexpr qc::Qubit expectedTargetQubitIndex = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithTargetLineMatchingDeactivatedControlQubitOfPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = expectedControlQubitIndexThree;
    constexpr qc::Qubit expectedTargetQubitIndex         = expectedControlQubitIndexOne;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingToffoliGateWithCallerProvidedControlQubitsMatchingDeregisteredControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    constexpr qc::Qubit expectedTargetQubitIndex       = 4;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexFour);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    constexpr qc::Qubit propagatedControlQubit = expectedControlQubitIndexThree;
    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexFour));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubit));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(propagatedControlQubit));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitOneIndex = expectedControlQubitIndexOne;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = expectedControlQubitIndexTwo;

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedTargetQubitIndex));
    auto expectedOperationForFirstToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForFirstToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubit));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingToffoliGate(expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedTargetQubitIndex));
    auto expectedOperationForSecondToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({propagatedControlQubit, expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForSecondToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGate) {
    constexpr qc::Qubit expectedControlQubitIndex = 0;
    constexpr qc::Qubit expectedTargetQubitIndex  = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithUnknownControlQubit) {
    constexpr qc::Qubit expectedControlQubitIndex = 1;
    constexpr qc::Qubit expectedTargetQubitIndex  = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithUnknownTargetLine) {
    constexpr qc::Qubit expectedControlQubitIndex = 0;
    constexpr qc::Qubit expectedTargetQubitIndex  = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithControlAndTargetLineBeingSameLine) {
    constexpr qc::Qubit expectedControlQubitIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndex, expectedControlQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    constexpr qc::Qubit expectedTargetQubitIndex       = 4;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexFour);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitIndexFour, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithTargetLineMatchingActiveControlQubitInAnyParentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = expectedControlQubitIndexOne;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithControlQubitBeingDeactivatedInCurrentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithDeactivationOfControlQubitPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex, expectedControlQubitIndexOne}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithTargetLineMatchingDeactivatedControlQubitOfPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexTwo;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = expectedControlQubitIndexOne;

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingCnotGateWithCallerProvidedControlQubitsMatchingDeregisteredControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexOne;
    constexpr qc::Qubit expectedGateTargetQubitIndex  = expectedControlQubitIndexThree;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    auto                                        expectedOperationForFirstCnotGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForFirstCnotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    constexpr qc::Qubit propagatedControlQubitIndex = expectedControlQubitIndexTwo;
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedGateTargetQubitIndex));

    auto expectedOperationForSecondCnotGate = std::make_unique<qc::StandardOperation>(qc::Controls({propagatedControlQubitIndex, expectedGateControlQubitIndex}), expectedGateTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(expectedOperationForSecondCnotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingNotGate) {
    constexpr qc::Qubit expectedTargetQubitIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(0));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithUnknownTargetLine) {
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingNotGate(0));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexFour);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexFour));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    constexpr qc::Qubit expectedTargetQubitIndex = 4;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexFour}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithTargetLineMatchingActiveControlQubitInAnyParentControlQubitScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();

    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexOne;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingNotGateWithTargetLineMatchingDeactivatedControlQubitOfControlQubitPropagationScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexOne;

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGate) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 2;
    constexpr qc::Qubit expectedControlQubitIndexThree = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    const qc::Controls gateControlQubitsIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(gateControlQubitsIndices, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(gateControlQubitsIndices, expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithUnknownControlQubit) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit unknownControlQubit            = 3;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    const qc::Controls expectedGateControlQubitIndices({expectedControlQubitIndexOne, unknownControlQubit, expectedControlQubitIndexThree});
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithUnknownTargetLine) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedTargetQubitIndex       = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    const qc::Controls expectedGateControlQubitIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithoutControlQubitsAndNoActiveLocalControlQubitScopes) {
    constexpr qc::Qubit expectedTargetQubitIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({}, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithActiveControlQubitsInParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexThree));

    constexpr qc::Qubit expectedGateControlQubitIndex = 3;
    constexpr qc::Qubit expectedTargetQubitIndex      = 4;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedGateControlQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({expectedGateControlQubitIndex}, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedGateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithTargetLineMatchingActiveControlQubitsOfAnyParentControlQubitScopes) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    const qc::Controls  expectedGateControlQubitIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo});
    constexpr qc::Qubit targetQubit = expectedControlQubitIndexOne;
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, targetQubit));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithTargetLineBeingEqualToUserProvidedControlQubit) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit targetQubit                    = expectedControlQubitIndexTwo;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    const qc::Controls expectedGateControlQubitIndices({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, targetQubit));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithTargetLineMatchingDeactivatedControlQubitOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    // The multi control toffoli gate should be created due to the target line only overlapping a deactivated control line in the current control line propagation scope
    constexpr qc::Qubit expectedTargetQubitIndex = expectedControlQubitIndexOne;
    const qc::Controls  expectedGateControlQubitIndices({expectedControlQubitIndexTwo, expectedControlQubitIndexThree});
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(expectedGateControlQubitIndices, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(expectedGateControlQubitIndices, expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingMultiControlToffoliGateWithCallerProvidedControlQubitsMatchingDeregisteredControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedControlQubitIndexFour  = 3;
    constexpr qc::Qubit expectedTargetQubitIndex       = 4;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexFour);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    constexpr qc::Qubit propagatedControlQubitIndex = expectedControlQubitIndexThree;
    constexpr qc::Qubit notPropagatedControlQubit   = expectedControlQubitIndexFour;

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexFour));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(propagatedControlQubitIndex));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(notPropagatedControlQubit));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(notPropagatedControlQubit));

    constexpr qc::Qubit expectedGateControlQubitOneIndex = propagatedControlQubitIndex;
    constexpr qc::Qubit expectedGateControlQubitTwoIndex = expectedControlQubitIndexTwo;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    auto                                        operationForFirstMultiControlToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo, expectedGateControlQubitOneIndex, expectedGateControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(operationForFirstMultiControlToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(propagatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate({expectedGateControlQubitTwoIndex}, expectedTargetQubitIndex));

    auto operationForSecondMultiControlToffoliGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo, propagatedControlQubitIndex, expectedGateControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumOperations.emplace_back(std::move(operationForSecondMultiControlToffoliGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingFredkinGate) {
    constexpr qc::Qubit expectedTargetQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndexTwo);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(expectedTargetQubitIndexOne, expectedTargetQubitIndexTwo));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumOperations;
    expectedQuantumOperations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), qc::Targets({expectedTargetQubitIndexOne, expectedTargetQubitIndexTwo}), qc::OpType::SWAP));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumOperations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithUnknownTargetLine) {
    constexpr qc::Qubit knownTargetQubitIndex   = 0;
    constexpr qc::Qubit unknownTargetQubitIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, knownTargetQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(knownTargetQubitIndex, unknownTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(unknownTargetQubitIndex, knownTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithTargetLinesTargetingSameLine) {
    constexpr qc::Qubit expectedTargetQubitIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(expectedTargetQubitIndex, expectedTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithTargetLineMatchingActiveControlQubitOfAnyParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    constexpr qc::Qubit notOverlappingTargetQubitIndex = 2;
    constexpr qc::Qubit overlappingTargetQubitIndex    = expectedControlQubitIndexTwo;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, notOverlappingTargetQubitIndex);

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(notOverlappingTargetQubitIndex, overlappingTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(overlappingTargetQubitIndex, notOverlappingTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});

    ASSERT_FALSE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(overlappingTargetQubitIndex, overlappingTargetQubitIndex));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, AddOperationsImplementingFredkinGateWithTargetLineMatchingDeactivatedControlQubitOfParentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    // The fredkin gate should be created due to the target line only overlapping a deactivated control line in the current control line propagation scope
    constexpr qc::Qubit notOverlappingTargetQubitIndex = 2;
    constexpr qc::Qubit overlappingTargetQubitIndex    = expectedControlQubitIndexOne;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, notOverlappingTargetQubitIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(notOverlappingTargetQubitIndex, overlappingTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    auto                                        operationImplementingFirstFredkinGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo}), qc::Targets({notOverlappingTargetQubitIndex, overlappingTargetQubitIndex}), qc::OpType::SWAP);
    expectedQuantumComputations.emplace_back(std::move(operationImplementingFirstFredkinGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingFredkinGate(overlappingTargetQubitIndex, notOverlappingTargetQubitIndex));

    auto operationImplementingSecondFredkinGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexTwo}), qc::Targets({overlappingTargetQubitIndex, notOverlappingTargetQubitIndex}), qc::OpType::SWAP);
    expectedQuantumComputations.emplace_back(std::move(operationImplementingSecondFredkinGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}
// END AddXGate tests

// BEGIN Control line propagation scopes tests
TEST_F(AnnotatedQuantumComputationTestsFixture, RegisterDuplicateControlQubitOfParentScopeInLocalControlQubitScope) {
    constexpr qc::Qubit parentScopeControlQubitIndex = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, parentScopeControlQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(parentScopeControlQubitIndex));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(parentScopeControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls(), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({parentScopeControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, RegisterDuplicateControlQubitDeactivatedOfParentScopeInLocalScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls(), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, RegisterControlQubitNotKnownInCircuit) {
    constexpr qc::Qubit expectedTargetQubitIndex         = 0;
    constexpr qc::Qubit expectedKnownControlQubitIndex   = 1;
    constexpr qc::Qubit expectedUnknownControlQubitIndex = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedKnownControlQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_FALSE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedUnknownControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, RegisterControlQubitWithNoActivateControlQubitScopeWillCreateNewScope) {
    constexpr qc::Qubit expectedControlQubitOneIndex = 0;
    constexpr qc::Qubit expectedControlQubitTwoIndex = 1;
    constexpr qc::Qubit expectedTargetQubitIndex     = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitOneIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitTwoIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    auto expectedOperationForFirstAddedNotGate = std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumComputations.emplace_back(std::move(expectedOperationForFirstAddedNotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitOneIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    auto expectedOperationForSecondAddedNotGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitOneIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumComputations.emplace_back(std::move(expectedOperationForSecondAddedNotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitOneIndex));
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedControlQubitTwoIndex, expectedTargetQubitIndex));

    auto expectedOperationForThirdAddedCnotGate = std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitTwoIndex}), expectedTargetQubitIndex, qc::OpType::X);
    expectedQuantumComputations.emplace_back(std::move(expectedOperationForThirdAddedCnotGate));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeregisterControlQubitOfLocalControlQubitScope) {
    constexpr qc::Qubit expectedTargetQubitIndex     = 0;
    constexpr qc::Qubit activateControlQubitIndex    = 1;
    constexpr qc::Qubit deactivatedControlQubitIndex = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, activateControlQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, deactivatedControlQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(deactivatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(activateControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(deactivatedControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({activateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeregisterControlQubitOfParentScopeInLastActivateControlQubitScope) {
    constexpr qc::Qubit expectedTargetQubitIndex     = 0;
    constexpr qc::Qubit activateControlQubitIndex    = 1;
    constexpr qc::Qubit deactivatedControlQubitIndex = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, activateControlQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, deactivatedControlQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(deactivatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(activateControlQubitIndex));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(deactivatedControlQubitIndex));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(deactivatedControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({activateControlQubitIndex}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({activateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeregisterControlQubitNotKnownInCircuit) {
    constexpr qc::Qubit expectedTargetQubitIndex         = 0;
    constexpr qc::Qubit expectedKnownControlQubitIndex   = 1;
    constexpr qc::Qubit expectedUnknownControlQubitIndex = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedKnownControlQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_FALSE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedUnknownControlQubitIndex));
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedUnknownControlQubitIndex));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedKnownControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeregisterControlQubitOfParentPropagationScopeNotRegisteredInCurrentScope) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo = 1;
    constexpr qc::Qubit expectedTargetQubitIndex     = 2;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    // Deregistering a not registered control line should not modify the aggregate of all activate control lines
    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexTwo));

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingMultiControlToffoliGate(qc::Controls({expectedControlQubitIndexOne}), expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, RegisteringLocalControlQubitDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    annotatedQuantumComputation->activateControlQubitPropagationScope();

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeactivatingLocalControlQubitDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, ActivatingControlQubitPropagationScopeDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne = 0;
    constexpr qc::Qubit expectedTargetQubitIndex     = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeactivatingControlQubitPropagationScopeDoesNotAddNewControlQubitsToExistingGates) {
    constexpr qc::Qubit expectedControlQubitIndexOne   = 0;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 1;
    constexpr qc::Qubit expectedControlQubitIndexThree = 2;
    constexpr qc::Qubit expectedTargetQubitIndex       = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    constexpr qc::Qubit expectedGateControlQubitIndex = expectedControlQubitIndexThree;
    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingCnotGate(expectedGateControlQubitIndex, expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo, expectedGateControlQubitIndex}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    annotatedQuantumComputation->deactivateControlQubitPropagationScope();
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeactivateControlQubitPropagationScopeRegisteringControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 2;
    constexpr qc::Qubit expectedControlQubitIndexThree = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_TRUE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeactivateControlQubitPropagationScopeNotRegisteringControlQubitsOfParentScope) {
    constexpr qc::Qubit expectedTargetQubitIndex       = 0;
    constexpr qc::Qubit expectedControlQubitIndexOne   = 1;
    constexpr qc::Qubit expectedControlQubitIndexTwo   = 2;
    constexpr qc::Qubit expectedControlQubitIndexThree = 3;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedTargetQubitIndex);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexOne);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexTwo);
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, expectedControlQubitIndexThree);

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexOne));
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexTwo));

    annotatedQuantumComputation->activateControlQubitPropagationScope();
    ASSERT_TRUE(annotatedQuantumComputation->registerControlQubitForPropagationInCurrentAndNestedScopes(expectedControlQubitIndexThree));
    ASSERT_FALSE(annotatedQuantumComputation->deregisterControlQubitFromPropagationInCurrentScope(expectedControlQubitIndexOne));
    annotatedQuantumComputation->deactivateControlQubitPropagationScope();

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(expectedTargetQubitIndex));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls({expectedControlQubitIndexOne, expectedControlQubitIndexTwo}), expectedTargetQubitIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, DeactivatingControlQubitPropagationScopeWithNoActivatePropagationScopesIsEqualToNoOp) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    annotatedQuantumComputation->deactivateControlQubitPropagationScope();
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}
// BEGIN Control line propagation scopes tests

// BEGIN Annotation tests
TEST_F(AnnotatedQuantumComputationTestsFixture, SetAnnotationsForQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string annotationKey   = "KEY";
    const std::string annotationValue = "InitialValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, annotationKey, annotationValue));

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{annotationKey, annotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, UpdateAnnotationsForQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string firstAnnotationKey          = "KEY_ONE";
    const std::string initialFirstAnnotationValue = "InitialValue";

    const std::string secondAnnotationKey          = "KEY_TWO";
    const std::string initialSecondAnnotationValue = "OtherValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, initialFirstAnnotationValue));
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, secondAnnotationKey, initialSecondAnnotationValue));

    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{firstAnnotationKey, initialFirstAnnotationValue}, {secondAnnotationKey, initialSecondAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string updatedAnnotationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, updatedAnnotationValue));

    expectedAnnotationsOfFirstQuantumOperation[firstAnnotationKey] = updatedAnnotationValue;
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetAnnotationForUnknownQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string annotationKey   = "KEY";
    const std::string annotationValue = "VALUE";

    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(2, annotationKey, annotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, UpdateNotExistingAnnotationsForQuantumOperation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string firstAnnotationKey          = "KEY_ONE";
    const std::string initialFirstAnnotationValue = "InitialValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, initialFirstAnnotationValue));

    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsForFirstQuantumOperation = {{firstAnnotationKey, initialFirstAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsForFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string secondAnnotationKey          = "KEY_TWO";
    const std::string initialSecondAnnotationValue = "OtherValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, secondAnnotationKey, initialSecondAnnotationValue));
    expectedAnnotationsForFirstQuantumOperation[secondAnnotationKey] = initialSecondAnnotationValue;

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsForFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetAnnotationsForQuantumOperationWithEmptyKey) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});

    const std::string firstAnnotationKey          = "KEY_ONE";
    const std::string initialFirstAnnotationValue = "InitialValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, firstAnnotationKey, initialFirstAnnotationValue));

    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{firstAnnotationKey, initialFirstAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const std::string valueForAnnotationWithEmptyKey = "OtherValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, "", valueForAnnotationWithEmptyKey));
    expectedAnnotationsOfFirstQuantumOperation[""] = valueForAnnotationWithEmptyKey;

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetGlobalQuantumOperationAnnotation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string globalAnnotationKey   = "KEY_ONE";
    const std::string globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsForSecondQuantumOperation = {{globalAnnotationKey, globalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsForSecondQuantumOperation);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, UpdateGlobalQuantumOperationAnnotation) {
    const std::string globalAnnotationKey          = "KEY_ONE";
    const std::string initialGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string updatedGlobalAnnoatationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, updatedGlobalAnnoatationValue));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{globalAnnotationKey, updatedGlobalAnnoatationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumComputation);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, UpdateNotExistingGlobalQuantumOperationAnnotation) {
    const std::string firstGlobalAnnotationKey   = "KEY_ONE";
    const std::string firstGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(firstGlobalAnnotationKey, firstGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{firstGlobalAnnotationKey, firstGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string secondGlobalAnnotationKey   = "KEY_TWO";
    const std::string secondGlobalAnnotationValue = "OtherValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(secondGlobalAnnotationKey, secondGlobalAnnotationValue));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{firstGlobalAnnotationKey, firstGlobalAnnotationValue}, {secondGlobalAnnotationKey, secondGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumComputation);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, RemoveGlobalQuantumOperationAnnotation) {
    const std::string globalAnnotationKey          = "KEY_ONE";
    const std::string initialGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    ASSERT_TRUE(annotatedQuantumComputation->removeGlobalQuantumOperationAnnotation(globalAnnotationKey));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetGlobalQuantumOperationAnnotationWithEmptyKey) {
    const std::string globalAnnotationKey          = "KEY_ONE";
    const std::string initialGlobalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, initialGlobalAnnotationValue));

    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumComputation = {{globalAnnotationKey, initialGlobalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);

    const std::string valueOfAnnotationWithEmptyKey = "OtherValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation("", valueOfAnnotationWithEmptyKey));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumComputation = {{"", valueOfAnnotationWithEmptyKey}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumComputation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, SetGlobalQuantumOperationAnnotationMatchingExistingAnnotationOfGateDoesNotUpdateTheLatter) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string localAnnotationKey   = "KEY_ONE";
    const std::string localAnnotationValue = "LocalValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, localAnnotationValue));
    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{localAnnotationKey, localAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    const std::string& globalAnnotationKey   = localAnnotationKey;
    const std::string  globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumOperation = {{globalAnnotationKey, globalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumOperation);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, RemovingGlobalQuantumOperationAnnotationMatchingExistingAnnotationOfGateDoesNotRemoveTheLatter) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string localAnnotationKey   = "KEY_ONE";
    const std::string localAnnotationValue = "LocalValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, localAnnotationValue));
    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{localAnnotationKey, localAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    const std::string& globalAnnotationKey   = localAnnotationKey;
    const std::string  globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    ASSERT_TRUE(annotatedQuantumComputation->removeGlobalQuantumOperationAnnotation(globalAnnotationKey));

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, {});
}

TEST_F(AnnotatedQuantumComputationTestsFixture, UpdateLocalAnnotationWhoseKeyMatchesGlobalAnnotationDoesOnlyUpdateLocalAnnotation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const std::string localAnnotationKey   = "KEY_ONE";
    const std::string localAnnotationValue = "LocalValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, localAnnotationValue));
    AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfFirstQuantumOperation = {{localAnnotationKey, localAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    const std::string& globalAnnotationKey   = localAnnotationKey;
    const std::string  globalAnnotationValue = "InitialValue";
    ASSERT_FALSE(annotatedQuantumComputation->setOrUpdateGlobalQuantumOperationAnnotation(globalAnnotationKey, globalAnnotationValue));
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);

    constexpr qc::Qubit targetQubitTwoIndex = 1;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitTwoIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitTwoIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitTwoIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup expectedAnnotationsOfSecondQuantumOperation = {{globalAnnotationKey, globalAnnotationValue}};
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumOperation);

    const std::string updatedLocalAnnotationValue = "UpdatedValue";
    ASSERT_TRUE(annotatedQuantumComputation->setOrUpdateAnnotationOfQuantumOperation(0, localAnnotationKey, updatedLocalAnnotationValue));
    expectedAnnotationsOfFirstQuantumOperation[localAnnotationKey] = updatedLocalAnnotationValue;

    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, expectedAnnotationsOfFirstQuantumOperation);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 1, expectedAnnotationsOfSecondQuantumOperation);
}

TEST_F(AnnotatedQuantumComputationTestsFixture, GetAnnotationsOfUnknownQuantumOperationInQuantumComputation) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);
    assertThatAnnotationsOfQuantumOperationAreEqualTo(*annotatedQuantumComputation, 0, {});

    const AnnotatableQuantumComputation::QuantumOperationAnnotationsLookup annotationsForUnknownQuantumOperation = annotatedQuantumComputation->getAnnotationsOfQuantumOperation(2);
    ASSERT_TRUE(annotationsForUnknownQuantumOperation.empty());
}
// END Annotation tests

TEST_F(AnnotatedQuantumComputationTestsFixture, GetQuantumOperationUsingOutOfRangeIndexNotPossible) {
    std::vector<std::unique_ptr<qc::Operation>> expectedQuantumComputations;

    constexpr qc::Qubit targetQubitOneIndex = 0;
    assertAdditionOfNonAncillaryQubitForIndexSucceeds(*annotatedQuantumComputation, targetQubitOneIndex);

    ASSERT_TRUE(annotatedQuantumComputation->addOperationsImplementingNotGate(targetQubitOneIndex));
    expectedQuantumComputations.emplace_back(std::make_unique<qc::StandardOperation>(qc::Controls(), targetQubitOneIndex, qc::OpType::X));
    assertThatOperationsOfQuantumComputationAreEqualToSequence(*annotatedQuantumComputation, expectedQuantumComputations);

    ASSERT_THAT(annotatedQuantumComputation->getQuantumOperation(2), testing::IsNull());
    // Since we are using zero-based indices, an index equal to the number of quantum operations in the quantum computation should also not work
    ASSERT_THAT(annotatedQuantumComputation->getQuantumOperation(1), testing::IsNull());
}
