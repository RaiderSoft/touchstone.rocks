#include <gtest/gtest.h>

#include "go/board.h"
#include "go/move.h"

using go::Board;
using go::CountLiberties;
using go::FindGroup;
using go::Stone;

// --- Single Stone Liberties ---

TEST(Liberties, SingleStone_Center_4Liberties) {
  Board b(5, ". . . . ."
              ". . . . ."
              ". . B . ."
              ". . . . ."
              ". . . . .");
  EXPECT_EQ(CountLiberties(b, 12), 4);
}

TEST(Liberties, SingleStone_Corner_2Liberties) {
  Board b(5, "B . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  EXPECT_EQ(CountLiberties(b, 0), 2);
}

TEST(Liberties, SingleStone_Edge_3Liberties) {
  Board b(5, ". . B . ."
              ". . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  EXPECT_EQ(CountLiberties(b, 2), 3);
}

TEST(Liberties, EmptyPosition_0Liberties) {
  Board b(5);
  EXPECT_EQ(CountLiberties(b, 12), 0);
}

// --- Group Liberties ---

TEST(Liberties, TwoStoneGroup_SharedLiberties) {
  // Two adjacent stones share some neighbors.
  // B B at (2,2) and (2,3): neighbors are
  //   (1,2), (1,3), (2,1), (2,4), (3,2), (3,3) = 6 liberties
  Board b(5, ". . . . ."
              ". . . . ."
              ". . B B ."
              ". . . . ."
              ". . . . .");
  EXPECT_EQ(CountLiberties(b, 12), 6);
}

TEST(Liberties, GroupSurroundedOnThreeSides) {
  // B at (1,1) with opponent on three sides, one liberty remaining
  Board b(5, ". W . . ."
              "W B W . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  // (1,1)=6, neighbors: (0,1)=1 W, (1,0)=5 W, (1,2)=7 W, (2,1)=11 empty
  EXPECT_EQ(CountLiberties(b, 6), 1);
}

TEST(Liberties, GroupWithZeroLiberties) {
  Board b(5, ". B . . ."
              "B W B . ."
              ". B . . ."
              ". . . . ."
              ". . . . .");
  // W at (1,1)=6 surrounded by B on all 4 sides
  EXPECT_EQ(CountLiberties(b, 6), 0);
}

TEST(Liberties, OwnStoneReducesLiberties) {
  // B at (2,2) and (2,3). B at (1,2) is own stone, not a liberty.
  Board b(5, ". . . . ."
              ". . B . ."
              ". . B B ."
              ". . . . ."
              ". . . . .");
  // Group is (1,2)=7, (2,2)=12, (2,3)=13
  // Liberties: (0,2), (1,1), (1,3), (2,1), (2,4), (3,2), (3,3) = 7
  EXPECT_EQ(CountLiberties(b, 12), 7);
}

TEST(Liberties, OpponentStoneReducesLiberties) {
  Board b(5, ". . . . ."
              ". . W . ."
              ". . B . ."
              ". . . . ."
              ". . . . .");
  // B at (2,2)=12. (1,2)=7 is W (not a liberty).
  // Liberties: (2,1), (2,3), (3,2) = 3
  EXPECT_EQ(CountLiberties(b, 12), 3);
}

// --- Larger Group ---

TEST(Liberties, UShapedGroup) {
  Board b(5, ". . . . ."
              ". B . B ."
              ". B . B ."
              ". B B B ."
              ". . . . .");
  // U shape: (1,1)=6, (2,1)=11, (3,1)=16, (3,2)=17, (3,3)=18, (2,3)=13, (1,3)=8
  // 7 stones. Count the unique empty neighbors.
  auto g = FindGroup(b, 6);
  EXPECT_EQ(g.stones.size(), 7u);
  // Liberties: (0,1), (0,3), (1,0), (1,2), (2,0), (2,2), (2,4), (1,4), (3,0), (3,4), (4,1), (4,2), (4,3)
  EXPECT_EQ(CountLiberties(b, 6), 13);
}

// --- Diagram-Based Tests ---

TEST(Liberties, Diagram_SingleBlackCorner) {
  Board b(3, "B . ."
              ". . ."
              ". . .");
  EXPECT_EQ(CountLiberties(b, 0), 2);
}

TEST(Liberties, Diagram_CaptureReady) {
  Board b(3, ". B ."
              "B W B"
              ". B .");
  // W at center (1,1)=4 has 0 liberties
  EXPECT_EQ(CountLiberties(b, 4), 0);
}

TEST(Liberties, Diagram_AlmostCaptured) {
  Board b(3, ". B ."
              "B W ."
              ". B .");
  // W at (1,1)=4, neighbors: (0,1)=B, (1,0)=B, (2,1)=B, (1,2)=empty
  EXPECT_EQ(CountLiberties(b, 4), 1);
}
