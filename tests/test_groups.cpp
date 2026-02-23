#include <gtest/gtest.h>

#include <algorithm>

#include "go/board.hpp"
#include "go/move.hpp"

using go::Board;
using go::FindAllGroups;
using go::FindGroup;
using go::Group;
using go::Stone;

static bool Contains(const std::vector<int>& v, int val) {
  return std::find(v.begin(), v.end(), val) != v.end();
}

// --- Single Stones ---

TEST(GroupDetection, SingleStoneGroup) {
  Board b(5, ". . . . ."
              ". . B . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  Group g = FindGroup(b, 7);  // (1,2) on 5x5
  EXPECT_EQ(g.color, Stone::kBlack);
  EXPECT_EQ(g.stones.size(), 1u);
  EXPECT_EQ(g.stones[0], 7);
}

TEST(GroupDetection, EmptyPositionReturnsEmptyGroup) {
  Board b(5);
  Group g = FindGroup(b, 12);
  EXPECT_EQ(g.color, Stone::kEmpty);
  EXPECT_TRUE(g.stones.empty());
  EXPECT_TRUE(g.liberties.empty());
}

// --- Connected Groups ---

TEST(GroupDetection, TwoAdjacentStonesFormOneGroup) {
  Board b(5, ". . . . ."
              ". B B . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  // (1,1)=6 and (1,2)=7
  Group g = FindGroup(b, 6);
  EXPECT_EQ(g.stones.size(), 2u);
  EXPECT_TRUE(Contains(g.stones, 6));
  EXPECT_TRUE(Contains(g.stones, 7));
}

TEST(GroupDetection, DiagonalStonesAreSeparateGroups) {
  Board b(5, ". . . . ."
              ". B . . ."
              ". . B . ."
              ". . . . ."
              ". . . . .");
  // (1,1)=6 and (2,2)=12 are diagonal, NOT connected
  Group g1 = FindGroup(b, 6);
  Group g2 = FindGroup(b, 12);
  EXPECT_EQ(g1.stones.size(), 1u);
  EXPECT_EQ(g2.stones.size(), 1u);
}

TEST(GroupDetection, LShapedGroup) {
  Board b(5, ". . . . ."
              ". B . . ."
              ". B B . ."
              ". . . . ."
              ". . . . .");
  // (1,1)=6, (2,1)=11, (2,2)=12
  Group g = FindGroup(b, 6);
  EXPECT_EQ(g.stones.size(), 3u);
  EXPECT_TRUE(Contains(g.stones, 6));
  EXPECT_TRUE(Contains(g.stones, 11));
  EXPECT_TRUE(Contains(g.stones, 12));
}

TEST(GroupDetection, LargeConnectedGroup) {
  Board b(5, ". . . . ."
              ". B B B ."
              ". B . B ."
              ". B B B ."
              ". . . . .");
  // Ring of 8 stones, all connected
  Group g = FindGroup(b, 6);
  EXPECT_EQ(g.stones.size(), 8u);
}

TEST(GroupDetection, GroupDoesNotCrossColors) {
  Board b(5, ". . . . ."
              ". B W . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  Group gb = FindGroup(b, 6);
  Group gw = FindGroup(b, 7);
  EXPECT_EQ(gb.stones.size(), 1u);
  EXPECT_EQ(gw.stones.size(), 1u);
  EXPECT_EQ(gb.color, Stone::kBlack);
  EXPECT_EQ(gw.color, Stone::kWhite);
}

// --- FindAllGroups ---

TEST(FindAllGroups, MultipleBlackGroups) {
  Board b(5, "B . . . B"
              ". . . . ."
              ". . . . ."
              ". . . . ."
              "B . . . B");
  auto groups = FindAllGroups(b, Stone::kBlack);
  EXPECT_EQ(groups.size(), 4u);  // Four separate corner stones
}

TEST(FindAllGroups, NoGroupsOfColor) {
  Board b(5, "B B . . ."
              ". . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  auto groups = FindAllGroups(b, Stone::kWhite);
  EXPECT_EQ(groups.size(), 0u);
}

TEST(FindAllGroups, OneConnectedGroup) {
  Board b(5, "B B B . ."
              "B . . . ."
              "B . . . ."
              ". . . . ."
              ". . . . .");
  auto groups = FindAllGroups(b, Stone::kBlack);
  EXPECT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0].stones.size(), 5u);
}
