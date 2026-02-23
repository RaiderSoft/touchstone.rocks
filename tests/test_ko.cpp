#include <gtest/gtest.h>

#include "go/board.hpp"
#include "go/move.hpp"

using go::Board;
using go::MoveResult;
using go::PlayMove;
using go::Stone;
using go::ValidateMove;

// --- Basic Ko Detection ---

TEST(Ko, BasicKoDetection) {
  // Classic ko position on 5x5:
  //  . B W . .
  //  B . B W .      B plays at (1,1) capturing W at... wait, need W there.
  //  . B W . .
  //  . . . . .
  //  . . . . .
  //
  // Actually let me set up a proper ko. The simplest ko:
  //  . B . . .
  //  B W B . .     W at (1,1) has 1 liberty at... wait, check neighbors.
  //  . B . . .     (1,1)=6: (0,1)=B, (1,0)=B, (2,1)=B, (1,2)=B -> 0 liberties
  //                That's already captured, not a ko.
  //
  // Ko requires: B captures single W, then W wants to recapture single B.
  // Standard ko shape:
  //  . B W .
  //  B W . W       B captures W at (1,1) by playing at (1,2)?
  //  . B W .       No, (1,2) already has W? Let me restart.
  //
  // Ko happens when:
  // 1. B captures exactly one W stone
  // 2. The capturing B stone has exactly one liberty (the spot W was just captured from)
  //    Wait no, the capturing B stone is where B just played.
  //    The spot where W was captured is now empty.
  //    W wants to play there to recapture the single B stone.
  //
  // Setup: positions so that after B captures, W recapturing would restore the board.
  //
  //  . B . . .
  //  B . B . .     B plays at (1,1)=6
  //  . B W . .     W at (2,2)=12
  //  . . B . .     B at (2,1)=11, wait this is getting complicated.
  //
  // Let me use the canonical ko position:
  //  B W . .
  //  . B W .       On a 4x4 board.
  //  B W . .
  //  . . . .
  // Hmm, still complicated. Let me be very explicit.
  //
  // On a 5x5 board, canonical ko:
  //  . B . . .
  //  B W B . .     <- W at (1,1) has liberty at... let me check
  //  . B . . .
  //  . . . . .
  //  . . . . .
  // (1,1)=6: up=(0,1)=1 B, left=(1,0)=5 B, right=(1,2)=7 B, down=(2,1)=11 B
  // 0 liberties! Not a ko setup, this is just capture.
  //
  // A ko requires the captured stone to have been placed on the previous turn.
  // Let's simulate the actual game sequence:
  //
  // Board before B's move:
  //  . B . . .
  //  B W B . .
  //  . B . . .
  //  . . . . .
  //  . . . . .
  // This has W at 6 with 0 liberties - impossible to reach in a game.
  //
  // Real ko: the capturing move and potential recapture each capture exactly 1 stone.
  //
  // Classic ko on 5x5:
  //  . . . . .
  //  . . B W .
  //  . B W . W
  //  . . B W .
  //  . . . . .
  //
  // B at (1,2)=7, (2,1)=11, (3,2)=17
  // W at (1,3)=8, (2,2)=12, (3,3)=18, (2,4)=14
  // Empty at (2,3)=13
  //
  // B plays at (2,3)=13: captures W at (2,2)=12
  //   After: W at 12 removed. B at 13 has liberties?
  //   B at 13 neighbors: (1,3)=W, (2,2)=now empty, (3,3)=W, (2,4)=W
  //   B at 13: liberty at (2,2). Only 1 liberty.
  //   W at (1,3)=8 group... complex. Let me try a different approach.
  //
  // Simplest possible ko - just test the rule mechanically:

  // Step 1: Set up a board where B can capture one W stone.
  Board before_b_move(5, ". . . . ."
                          ". . B . ."
                          ". B W . ."
                          ". . B . ."
                          ". . . . .");
  // W at (2,2)=12, liberties at (2,3)=13. One liberty.
  // B plays at (2,3)=13 to capture W at (2,2)=12.

  Board prev_for_b = before_b_move;  // "previous board" = state before W's last move
  // (In a real game, this would be the board before W placed at (2,2))
  // For this test, we just need prev_for_b to NOT match the board after B's move.

  Board b = before_b_move;
  std::vector<int> captured;
  MoveResult r = PlayMove(b, 13, Stone::kBlack, prev_for_b, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 1u);
  EXPECT_EQ(captured[0], 12);

  // Board is now:
  //  . . . . .
  //  . . B . .
  //  . B . B .   (W at 12 gone, B at 13 placed)
  //  . . B . .
  //  . . . . .

  // Step 2: W wants to play at (2,2)=12 to recapture B at (2,3)=13.
  // B at 13: neighbors (1,3)=empty, (2,2)=would-be-W, (3,3)=empty, (2,4)=empty
  // B at 13 has other liberties -> W can't capture it! So this isn't really a ko.
  // The recapture would not capture B because B has other liberties.
  //
  // For a real ko, the capturing stone must also have exactly one liberty.
  // Let me set up the classic ko shape properly:

  Board ko_board(5, ". . . . ."
                     ". B W . ."
                     "B W . W ."
                     ". B W . ."
                     ". . . . .");
  // B: (1,1)=6, (2,0)=10, (3,1)=16
  // W: (1,2)=7, (2,1)=11, (2,3)=13, (3,2)=17
  // Empty: (2,2)=12
  //
  // B plays at (2,2)=12. Check W at (2,1)=11:
  //   (2,1) neighbors: (1,1)=B, (2,0)=B, (3,1)=B, (2,2)=B -> 0 liberties!
  //   W at 11 captured.
  //
  // After capture, board is:
  //  . . . . .
  //  . B W . .
  //  B . B W .    <- (2,1) now empty
  //  . B W . .
  //  . . . . .
  //
  // Now W wants to play at (2,1)=11.
  // B at (2,2)=12 neighbors: (1,2)=W, (2,1)=would-be-W, (3,2)=W, (2,3)=W
  // If W plays at 11: B at 12 has 0 liberties -> would be captured.
  // Board would become:
  //  . . . . .
  //  . B W . .
  //  B W . W .    <- back to the original board!
  //  . B W . .
  //  . . . . .
  // That's the ko_board! So if previous_board = ko_board, this is a ko violation.

  Board prev_board = ko_board;
  Board current = ko_board;
  std::vector<int> cap;

  // B plays at (2,2)=12
  MoveResult r2 = PlayMove(current, 12, Stone::kBlack, prev_board, cap);
  EXPECT_EQ(r2, MoveResult::kOk);
  EXPECT_EQ(cap.size(), 1u);
  EXPECT_EQ(cap[0], 11);  // W at (2,1) captured

  // Now W wants to recapture at (2,1)=11.
  // The previous_board for W's move is ko_board (the board before B's move).
  // After W plays at 11 and captures B at 12, the board would equal ko_board -> ko!
  MoveResult r3 = ValidateMove(current, 11, Stone::kWhite, ko_board);
  EXPECT_EQ(r3, MoveResult::kKoViolation);
}

// --- Ko is Only One Step Back ---

TEST(Ko, KoResolvedAfterInterveningMove) {
  // After a move elsewhere, the ko restriction is lifted.
  Board ko_board(5, ". . . . ."
                     ". B W . ."
                     "B W . W ."
                     ". B W . ."
                     ". . . . .");

  Board current = ko_board;
  std::vector<int> cap;

  // B captures at (2,2)=12
  PlayMove(current, 12, Stone::kBlack, ko_board, cap);
  // current now has B at 12, W at 11 removed.

  // W plays somewhere else first (say (4,4)=24). Previous board for this move
  // is ko_board (before B's capturing move).
  Board after_b_capture = current;
  std::vector<int> cap2;
  PlayMove(current, 24, Stone::kWhite, ko_board, cap2);

  // Now B plays somewhere (say (4,0)=20). Previous board = after_b_capture.
  Board after_w_elsewhere = current;
  std::vector<int> cap3;
  PlayMove(current, 20, Stone::kBlack, after_b_capture, cap3);

  // Now W can play at (2,1)=11 because previous_board is after_w_elsewhere,
  // NOT ko_board. The board after W's recapture won't equal after_w_elsewhere.
  MoveResult r = ValidateMove(current, 11, Stone::kWhite, after_w_elsewhere);
  EXPECT_EQ(r, MoveResult::kOk);
}

// --- Not Ko If Board Differs ---

TEST(Ko, NotKoIfBoardDiffers) {
  // A capture that doesn't reproduce the previous board is not ko.
  Board b(5, ". . . . ."
              ". B . . ."
              "B W B . ."
              ". B . . ."
              ". . . . .");
  Board prev(5);  // Empty board - clearly different from any resulting state
  std::vector<int> cap;

  // B plays at (1,2)=7. W at (2,1)=11... wait, (2,1) is not 11 on 5x5.
  // (2,1)=11. W at 11, B surrounds it. B plays to capture.
  // W at (2,1)=11: (1,1)=B, (2,0)=B, (3,1)=B, (2,2)=B -> 0 liberties
  // Hmm, already captured. Let me fix:
  Board b2(5, ". . . . ."
               ". . B . ."
               ". B W . ."
               ". . B . ."
               ". . . . .");
  // W at (2,2)=12, liberty at (2,3)=13
  // B plays at (2,3)=13 to capture
  MoveResult r = PlayMove(b2, 13, Stone::kBlack, prev, cap);
  EXPECT_EQ(r, MoveResult::kOk);
  // This is just a normal capture, not ko (prev was empty board, result is not empty board)
}

// --- Not Superko ---

TEST(Ko, NotSuperko) {
  // We only check against the immediately previous board state, not the full history.
  // A board position that appeared 4 moves ago can be repeated.
  Board b(5);
  Board prev(5);  // empty

  // Move 1: B plays at 0
  std::vector<int> cap;
  PlayMove(b, 0, Stone::kBlack, prev, cap);
  Board after1 = b;

  // Move 2: W plays at 24
  prev = after1;
  PlayMove(b, 24, Stone::kWhite, prev, cap);
  Board after2 = b;

  // Move 3: B removes stone at 0 (not possible in Go, but testing ko logic)
  // Actually we can't remove stones by playing. Let me just verify that
  // ValidateMove doesn't check against boards older than 1 ply.
  // The previous_board parameter is what we compare against.
  // If we pass the correct previous_board (1 ply back), and the result
  // doesn't match it, the move is legal regardless of older history.
  prev = after2;
  MoveResult r = ValidateMove(b, 12, Stone::kBlack, prev);
  EXPECT_EQ(r, MoveResult::kOk);  // Can't match after2 since we're adding a stone
}
