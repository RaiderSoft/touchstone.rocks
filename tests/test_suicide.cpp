#include <gtest/gtest.h>

#include "go/board.h"
#include "go/move.h"

using go::Board;
using go::IsSuicide;
using go::MoveResult;
using go::PlayMove;
using go::Stone;
using go::ValidateMove;

// --- Basic Suicide is Illegal ---

TEST(Suicide, BasicSuicideIllegal) {
  // W surrounded on all 4 sides by B. Playing W there would be suicide.
  //  . B .
  //  B . B    W plays at center -> suicide (0 liberties, no captures)
  //  . B .
  Board b(3, ". B ."
              "B . B"
              ". B .");
  Board prev(3);

  EXPECT_TRUE(IsSuicide(b, 4, Stone::kWhite));
  EXPECT_EQ(ValidateMove(b, 4, Stone::kWhite, prev), MoveResult::kSuicide);
}

TEST(Suicide, SuicideInCorner) {
  //  . B
  //  B .     W plays at (0,0) -> suicide
  Board b(3, ". B ."
              "B . ."
              ". . .");
  EXPECT_TRUE(IsSuicide(b, 0, Stone::kWhite));
}

TEST(Suicide, SuicideOnEdge) {
  Board b(5, "B . B . ."
              ". B . . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  // W at (0,1)=1: neighbors (0,0)=B, (0,2)=B, (1,1)=B -> 0 liberties, no captures
  EXPECT_TRUE(IsSuicide(b, 1, Stone::kWhite));
}

// --- NOT Suicide If It Captures ---

TEST(Suicide, NotSuicideIfCaptures) {
  // Classic pattern: placing looks like suicide but captures first.
  //  . B . . .
  //  B W B . .
  //  B . B . .      B plays at (2,1)=11
  //  . B . . .
  //  . . . . .
  // After placing B at 11: W at (1,1)=6 has 0 liberties -> captured first
  // Then B at 11 has (1,1) as liberty -> NOT suicide
  Board b(5, ". B . . ."
              "B W B . ."
              "B . B . ."
              ". B . . ."
              ". . . . .");
  EXPECT_FALSE(IsSuicide(b, 11, Stone::kBlack));
}

TEST(Suicide, NotSuicideIfCapturesInCorner) {
  // W at (0,0), B at (0,1) and (1,0).
  // B plays at... wait, W is already captured (0 libs).
  // Better: Set up so White plays to capture Black.
  //  B W .
  //  W . .
  //  . . .
  // W plays at (1,1)=4: B at (0,0)=0 has neighbors (0,1)=W, (1,0)=W -> 0 liberties
  // B captured first, then W group at (0,1),(1,0),(1,1) has liberties -> legal
  Board b(3, "B W ."
              "W . ."
              ". . .");
  EXPECT_FALSE(IsSuicide(b, 4, Stone::kWhite));
}

// --- Suicide with Multiple Own Stones ---

TEST(Suicide, SuicideMultiStoneGroup) {
  // Playing into your own surrounded group is also suicide.
  //  . B . . .
  //  B W B . .
  //  B W B . .
  //  . B . . .
  //  . . . . .
  // W at (1,1) and (2,1). W plays at... hmm, they're already placed.
  // Better: W group at (1,1) and (2,1), B plays at (1,2) to fill.
  // No, let me construct: B plays into own surrounded multi-stone group.
  //
  //  W B W . .
  //  B . B . .
  //  W B W . .
  //  . . . . .
  //  . . . . .
  // W plays at (1,1)=6: own group would be (1,1) with neighbors all B/W...
  // Actually this is confusing. Let me simplify.
  //
  // Simple case: two empty spots inside B, both surrounded.
  //  B B B . .
  //  B . . B .
  //  B B B . .
  //  . . . . .
  //  . . . . .
  // W plays at (1,1)=6: W would be at 6, neighbor at (1,2)=7 is empty.
  // So W would have a liberty at 7. Not suicide.
  // W plays at (1,1)=6 and (1,2)=7 at once? No, that's two moves.
  //
  // Let me just test: group of W with no liberties and no captures.
  Board b(5, "B B B . ."
              "B W . B ."
              "B B B . ."
              ". . . . ."
              ". . . . .");
  // W at (1,1)=6, liberty at (1,2)=7.
  // If W plays at (1,2)=7: group {6,7} neighbors: (0,1)=B, (0,2)=B, (1,0)=B, (2,1)=B, (2,2)=B, (1,3)=B
  // All neighbors are B -> 0 liberties. No opponent captures (B groups all alive).
  // -> Suicide!
  EXPECT_TRUE(IsSuicide(b, 7, Stone::kWhite));
}

// --- Self-Atari is NOT Suicide ---

TEST(Suicide, SelfAtariIsNotSuicide) {
  // Reducing your own group to 1 liberty is legal (self-atari), not suicide.
  Board b(5, ". B . . ."
              "B . B . ."
              ". B . . ."
              ". . . . ."
              ". . . . .");
  // B at (0,1), (1,0), (1,2), (2,1). If W plays at (1,1)=6:
  // W at 6 has neighbors: (0,1)=B, (1,0)=B, (2,1)=B, (1,2)=B -> 0 liberties
  // No captures (all B groups have other liberties) -> suicide!
  // OK that's actually suicide. Let me make it self-atari for the placing color:
  //
  // B plays at (1,1) with own stones around:
  Board b2(5, ". B . . ."
               "B . . . ."
               ". B . . ."
               ". . . . ."
               ". . . . .");
  // B plays at (1,1)=6. Group becomes (0,1)=1, (1,0)=5, (1,1)=6, (2,1)=11.
  // Liberties of group: (0,0), (0,2), (1,2), (2,0), (2,2), (3,1) -> 6 liberties. Legal.
  EXPECT_FALSE(IsSuicide(b2, 6, Stone::kBlack));

  // Tighter self-atari:
  Board b3(5, ". W . . ."
               "W B W . ."
               ". . . . ."
               ". . . . ."
               ". . . . .");
  // B at (1,1)=6. If B plays at (2,1)=11:
  // Group {6, 11}. Neighbors: (0,1)=W, (1,0)=W, (1,2)=W, (2,0)=empty, (2,2)=empty, (3,1)=empty
  // 3 liberties. Legal, even though it's getting surrounded.
  EXPECT_FALSE(IsSuicide(b3, 11, Stone::kBlack));
}

// --- Legal Move Not Suicide ---

TEST(Suicide, NormalMoveNotSuicide) {
  Board b(5);  // empty board
  EXPECT_FALSE(IsSuicide(b, 12, Stone::kBlack));
  EXPECT_FALSE(IsSuicide(b, 0, Stone::kWhite));
}

TEST(Suicide, MoveNextToOpponentNotSuicide) {
  Board b(5, ". . . . ."
              ". . B . ."
              ". . . . ."
              ". . . . ."
              ". . . . .");
  // W plays adjacent to B at (1,2)=7. For example at (1,1)=6.
  // W at 6 has liberties: (0,1), (1,0), (2,1) -> 3 liberties. Legal.
  EXPECT_FALSE(IsSuicide(b, 6, Stone::kWhite));
}
