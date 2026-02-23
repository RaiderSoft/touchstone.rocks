#include <gtest/gtest.h>

#include "go/board.hpp"

using go::Board;
using go::Stone;

// --- Construction ---

TEST(BoardConstruction, EmptyBoardAllEmpty) {
  Board b(9);
  for (int i = 0; i < 81; ++i) {
    EXPECT_EQ(b.At(i), Stone::kEmpty);
  }
}

TEST(BoardConstruction, SizeAccessor) {
  EXPECT_EQ(Board(9).Size(), 9);
  EXPECT_EQ(Board(13).Size(), 13);
  EXPECT_EQ(Board(19).Size(), 19);
}

TEST(BoardConstruction, NumPositions) {
  EXPECT_EQ(Board(9).NumPositions(), 81);
  EXPECT_EQ(Board(5).NumPositions(), 25);
}

TEST(BoardConstruction, FromStringSimple) {
  Board b(3, "B . W"
              ". B ."
              "W . B");
  EXPECT_EQ(b.At(0), Stone::kBlack);
  EXPECT_EQ(b.At(1), Stone::kEmpty);
  EXPECT_EQ(b.At(2), Stone::kWhite);
  EXPECT_EQ(b.At(3), Stone::kEmpty);
  EXPECT_EQ(b.At(4), Stone::kBlack);
  EXPECT_EQ(b.At(5), Stone::kEmpty);
  EXPECT_EQ(b.At(6), Stone::kWhite);
  EXPECT_EQ(b.At(7), Stone::kEmpty);
  EXPECT_EQ(b.At(8), Stone::kBlack);
}

TEST(BoardConstruction, FromStringIgnoresWhitespace) {
  Board a(3, "B.W .B. W.B");
  Board b(3, "B . W\n. B .\nW . B");
  Board c(3, "  B.W.B.W.B  ");
  EXPECT_EQ(a, b);
  EXPECT_EQ(b, c);
}

TEST(BoardConstruction, FromStringWrongSizeThrows) {
  EXPECT_THROW(Board(3, "B . W . B"), std::invalid_argument);
}

// --- Set and Get ---

TEST(BoardSetGet, SetAndGet) {
  Board b(5);
  b.Set(12, Stone::kBlack);
  EXPECT_EQ(b.At(12), Stone::kBlack);
  b.Set(12, Stone::kWhite);
  EXPECT_EQ(b.At(12), Stone::kWhite);
  b.Set(12, Stone::kEmpty);
  EXPECT_EQ(b.At(12), Stone::kEmpty);
}

TEST(BoardSetGet, RemoveStones) {
  Board b(3, "B B B"
              ". . ."
              ". . .");
  b.RemoveStones({0, 1, 2});
  EXPECT_EQ(b.At(0), Stone::kEmpty);
  EXPECT_EQ(b.At(1), Stone::kEmpty);
  EXPECT_EQ(b.At(2), Stone::kEmpty);
}

// --- Equality ---

TEST(BoardEquality, EqualBoards) {
  Board a(3, "B . W"
              ". B ."
              "W . B");
  Board b(3, "B . W"
              ". B ."
              "W . B");
  EXPECT_EQ(a, b);
}

TEST(BoardEquality, DifferByOneStone) {
  Board a(3, "B . W"
              ". B ."
              "W . B");
  Board b(3, "B . W"
              ". W ."
              "W . B");
  EXPECT_NE(a, b);
}

TEST(BoardEquality, DifferentSizes) {
  Board a(3);
  Board b(5);
  EXPECT_NE(a, b);
}

// --- ToString Round-Trip ---

TEST(BoardToString, RoundTrip) {
  std::string layout = ". B W\nB . B\nW B .";
  Board b(3, layout);
  EXPECT_EQ(b.ToString(), layout);
}

TEST(BoardToString, EmptyBoard) {
  Board b(3);
  EXPECT_EQ(b.ToString(), ". . .\n. . .\n. . .");
}

// --- Position Conversion ---

TEST(PositionConversion, TopLeft) {
  Board b(9);
  EXPECT_EQ(b.PosFromRowCol(0, 0), 0);
}

TEST(PositionConversion, BottomRight) {
  Board b(9);
  EXPECT_EQ(b.PosFromRowCol(8, 8), 80);
}

TEST(PositionConversion, RoundTrip) {
  Board b(9);
  for (int pos = 0; pos < 81; ++pos) {
    int row = b.RowOf(pos);
    int col = b.ColOf(pos);
    EXPECT_EQ(b.PosFromRowCol(row, col), pos);
  }
}

TEST(PositionConversion, InBoundsValid) {
  Board b(9);
  EXPECT_TRUE(b.InBounds(0));
  EXPECT_TRUE(b.InBounds(80));
  EXPECT_TRUE(b.InBounds(40));
}

TEST(PositionConversion, InBoundsInvalid) {
  Board b(9);
  EXPECT_FALSE(b.InBounds(-1));
  EXPECT_FALSE(b.InBounds(81));
  EXPECT_FALSE(b.InBounds(100));
}

// --- CountStones ---

TEST(CountStones, EmptyBoard) {
  Board b(9);
  EXPECT_EQ(b.CountStones(Stone::kEmpty), 81);
  EXPECT_EQ(b.CountStones(Stone::kBlack), 0);
  EXPECT_EQ(b.CountStones(Stone::kWhite), 0);
}

TEST(CountStones, MixedBoard) {
  Board b(3, "B B W"
              ". B ."
              "W . B");
  EXPECT_EQ(b.CountStones(Stone::kBlack), 4);
  EXPECT_EQ(b.CountStones(Stone::kWhite), 2);
  EXPECT_EQ(b.CountStones(Stone::kEmpty), 3);
}

// --- Opponent ---

TEST(Opponent, BlackToWhite) {
  EXPECT_EQ(go::Opponent(Stone::kBlack), Stone::kWhite);
}

TEST(Opponent, WhiteToBlack) {
  EXPECT_EQ(go::Opponent(Stone::kWhite), Stone::kBlack);
}
