#include <gtest/gtest.h>

#include <algorithm>

#include "go/board.h"

using go::Board;

// Helper: check if a vector contains a value.
static bool Contains(const std::vector<int>& v, int val) {
  return std::find(v.begin(), v.end(), val) != v.end();
}

// --- Neighbor Count by Position Type ---

TEST(Neighbors, CenterHas4) {
  Board b(9);
  // Position (4,4) = 40
  auto n = b.Neighbors(40);
  EXPECT_EQ(n.size(), 4u);
}

TEST(Neighbors, CornerTopLeftHas2) {
  Board b(9);
  auto n = b.Neighbors(0);
  EXPECT_EQ(n.size(), 2u);
  EXPECT_TRUE(Contains(n, 1));   // right
  EXPECT_TRUE(Contains(n, 9));   // down
}

TEST(Neighbors, CornerTopRightHas2) {
  Board b(9);
  auto n = b.Neighbors(8);
  EXPECT_EQ(n.size(), 2u);
  EXPECT_TRUE(Contains(n, 7));   // left
  EXPECT_TRUE(Contains(n, 17));  // down
}

TEST(Neighbors, CornerBottomLeftHas2) {
  Board b(9);
  auto n = b.Neighbors(72);
  EXPECT_EQ(n.size(), 2u);
  EXPECT_TRUE(Contains(n, 63));  // up
  EXPECT_TRUE(Contains(n, 73));  // right
}

TEST(Neighbors, CornerBottomRightHas2) {
  Board b(9);
  auto n = b.Neighbors(80);
  EXPECT_EQ(n.size(), 2u);
  EXPECT_TRUE(Contains(n, 71));  // up
  EXPECT_TRUE(Contains(n, 79));  // left
}

TEST(Neighbors, TopEdgeHas3) {
  Board b(9);
  // Position (0,4) = 4
  auto n = b.Neighbors(4);
  EXPECT_EQ(n.size(), 3u);
  EXPECT_TRUE(Contains(n, 3));   // left
  EXPECT_TRUE(Contains(n, 5));   // right
  EXPECT_TRUE(Contains(n, 13));  // down
}

TEST(Neighbors, LeftEdgeHas3) {
  Board b(9);
  // Position (4,0) = 36
  auto n = b.Neighbors(36);
  EXPECT_EQ(n.size(), 3u);
  EXPECT_TRUE(Contains(n, 27));  // up
  EXPECT_TRUE(Contains(n, 45));  // down
  EXPECT_TRUE(Contains(n, 37));  // right
}

// --- No Diagonal Neighbors ---

TEST(Neighbors, NoDiagonals) {
  Board b(9);
  // Position (1,1) = 10. Diagonals would be 0, 2, 18, 20.
  auto n = b.Neighbors(10);
  EXPECT_FALSE(Contains(n, 0));
  EXPECT_FALSE(Contains(n, 2));
  EXPECT_FALSE(Contains(n, 18));
  EXPECT_FALSE(Contains(n, 20));
}

// --- Symmetry ---

TEST(Neighbors, Symmetric) {
  Board b(9);
  for (int pos = 0; pos < 81; ++pos) {
    for (int nbr : b.Neighbors(pos)) {
      auto nbr_neighbors = b.Neighbors(nbr);
      EXPECT_TRUE(Contains(nbr_neighbors, pos))
          << "Pos " << pos << " has neighbor " << nbr
          << " but " << nbr << " doesn't have neighbor " << pos;
    }
  }
}

// --- Small Board ---

TEST(Neighbors, SmallBoard3x3Center) {
  Board b(3);
  // Position (1,1) = 4. Neighbors: 1, 3, 5, 7
  auto n = b.Neighbors(4);
  EXPECT_EQ(n.size(), 4u);
  EXPECT_TRUE(Contains(n, 1));
  EXPECT_TRUE(Contains(n, 3));
  EXPECT_TRUE(Contains(n, 5));
  EXPECT_TRUE(Contains(n, 7));
}

TEST(Neighbors, SmallBoard3x3Corner) {
  Board b(3);
  auto n = b.Neighbors(0);
  EXPECT_EQ(n.size(), 2u);
  EXPECT_TRUE(Contains(n, 1));
  EXPECT_TRUE(Contains(n, 3));
}
