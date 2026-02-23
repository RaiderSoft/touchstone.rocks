#include <gtest/gtest.h>

#include "go/game.h"

using go::Game;
using go::GamePhase;
using go::MoveResult;
using go::Stone;

// --- Initial State ---

TEST(GameFlow, NewGameStartsWithBlack) {
  Game g(9);
  EXPECT_EQ(g.CurrentPlayer(), Stone::kBlack);
}

TEST(GameFlow, NewGameIsPlaying) {
  Game g(9);
  EXPECT_EQ(g.Phase(), GamePhase::kPlaying);
}

TEST(GameFlow, NewGameMoveNumberIsZero) {
  Game g(9);
  EXPECT_EQ(g.MoveNumber(), 0);
}

TEST(GameFlow, NewGameNoConsecutivePasses) {
  Game g(9);
  EXPECT_EQ(g.ConsecutivePasses(), 0);
}

// --- Turn Alternation ---

TEST(GameFlow, PlayAlternatesTurns) {
  Game g(9);
  EXPECT_EQ(g.CurrentPlayer(), Stone::kBlack);
  g.Play(0);
  EXPECT_EQ(g.CurrentPlayer(), Stone::kWhite);
  g.Play(1);
  EXPECT_EQ(g.CurrentPlayer(), Stone::kBlack);
}

TEST(GameFlow, PassAlternatesTurns) {
  Game g(9);
  EXPECT_EQ(g.CurrentPlayer(), Stone::kBlack);
  g.Pass();
  EXPECT_EQ(g.CurrentPlayer(), Stone::kWhite);
}

// --- Move Number ---

TEST(GameFlow, MoveNumberIncrements) {
  Game g(9);
  g.Play(0);
  EXPECT_EQ(g.MoveNumber(), 1);
  g.Play(1);
  EXPECT_EQ(g.MoveNumber(), 2);
}

TEST(GameFlow, PassDoesNotIncrementMoveNumber) {
  Game g(9);
  g.Play(0);
  EXPECT_EQ(g.MoveNumber(), 1);
  g.Pass();
  EXPECT_EQ(g.MoveNumber(), 1);
}

// --- Occupied Position ---

TEST(GameFlow, PlayOnOccupiedFails) {
  Game g(9);
  EXPECT_EQ(g.Play(40), MoveResult::kOk);
  EXPECT_EQ(g.Play(40), MoveResult::kOccupied);
  // Turn should NOT have changed on failed move
  EXPECT_EQ(g.CurrentPlayer(), Stone::kWhite);
}

// --- Passing and Game End ---

TEST(GameFlow, TwoConsecutivePassesEndGame) {
  Game g(9);
  g.Pass();
  EXPECT_EQ(g.ConsecutivePasses(), 1);
  EXPECT_EQ(g.Phase(), GamePhase::kPlaying);
  g.Pass();
  EXPECT_EQ(g.ConsecutivePasses(), 2);
  EXPECT_EQ(g.Phase(), GamePhase::kGameOver);
}

TEST(GameFlow, PlayResetsConsecutivePasses) {
  Game g(9);
  g.Pass();
  EXPECT_EQ(g.ConsecutivePasses(), 1);
  g.Play(0);
  EXPECT_EQ(g.ConsecutivePasses(), 0);
}

TEST(GameFlow, PlayAfterGameOverFails) {
  Game g(9);
  g.Pass();
  g.Pass();
  EXPECT_EQ(g.Phase(), GamePhase::kGameOver);
  EXPECT_NE(g.Play(0), MoveResult::kOk);
}

// --- Capture Tracking ---

TEST(GameFlow, CaptureCountTracking) {
  Game g(5);
  // Set up a capture: B surrounds W
  // B at (0,1), (1,0), (1,2), (2,1) surrounding (1,1)
  g.Play(1);   // B at (0,1)
  g.Play(6);   // W at (1,1)
  g.Play(5);   // B at (1,0)
  g.Play(24);  // W plays elsewhere
  g.Play(7);   // B at (1,2)
  g.Play(23);  // W plays elsewhere
  g.Play(11);  // B at (2,1) - captures W at (1,1)

  EXPECT_EQ(g.CapturedBy(Stone::kBlack), 1);
  EXPECT_EQ(g.CapturedBy(Stone::kWhite), 0);
}

// --- Board State ---

TEST(GameFlow, BoardReflectsPlacement) {
  Game g(9);
  g.Play(40);
  EXPECT_EQ(g.GetBoard().At(40), Stone::kBlack);
  g.Play(41);
  EXPECT_EQ(g.GetBoard().At(41), Stone::kWhite);
}

// --- Full Short Game ---

TEST(GameFlow, FullShortGame) {
  Game g(5);
  // Play a few moves, then both pass.
  EXPECT_EQ(g.Play(0), MoveResult::kOk);   // B
  EXPECT_EQ(g.Play(24), MoveResult::kOk);  // W
  EXPECT_EQ(g.Play(1), MoveResult::kOk);   // B
  EXPECT_EQ(g.Play(23), MoveResult::kOk);  // W

  g.Pass();  // B passes
  g.Pass();  // W passes

  EXPECT_EQ(g.Phase(), GamePhase::kGameOver);

  auto score = g.Score();
  EXPECT_EQ(score.black_stones, 2);
  EXPECT_EQ(score.white_stones, 2);
}

// --- Score After Game ---

TEST(GameFlow, ScoreWithTerritory) {
  Game g(3);
  // B fills top row, W fills bottom row, middle is contested
  g.Play(0);  // B
  g.Play(6);  // W
  g.Play(1);  // B
  g.Play(7);  // W
  g.Play(2);  // B
  g.Play(8);  // W

  g.Pass();
  g.Pass();

  EXPECT_EQ(g.Phase(), GamePhase::kGameOver);
  auto score = g.Score();
  EXPECT_EQ(score.black_stones, 3);
  EXPECT_EQ(score.white_stones, 3);
  // Middle row borders both colors -> neutral
  EXPECT_EQ(score.black_territory, 0);
  EXPECT_EQ(score.white_territory, 0);
}
