// Copyright (c) 2023 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/lok_tilebuffer.h"
#include "office_client.h"

#include "gin/converter.h"
#include "office/test/office_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace electron::office {

using TileBufferTest = OfficeTest;

TEST_F(TileBufferTest, SingleEventHandler) {
}

TEST(TileBufferUtilityTest, SimplifyRanges) {
  std::vector<TileRange> empty;

  EXPECT_TRUE(SimplifyRanges(empty).size() == 0U);

  std::vector<TileRange> single = {{0, 5}};
  std::vector<TileRange> single_expected = {{0,5}};
  EXPECT_TRUE(SimplifyRanges(single) == single_expected);

  std::vector<TileRange> no_overlap = {{0, 5}, {6, 20}};
  std::vector<TileRange> no_overlap_expected = {{0, 5}, {6, 20}};
  EXPECT_TRUE(SimplifyRanges(no_overlap) == no_overlap_expected);

  std::vector<TileRange> single_overlap = {{0, 5}, {5, 20}};
  std::vector<TileRange> single_overlap_expected = {{0, 20}};
  EXPECT_TRUE(SimplifyRanges(single_overlap) == single_overlap_expected);

  std::vector<TileRange> multi_overlap = {{0, 5}, {5, 20}, {3, 10}};
  std::vector<TileRange> multi_overlap_expected = {{0, 20}};
  EXPECT_TRUE(SimplifyRanges(multi_overlap) == multi_overlap_expected);

  std::vector<TileRange> multi_overlap2 = {{0, 5}, {5, 20}, {3, 10}, {21, 21}};
  std::vector<TileRange> multi_overlap_expected2 = {{0, 20}, {21, 21}};
  EXPECT_TRUE(SimplifyRanges(multi_overlap2) == multi_overlap_expected2);
}

TEST(TileBufferUtilityTest, TileCount) {
  std::vector<TileRange> empty;
  EXPECT_EQ(TileCount(empty), size_t(0));

  std::vector<TileRange> single = {{0, 5}};
  EXPECT_EQ(TileCount(single), size_t(6));

  std::vector<TileRange> separate = {{0, 5}, {6, 20}};
  EXPECT_EQ(TileCount(separate), size_t(6 + 15));

  std::vector<TileRange> multi = {{0, 5}, {6, 20}, {21, 21}};
  EXPECT_EQ(TileCount(multi), size_t(6 + 15 + 1));
}

}  // namespace electron::office
