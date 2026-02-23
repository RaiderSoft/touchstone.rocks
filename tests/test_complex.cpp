#include <gtest/gtest.h>

#include "go/board.hpp"
#include "go/move.hpp"

using go::Board;
using go::MoveResult;
using go::Stone;
using go::ValidateMove;

// --- Eyes ---

TEST(Complex, TwoEyeGroupCannotBeCaptured) {
  // A group with two separate internal eyes is alive - the opponent
  // cannot fill both eyes (each would be suicide).
  //
  //  B B B B B
  //  B . B . B    Two eyes at (1,1) and (1,3)
  //  B B B B B
  //  . . . . .
  //  . . . . .
  Board b(5, "B B B B B"
              "B . B . B"
              "B B B B B"
              ". . . . ."
              ". . . . .");
  // W plays at eye (1,1)=6: suicide (surrounded by B, no captures)
  EXPECT_EQ(ValidateMove(b, 6, Stone::kWhite, Board(5)), MoveResult::kSuicide);
  // W plays at eye (1,3)=8: also suicide
  EXPECT_EQ(ValidateMove(b, 8, Stone::kWhite, Board(5)), MoveResult::kSuicide);
}

TEST(Complex, OneEyeGroupCanBeCaptured) {
  // A group with only one eye can be captured.
  // W plays at the eye, capturing the entire group (capture before suicide check).
  //
  //  W W W W W
  //  W B B B W
  //  W B . B W    Eye at (2,2)=12
  //  W B B B W
  //  W W W W W
  Board b(5, "W W W W W"
              "W B B B W"
              "W B . B W"
              "W B B B W"
              "W W W W W");
  Board prev(5);
  std::vector<int> cap;
  MoveResult r = go::PlayMove(b, 12, Stone::kWhite, prev, cap);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(cap.size(), 8u);  // All 8 B stones captured
}

TEST(Complex, FalseEyeCapturable) {
  // B group with a single "eye" that W can fill to capture.
  //  W W W W .
  //  W B B W .
  //  W B . W .
  //  W W W W .
  //  . . . . .
  Board b(5, "W W W W ."
              "W B B W ."
              "W B . W ."
              "W W W W ."
              ". . . . .");
  Board prev(5);
  std::vector<int> cap;
  MoveResult r = go::PlayMove(b, 12, Stone::kWhite, prev, cap);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(cap.size(), 3u);  // B at (1,1), (1,2), (2,1) captured
}
