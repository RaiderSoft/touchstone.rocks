#include <gtest/gtest.h>

#include "go/board.hpp"
#include "go/scoring.hpp"

using go::Board;
using go::CalculateScore;
using go::FindTerritories;
using go::FindTerritoryRegion;
using go::kKomi;
using go::ScoreResult;
using go::Stone;
using go::Territory;

// --- Territory Detection ---

TEST(Territory, EmptyBoardIsNeutral) {
  Board b(5);
  auto territories = FindTerritories(b);
  EXPECT_EQ(territories.size(), 1u);  // One big neutral region
  EXPECT_EQ(territories[0].owner, Stone::kEmpty);
  EXPECT_EQ(territories[0].positions.size(), 25u);
}

TEST(Territory, FullyEnclosedBlackTerritory) {
  Board b(5, "B B B B B"
              "B . . . B"
              "B . . . B"
              "B . . . B"
              "B B B B B");
  auto territories = FindTerritories(b);
  EXPECT_EQ(territories.size(), 1u);
  EXPECT_EQ(territories[0].owner, Stone::kBlack);
  EXPECT_EQ(territories[0].positions.size(), 9u);
}

TEST(Territory, FullyEnclosedWhiteTerritory) {
  Board b(5, "W W W W W"
              "W . . . W"
              "W . . . W"
              "W . . . W"
              "W W W W W");
  auto territories = FindTerritories(b);
  EXPECT_EQ(territories.size(), 1u);
  EXPECT_EQ(territories[0].owner, Stone::kWhite);
  EXPECT_EQ(territories[0].positions.size(), 9u);
}

TEST(Territory, NeutralTerritory_Dame) {
  // Empty region borders both colors -> neutral
  Board b(5, "B . . . W"
              ". . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  auto territories = FindTerritories(b);
  EXPECT_EQ(territories.size(), 1u);
  EXPECT_EQ(territories[0].owner, Stone::kEmpty);  // neutral
}

TEST(Territory, TwoSeparateTerritories) {
  // Left side Black territory, right side White territory
  Board b(5, "B B B W W"
              "B . B W ."
              "B . B W ."
              "B . B W ."
              "B B B W W");
  auto territories = FindTerritories(b);
  EXPECT_EQ(territories.size(), 2u);

  // Find which territory belongs to which
  Territory* black_t = nullptr;
  Territory* white_t = nullptr;
  for (auto& t : territories) {
    if (t.owner == Stone::kBlack) black_t = &t;
    if (t.owner == Stone::kWhite) white_t = &t;
  }
  ASSERT_NE(black_t, nullptr);
  ASSERT_NE(white_t, nullptr);
  EXPECT_EQ(black_t->positions.size(), 3u);
  EXPECT_EQ(white_t->positions.size(), 3u);
}

TEST(Territory, TerritoryDoesNotIncludeStones) {
  Board b(3, "B B B"
              "B . B"
              "B B B");
  auto territories = FindTerritories(b);
  EXPECT_EQ(territories.size(), 1u);
  EXPECT_EQ(territories[0].positions.size(), 1u);
  // The territory is just the center empty position, not the B stones
  EXPECT_EQ(territories[0].positions[0], 4);
}

TEST(Territory, SingleEmptyPositionTerritory) {
  Board b(3, "B B B"
              "B . B"
              "B B B");
  Territory t = FindTerritoryRegion(b, 4);
  EXPECT_EQ(t.owner, Stone::kBlack);
  EXPECT_EQ(t.positions.size(), 1u);
}

TEST(Territory, EmptyRegionBorderingNothing) {
  // If a region borders no stones at all, it's neutral.
  // On an empty board, the entire board is one neutral region.
  Board b(3);
  Territory t = FindTerritoryRegion(b, 0);
  EXPECT_EQ(t.owner, Stone::kEmpty);
}

// --- Scoring ---

TEST(Scoring, AllBlackBoard) {
  // Every position is Black
  Board b(3, "B B B"
              "B B B"
              "B B B");
  ScoreResult s = CalculateScore(b);
  EXPECT_EQ(s.black_stones, 9);
  EXPECT_EQ(s.white_stones, 0);
  EXPECT_EQ(s.black_territory, 0);
  EXPECT_EQ(s.white_territory, 0);
  EXPECT_DOUBLE_EQ(s.black_score, 9.0);
  EXPECT_DOUBLE_EQ(s.white_score, kKomi);  // 0 + 7.5
  EXPECT_EQ(s.winner, Stone::kBlack);
}

TEST(Scoring, EmptyBoardWhiteWinsByKomi) {
  Board b(3);
  ScoreResult s = CalculateScore(b);
  EXPECT_EQ(s.black_stones, 0);
  EXPECT_EQ(s.white_stones, 0);
  EXPECT_EQ(s.black_territory, 0);
  EXPECT_EQ(s.white_territory, 0);
  EXPECT_DOUBLE_EQ(s.black_score, 0.0);
  EXPECT_DOUBLE_EQ(s.white_score, kKomi);
  EXPECT_EQ(s.winner, Stone::kWhite);
}

TEST(Scoring, StonesAndTerritory) {
  // B has 16 stones forming the border, 9 territory inside
  // W has nothing
  Board b(5, "B B B B B"
              "B . . . B"
              "B . . . B"
              "B . . . B"
              "B B B B B");
  ScoreResult s = CalculateScore(b);
  EXPECT_EQ(s.black_stones, 16);
  EXPECT_EQ(s.black_territory, 9);
  EXPECT_DOUBLE_EQ(s.black_score, 25.0);
  EXPECT_EQ(s.white_stones, 0);
  EXPECT_EQ(s.white_territory, 0);
  EXPECT_DOUBLE_EQ(s.white_score, kKomi);
  EXPECT_EQ(s.winner, Stone::kBlack);
}

TEST(Scoring, BothSidesHaveTerritory) {
  // 5x5 board split vertically: B on left, W on right, each with interior territory.
  // Row 0: B B B W W  (3B + 2W)
  // Row 1: B . B W .  (2B + 1W)
  // Row 2: B . B W .  (2B + 1W)
  // Row 3: B . B W .  (2B + 1W)
  // Row 4: B B B W W  (3B + 2W)
  // Total: 12B, 7W. B territory: col1 rows1-3 = 3. W territory: col4 rows1-3 = 3.
  Board b(5, "B B B W W"
              "B . B W ."
              "B . B W ."
              "B . B W ."
              "B B B W W");
  ScoreResult s = CalculateScore(b);
  EXPECT_EQ(s.black_stones, 12);
  EXPECT_EQ(s.black_territory, 3);
  EXPECT_EQ(s.white_stones, 7);
  EXPECT_EQ(s.white_territory, 3);
  EXPECT_DOUBLE_EQ(s.black_score, 15.0);
  EXPECT_DOUBLE_EQ(s.white_score, 10.0 + kKomi);  // 17.5
  EXPECT_EQ(s.winner, Stone::kWhite);  // 15 < 17.5
}

TEST(Scoring, KomiBreaksTie) {
  // Equal stones, equal territory -> komi decides.
  // B: 3 stones, W: 3 stones, no territory. White wins by komi.
  Board b(3, "B . W"
              ". . ."
              "B . W");
  // Empty region borders both colors -> neutral. No territory for either.
  ScoreResult s = CalculateScore(b);
  EXPECT_EQ(s.black_stones, 2);
  EXPECT_EQ(s.white_stones, 2);
  EXPECT_EQ(s.black_territory, 0);
  EXPECT_EQ(s.white_territory, 0);
  EXPECT_DOUBLE_EQ(s.black_score, 2.0);
  EXPECT_DOUBLE_EQ(s.white_score, 2.0 + kKomi);  // 9.5
  EXPECT_EQ(s.winner, Stone::kWhite);  // Komi breaks the tie
}

TEST(Scoring, NeutralTerritoryNotCounted) {
  // Empty region bordering both colors is not counted for either
  Board b(3, "B . W"
              ". . ."
              "B . W");
  ScoreResult s = CalculateScore(b);
  EXPECT_EQ(s.black_territory, 0);
  EXPECT_EQ(s.white_territory, 0);
}
