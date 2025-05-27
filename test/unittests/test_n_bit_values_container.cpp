/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "core/n_bit_values_container.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>

using namespace syrec;

namespace {
    void assertBitAtPositionHasValue(const NBitValuesContainer& nBitValuesContainer, std::size_t bitPositionInContainer, const std::optional<bool> expectedValueOfBit) {
        const std::optional<bool> actualValueOfBit = nBitValuesContainer.test(bitPositionInContainer);
        if (expectedValueOfBit.has_value()) {
            ASSERT_TRUE(actualValueOfBit.has_value());
            ASSERT_EQ(*expectedValueOfBit, *actualValueOfBit) << "Bit value mismatch @ position " << std::to_string(bitPositionInContainer) << ", Expected: " << std::to_string(static_cast<int>(*expectedValueOfBit)) << "| Actual: " << std::to_string(static_cast<int>(*actualValueOfBit));
        } else {
            ASSERT_FALSE(actualValueOfBit.has_value()) << "Bit value mismatch @ position " << std::to_string(bitPositionInContainer) << ", Expected: <None> | Actual: " << std::to_string(static_cast<int>(*actualValueOfBit));
        }
    }

    void assertBitValuesInContainerMatchSequence(const NBitValuesContainer& nBitValuesContainer, const std::initializer_list<std::optional<bool>>& expectedBitValuesStartingFromLowestBit) {
        ASSERT_EQ(expectedBitValuesStartingFromLowestBit.size(), nBitValuesContainer.size());
        std::size_t bitPosition = 0;
        for (const auto expectedBitValue: expectedBitValuesStartingFromLowestBit) {
            assertBitAtPositionHasValue(nBitValuesContainer, bitPosition++, expectedBitValue);
        }
    }
}; // namespace

TEST(NBitValuesContainerTests, CreateEmptyContainer) {
    auto nBitValuesContainer = std::make_unique<NBitValuesContainer>();
    ASSERT_EQ(0, nBitValuesContainer->size());
    ASSERT_EQ("", nBitValuesContainer->stringify());

    ASSERT_TRUE(nBitValuesContainer->none());
    assertBitAtPositionHasValue(*nBitValuesContainer, 0, std::nullopt);
    ASSERT_FALSE(nBitValuesContainer->set(0));
    ASSERT_FALSE(nBitValuesContainer->set(0, true));
    ASSERT_FALSE(nBitValuesContainer->reset(0));
    ASSERT_FALSE(nBitValuesContainer->flip(0));
}

TEST(NBitValuesContainerTests, CreateContainerOfSizeN) {
    constexpr std::size_t containerSize       = 3;
    auto                  nBitValuesContainer = std::make_unique<NBitValuesContainer>(containerSize);
    ASSERT_EQ(containerSize, nBitValuesContainer->size());
    ASSERT_EQ("000", nBitValuesContainer->stringify());

    ASSERT_TRUE(nBitValuesContainer->none());
    assertBitAtPositionHasValue(*nBitValuesContainer, 1, false);
}

TEST(NBitValuesContainerTests, CreateContainerOfSizeNAndInitWithIntegerWithNSmallerThanBitsOfInteger) {
    constexpr std::size_t   containerSize         = 5;
    constexpr std::uint64_t initializationInteger = 5; // 00101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);
    ASSERT_EQ(containerSize, nBitValuesContainer->size());
    ASSERT_EQ("10100", nBitValuesContainer->stringify());
    ASSERT_FALSE(nBitValuesContainer->none());

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true, false, false});
}

TEST(NBitValuesContainerTests, CreateContainerOfSizeNAndInitWithIntegerWithNLargerThanBitsOfInteger) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 29; // 11101 should be truncated to 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);
    ASSERT_EQ(containerSize, nBitValuesContainer->size());
    ASSERT_EQ("101", nBitValuesContainer->stringify());
    ASSERT_FALSE(nBitValuesContainer->none());

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, CreateContainerOfSizeNAndInitWithIntegerWithNEqualToBitsOfInteger) {
    constexpr std::size_t   containerSize         = 5;
    constexpr std::uint64_t initializationInteger = 29; // 11101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);
    ASSERT_EQ(containerSize, nBitValuesContainer->size());
    ASSERT_EQ("10111", nBitValuesContainer->stringify());
    ASSERT_FALSE(nBitValuesContainer->none());

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true, true, true});
}

// RESIZE
// Check all operations and no out of range access after resize
TEST(NBitValuesContainerTests, ResizeEmptyContainer) {
    auto                  nBitValuesContainer = std::make_unique<NBitValuesContainer>();
    constexpr std::size_t newContainerSize    = 3;
    nBitValuesContainer->resize(newContainerSize);

    ASSERT_EQ(newContainerSize, nBitValuesContainer->size());
    ASSERT_EQ("000", nBitValuesContainer->stringify());

    constexpr std::size_t toBeModifiedBitPosition = newContainerSize - 1;
    assertBitAtPositionHasValue(*nBitValuesContainer, toBeModifiedBitPosition, false);
    ASSERT_TRUE(nBitValuesContainer->none());

    ASSERT_TRUE(nBitValuesContainer->set(toBeModifiedBitPosition));
    assertBitAtPositionHasValue(*nBitValuesContainer, toBeModifiedBitPosition, true);

    ASSERT_TRUE(nBitValuesContainer->set(toBeModifiedBitPosition, false));
    assertBitAtPositionHasValue(*nBitValuesContainer, toBeModifiedBitPosition, false);

    ASSERT_TRUE(nBitValuesContainer->flip(toBeModifiedBitPosition));
    assertBitAtPositionHasValue(*nBitValuesContainer, toBeModifiedBitPosition, true);

    ASSERT_TRUE(nBitValuesContainer->reset(toBeModifiedBitPosition));
    assertBitAtPositionHasValue(*nBitValuesContainer, toBeModifiedBitPosition, false);
}

TEST(NBitValuesContainerTests, ResizeContainerByMakingItSmaller) {
    constexpr std::size_t   initialContainerSize  = 5;
    constexpr std::uint64_t initializationInteger = 29; // 11101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(initialContainerSize, initializationInteger);

    constexpr std::size_t newContainerSize = 3;
    nBitValuesContainer->resize(newContainerSize);
    ASSERT_EQ(newContainerSize, nBitValuesContainer->size());
    ASSERT_EQ("101", nBitValuesContainer->stringify());

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
    ASSERT_FALSE(nBitValuesContainer->none());

    constexpr std::size_t outOfRangeBitPositionInResizedContainer = initialContainerSize - 1;
    ASSERT_FALSE(nBitValuesContainer->test(outOfRangeBitPositionInResizedContainer).has_value());
    ASSERT_FALSE(nBitValuesContainer->set(outOfRangeBitPositionInResizedContainer));
    ASSERT_FALSE(nBitValuesContainer->set(outOfRangeBitPositionInResizedContainer, true));
    ASSERT_FALSE(nBitValuesContainer->flip(outOfRangeBitPositionInResizedContainer));
    ASSERT_FALSE(nBitValuesContainer->reset(outOfRangeBitPositionInResizedContainer));

    constexpr std::size_t inRangeBitPositionInResizedContainer = 1;
    ASSERT_TRUE(nBitValuesContainer->set(inRangeBitPositionInResizedContainer));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, true);

    ASSERT_TRUE(nBitValuesContainer->set(inRangeBitPositionInResizedContainer, false));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, false);

    ASSERT_TRUE(nBitValuesContainer->flip(inRangeBitPositionInResizedContainer));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, true);

    ASSERT_TRUE(nBitValuesContainer->reset(inRangeBitPositionInResizedContainer));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, false);
}

TEST(NBitValuesContainerTests, ResizeContainerByMakingItLarger) {
    constexpr std::size_t   initialContainerSize  = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(initialContainerSize, initializationInteger);

    constexpr std::size_t newContainerSize = 5;
    nBitValuesContainer->resize(newContainerSize);
    ASSERT_EQ(newContainerSize, nBitValuesContainer->size());
    ASSERT_EQ("10100", nBitValuesContainer->stringify());

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true, false, false});
    ASSERT_FALSE(nBitValuesContainer->none());

    constexpr std::size_t outOfRangeBitPositionInResizedContainer = newContainerSize + 1;
    ASSERT_FALSE(nBitValuesContainer->test(outOfRangeBitPositionInResizedContainer).has_value());
    ASSERT_FALSE(nBitValuesContainer->set(outOfRangeBitPositionInResizedContainer));
    ASSERT_FALSE(nBitValuesContainer->set(outOfRangeBitPositionInResizedContainer, true));
    ASSERT_FALSE(nBitValuesContainer->flip(outOfRangeBitPositionInResizedContainer));
    ASSERT_FALSE(nBitValuesContainer->reset(outOfRangeBitPositionInResizedContainer));

    constexpr std::size_t inRangeBitPositionInResizedContainer = initialContainerSize + 1;
    ASSERT_TRUE(nBitValuesContainer->set(inRangeBitPositionInResizedContainer));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, true);

    ASSERT_TRUE(nBitValuesContainer->set(inRangeBitPositionInResizedContainer, false));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, false);

    ASSERT_TRUE(nBitValuesContainer->flip(inRangeBitPositionInResizedContainer));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, true);

    ASSERT_TRUE(nBitValuesContainer->reset(inRangeBitPositionInResizedContainer));
    assertBitAtPositionHasValue(*nBitValuesContainer, inRangeBitPositionInResizedContainer, false);

    ASSERT_TRUE(nBitValuesContainer->reset(0));
    ASSERT_TRUE(nBitValuesContainer->reset(2));
    ASSERT_TRUE(nBitValuesContainer->none());

    ASSERT_TRUE(nBitValuesContainer->set(inRangeBitPositionInResizedContainer));
    ASSERT_FALSE(nBitValuesContainer->none());
}

TEST(NBitValuesContainerTests, ResizeContainerToSameSize) {
    constexpr std::size_t   initialContainerSize  = 5;
    constexpr std::uint64_t initializationInteger = 29; // 11101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(initialContainerSize, initializationInteger);

    nBitValuesContainer->resize(initialContainerSize);
    ASSERT_EQ(initialContainerSize, nBitValuesContainer->size());
    ASSERT_EQ("10111", nBitValuesContainer->stringify());

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true, true, true});
    ASSERT_FALSE(nBitValuesContainer->none());
}

// SET
TEST(NBitValuesContainerTests, SetOutOfRangeBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});

    ASSERT_FALSE(nBitValuesContainer->set(containerSize));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, SetAlreadySetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});

    ASSERT_TRUE(nBitValuesContainer->set(0));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, SetNotSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});

    ASSERT_TRUE(nBitValuesContainer->set(1));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, true, true});
}

// SET to specific value
TEST(NBitValuesContainerTests, SetOutOfRangeBitToSpecificValue) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});

    ASSERT_FALSE(nBitValuesContainer->set(containerSize, true));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});

    ASSERT_FALSE(nBitValuesContainer->set(containerSize, false));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, SetBitToSameValue) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_TRUE(nBitValuesContainer->set(0, true));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});

    ASSERT_TRUE(nBitValuesContainer->set(1, false));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, SetBitToInvertedValue) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_TRUE(nBitValuesContainer->set(0, false));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {false, false, true});

    ASSERT_TRUE(nBitValuesContainer->set(0, true));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

// FLIP
TEST(NBitValuesContainerTests, FlipOutOfRangeBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_FALSE(nBitValuesContainer->flip(containerSize));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, FlipSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_TRUE(nBitValuesContainer->flip(0));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {false, false, true});
}

TEST(NBitValuesContainerTests, FlipNotSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_TRUE(nBitValuesContainer->flip(1));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, true, true});
}

// TEST
TEST(NBitValuesContainerTests, TestOutOfRangeBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_FALSE(nBitValuesContainer->test(containerSize).has_value());
}

TEST(NBitValuesContainerTests, TestSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    const std::optional<bool> actualBitValue = nBitValuesContainer->test(0);
    ASSERT_TRUE(actualBitValue.has_value());
    ASSERT_TRUE(*actualBitValue);
}

TEST(NBitValuesContainerTests, TestNotSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    const std::optional<bool> actualBitValue = nBitValuesContainer->test(1);
    ASSERT_TRUE(actualBitValue.has_value());
    ASSERT_FALSE(*actualBitValue);
}

// RESET
TEST(NBitValuesContainerTests, ResetOutOfRangeBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_FALSE(nBitValuesContainer->reset(containerSize));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

TEST(NBitValuesContainerTests, ResetSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_TRUE(nBitValuesContainer->reset(containerSize - 1));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, false});
}

TEST(NBitValuesContainerTests, ResetNotSetBit) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 5; // 101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);

    ASSERT_TRUE(nBitValuesContainer->reset(1));
    assertBitValuesInContainerMatchSequence(*nBitValuesContainer, {true, false, true});
}

// SIZE
TEST(NBitValuesContainerTests, GetSizeOfEmptyContainer) {
    auto nBitValuesContainer = std::make_unique<NBitValuesContainer>();
    ASSERT_EQ(0, nBitValuesContainer->size());
}

TEST(NBitValuesContainerTests, GetSizeOfContainer) {
    constexpr std::size_t containerSize       = 3;
    auto                  nBitValuesContainer = std::make_unique<NBitValuesContainer>(containerSize);
    ASSERT_EQ(containerSize, nBitValuesContainer->size());

    constexpr std::size_t smallerContainerSize = containerSize - 1;
    nBitValuesContainer->resize(smallerContainerSize);
    ASSERT_EQ(smallerContainerSize, nBitValuesContainer->size());

    constexpr std::size_t largerContainerSize = containerSize + 2;
    nBitValuesContainer->resize(largerContainerSize);
    ASSERT_EQ(largerContainerSize, nBitValuesContainer->size());

    nBitValuesContainer->resize(0);
    ASSERT_EQ(0, nBitValuesContainer->size());
}

// NONE
TEST(NBitValuesContainerTests, CheckForNoSetBitInEmptyContainer) {
    auto nBitValuesContainer = std::make_unique<NBitValuesContainer>();
    ASSERT_TRUE(nBitValuesContainer->none());
}

TEST(NBitValuesContainerTests, CheckForNoSetBitInContainerWithNoBitSet) {
    constexpr std::size_t containerSize       = 5;
    auto                  nBitValuesContainer = std::make_unique<NBitValuesContainer>(containerSize);
    ASSERT_TRUE(nBitValuesContainer->none());
}

TEST(NBitValuesContainerTests, CheckForNoSetBitInContainerWithMultipleBitsSet) {
    constexpr std::size_t   containerSize         = 5;
    constexpr std::uint64_t initializationInteger = 3;
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);
    ASSERT_FALSE(nBitValuesContainer->none());
}

// STRINGIFY
TEST(NBitValuesContainerTests, StringifyEmptyContainer) {
    auto nBitValuesContainer = std::make_unique<NBitValuesContainer>();
    ASSERT_EQ("", nBitValuesContainer->stringify());
}

TEST(NBitValuesContainerTests, StringifyContainerWithNoBitsSet) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 0;
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);
    ASSERT_EQ("000", nBitValuesContainer->stringify());
}

TEST(NBitValuesContainerTests, StringifyContainerWithAllBitsSet) {
    constexpr std::size_t   containerSize         = 3;
    constexpr std::uint64_t initializationInteger = 7; // 111
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(containerSize, initializationInteger);
    ASSERT_EQ("111", nBitValuesContainer->stringify());
}

TEST(NBitValuesContainerTests, StringifyContainerWithSomeBitsSet) {
    constexpr std::size_t   initialContainerSize  = 5;
    constexpr std::uint64_t initializationInteger = 29; // 11101
    auto                    nBitValuesContainer   = std::make_unique<NBitValuesContainer>(initialContainerSize, initializationInteger);
    ASSERT_EQ("10111", nBitValuesContainer->stringify());
}
