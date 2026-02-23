#include <gtest/gtest.h>

#include <algorithm>

#include "go/board.hpp"
#include "go/move.hpp"

using go::ApplyCaptures;
using go::Board;
using go::MoveResult;
using go::PlayMove;
using go::Stone;

static bool Contains(const std::vector<int>& v, int val) {
  return std::find(v.begin(), v.end(), val) != v.end();
}

// --- Single Stone Capture ---

TEST(Capture, SingleStoneCapture) {
  //  . B .      . B .
  //  B W .  ->  B _ B  (B plays at (1,2), captures W at (1,1))
  //  . B .      . B .
  Board b(3, ". B ."
              "B W ."
              ". B .");
  Board prev(3);  // dummy previous board for ko
  std::vector<int> captured;

  // Place B at position 5 = (1,2)
  MoveResult r = PlayMove(b, 5, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 1u);
  EXPECT_TRUE(Contains(captured, 4));  // W was at (1,1)=4
  EXPECT_EQ(b.At(4), Stone::kEmpty);   // W is removed
  EXPECT_EQ(b.At(5), Stone::kBlack);   // B was placed
}

// --- Multi-Stone Group Capture ---

TEST(Capture, MultiStoneGroupCapture) {
  // Surround a 2-stone white group
  //  . B B .
  //  B W W .   B plays at (1,3) to capture W W
  //  . B B .
  //  . . . .
  Board b(4, ". B B ."
              "B W W ."
              ". B B ."
              ". . . .");
  Board prev(4);
  std::vector<int> captured;

  // Place B at (1,3)=7
  MoveResult r = PlayMove(b, 7, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 2u);
  EXPECT_TRUE(Contains(captured, 5));  // (1,1)
  EXPECT_TRUE(Contains(captured, 6));  // (1,2)
  EXPECT_EQ(b.At(5), Stone::kEmpty);
  EXPECT_EQ(b.At(6), Stone::kEmpty);
}

// --- Only Zero-Liberty Groups Are Captured ---

TEST(Capture, OnlyZeroLibertyGroupsCaptured) {
  // Two white groups adjacent to where B plays.
  // One has 0 liberties after placement, the other still has 1.
  //  . B . . .
  //  B W B W .   B plays at (1,2)=7... wait, (1,2) has B already
  //  . B . . .
  //  . . . . .
  //  . . . . .
  // Let me construct this more carefully.
  // W1 at (1,1) with liberties at only (1,2)
  // W2 at (1,3) with liberties at (1,2) and (1,4)
  // B plays at (1,2): W1 loses last liberty -> captured. W2 still has (1,4) -> safe.
  Board b(5, ". B . B ."
              "B W . W ."
              ". B . B ."
              ". . . . ."
              ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  // (1,1)=6 W has liberty at (1,2)=7
  // (1,3)=8 W has liberty at (1,2)=7 and (1,4)=9
  MoveResult r = PlayMove(b, 7, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  // W at (1,1)=6 had only (1,2) as liberty, now captured
  EXPECT_EQ(captured.size(), 1u);
  EXPECT_TRUE(Contains(captured, 6));
  // W at (1,3)=8 still has liberty at (1,4)=9, NOT captured
  EXPECT_EQ(b.At(8), Stone::kWhite);
}

// --- Multiple Groups Captured in One Move ---

TEST(Capture, MultipleSeparateGroupsCaptured) {
  // Two separate W stones, each with one liberty at the same position.
  //  . B .
  //  B W B
  //  . . .   <- B plays at (2,1) to also close below
  //  Wait, need to think about this more carefully.
  //
  //  B W B
  //  W . W    B plays at center (1,1) captures both W at (0,1) and... no,
  //  B W B    W at (0,1) has liberties from (0,0) and (0,2)
  //
  // Better: two isolated W stones on a 5x5, each surrounded except one shared liberty
  //  . B . B .
  //  B W B W B
  //  . B . B .
  //  . . . . .
  //  . . . . .
  //  But (1,1)=6 W has liberty... let me check
  //  (1,1) neighbors: (0,1)=B, (1,0)=B, (2,1)=B, (1,2)=B -> 0 liberties!
  //  That means W is already captured. Let me set it up so B plays to close both.

  // Two W stones each needing one more stone to capture:
  //  . B . B .
  //  B W . W B
  //  . . B . .
  //  . . . . .
  //  . . . . .
  // W at (1,1)=6: neighbors (0,1)=B, (1,0)=B, (2,1)=empty, (1,2)=empty -> 2 liberties
  // That's too many.

  // Simplest: column 1 and column 3 each have a W needing capture from same row
  Board b(5, ". B . B ."
              "B W B W B"
              ". B . B ."
              ". . . . ."
              ". . . . .");
  // W at (1,1)=6: neighbors (0,1)=1 B, (1,0)=5 B, (2,1)=11 B, (1,2)=7 B -> 0 liberties
  // W at (1,3)=8: neighbors (0,3)=3 B, (1,2)=7 B, (2,3)=13 B, (1,4)=9 B -> 0 liberties
  // Both already have 0 liberties! They'd be captured if any stone triggers it.
  // Actually, in a real game this board can't exist because the stones would have
  // been captured already. But for testing ApplyCaptures we can construct it.
  //
  // Let's test differently: B places a stone that simultaneously captures two groups.

  Board b2(5, ". B . B ."
               "B W . W B"
               ". B . B ."
               ". . . . ."
               ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  // W at (1,1)=6: neighbors (0,1)=B, (1,0)=B, (2,1)=B, (1,2)=empty -> 1 liberty at 7
  // W at (1,3)=8: neighbors (0,3)=B, (1,4)=B, (2,3)=B, (1,2)=empty -> 1 liberty at 7
  // B plays at (1,2)=7: both W groups lose their last liberty
  MoveResult r = PlayMove(b2, 7, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 2u);
  EXPECT_TRUE(Contains(captured, 6));
  EXPECT_TRUE(Contains(captured, 8));
}

// --- Edge and Corner Captures ---

TEST(Capture, CaptureOnEdge) {
  Board b(5, "B W B . ."
              ". . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  // W at (0,1)=1: neighbors (0,0)=B, (0,2)=B, (1,1)=empty -> 1 liberty
  // B plays at (1,1)=6 to capture
  MoveResult r = PlayMove(b, 6, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 1u);
  EXPECT_TRUE(Contains(captured, 1));
}

TEST(Capture, CaptureInCorner) {
  Board b(5, "W B . . ."
              "B . . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  // W at (0,0)=0: neighbors (0,1)=B, (1,0)=B -> 0 liberties
  // Already captured! But if we add it this way:
  // Let's set up so B plays the capturing move.
  Board b2(5, "W B . . ."
               ". . . . ."
               ". . . . ."
               ". . . . ."
               ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  // W at (0,0)=0: neighbors (0,1)=B, (1,0)=empty -> 1 liberty at (1,0)=5
  // B plays at (1,0)=5
  MoveResult r = PlayMove(b2, 5, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 1u);
  EXPECT_TRUE(Contains(captured, 0));
}

// --- CRITICAL: Capture Before Suicide Check ---

TEST(Capture, CaptureBeforeSuicideCheck) {
  // This is the key rule: if placing a stone would be suicide EXCEPT that
  // it captures opponent stones first (freeing liberties), the move is LEGAL.
  //
  //  . B .
  //  B W .     B plays at (2,1)=7... no wait, let me think about this.
  //  . B .
  //
  // We need: B plays into a position that looks like suicide, but captures
  // a W group first, making it legal.
  //
  //  W B .       B plays at... no, B and W are swapped for this.
  //
  // Classic example:
  //  B .        On a 2x3 board? No, let's use 5x5:
  //
  //  . B . . .
  //  B W B . .    B already surrounds W at (1,1)
  //  B . B . .    B plays at (2,1)=11, which would be self-atari EXCEPT...
  //  . B . . .    wait, (2,1) has neighbors (1,1)=W, (3,1)=B, (2,0)=B, (2,2)=B
  //               After B is placed: W at (1,1) has 0 liberties -> captured
  //               Then B at (2,1) has (1,1) as liberty -> legal!
  //
  // Actually the real classic is: Black plays inside White's territory to capture.

  Board b(5, ". B . . ."
              "B W B . ."
              "B . B . ."
              ". B . . ."
              ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  // B plays at (2,1)=11.
  // (2,1) neighbors: (1,1)=6 W, (3,1)=16 B, (2,0)=10 B, (2,2)=12 B
  // After placing B at 11: W at (1,1)=6 neighbors are all B -> 0 liberties -> captured
  // After capture: B at (2,1)=11 now has (1,1)=6 as empty neighbor -> has liberties -> legal
  MoveResult r = PlayMove(b, 11, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 1u);
  EXPECT_TRUE(Contains(captured, 6));
  EXPECT_EQ(b.At(6), Stone::kEmpty);   // W captured
  EXPECT_EQ(b.At(11), Stone::kBlack);  // B placed
}

// --- Large Group Capture ---

TEST(Capture, LargeGroupCapture) {
  // Capture a 4-stone white group
  Board b(5, ". B B B ."
              "B W W W B"
              "B W W W B"
              ". B B B ."
              ". . . . .");
  // W group: (1,1), (1,2), (1,3), (2,1), (2,2), (2,3) - actually 6 stones
  // Check: does this group have 0 liberties? Let me check.
  // Wait, need to verify boundaries. Let me check each W stone:
  // Already fully surrounded by B? Let me recheck:
  // (1,1)=6: (0,1)=B, (1,0)=B, (2,1)=W, (1,2)=W -> OK
  // (2,3)=13: (1,3)=W, (3,3)=B, (2,2)=W, (2,4)=B -> OK
  // All W have no empty neighbors -> 0 liberties
  // But this is already an illegal board state. Let me make B place the last stone.

  Board b2(5, ". B B B ."
               "B W W W B"
               "B W W W ."
               ". B B B ."
               ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  // W group has one liberty at (2,4)=14. B plays there.
  MoveResult r = PlayMove(b2, 14, Stone::kBlack, prev, captured);
  EXPECT_EQ(r, MoveResult::kOk);
  EXPECT_EQ(captured.size(), 6u);
  // All W stones should be removed
  for (int pos : captured) {
    EXPECT_EQ(b2.At(pos), Stone::kEmpty);
  }
}

// --- Placement on Occupied Position ---

TEST(Capture, OccupiedPositionRejected) {
  Board b(5, ". . . . ."
              ". . B . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  Board prev(5);
  std::vector<int> captured;

  MoveResult r = PlayMove(b, 7, Stone::kWhite, prev, captured);
  EXPECT_EQ(r, MoveResult::kOccupied);
}

TEST(Capture, OutOfBoundsRejected) {
  Board b(5);
  Board prev(5);
  std::vector<int> captured;

  EXPECT_EQ(PlayMove(b, -1, Stone::kBlack, prev, captured), MoveResult::kOutOfBounds);
  EXPECT_EQ(PlayMove(b, 25, Stone::kBlack, prev, captured), MoveResult::kOutOfBounds);
}
