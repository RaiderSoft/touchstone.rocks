#include <gtest/gtest.h>

#include "engine/katago_engine.hpp"

// --- PosToGtp tests ---

TEST(KataGoCoords, TopLeftCorner) {
  // pos 0 = row 0, col 0 = A9 on a 9x9 board.
  EXPECT_EQ(katago::PosToGtp(0, 9), "A9");
}

TEST(KataGoCoords, TopRightCorner) {
  // pos 8 = row 0, col 8 = J9 (column I is skipped).
  EXPECT_EQ(katago::PosToGtp(8, 9), "J9");
}

TEST(KataGoCoords, BottomLeftCorner) {
  // pos 72 = row 8, col 0 = A1.
  EXPECT_EQ(katago::PosToGtp(72, 9), "A1");
}

TEST(KataGoCoords, BottomRightCorner) {
  // pos 80 = row 8, col 8 = J1.
  EXPECT_EQ(katago::PosToGtp(80, 9), "J1");
}

TEST(KataGoCoords, Center) {
  // pos 40 = row 4, col 4 = E5.
  EXPECT_EQ(katago::PosToGtp(40, 9), "E5");
}

TEST(KataGoCoords, ColumnBeforeSkip) {
  // pos 7 = row 0, col 7 = H9 (last column before skip).
  EXPECT_EQ(katago::PosToGtp(7, 9), "H9");
}

TEST(KataGoCoords, PassPos) {
  EXPECT_EQ(katago::PosToGtp(-1, 9), "pass");
}

// --- GtpToPos tests ---

TEST(KataGoCoords, GtpTopLeft) { EXPECT_EQ(katago::GtpToPos("A9", 9), 0); }

TEST(KataGoCoords, GtpTopRight) { EXPECT_EQ(katago::GtpToPos("J9", 9), 8); }

TEST(KataGoCoords, GtpBottomLeft) {
  EXPECT_EQ(katago::GtpToPos("A1", 9), 72);
}

TEST(KataGoCoords, GtpBottomRight) {
  EXPECT_EQ(katago::GtpToPos("J1", 9), 80);
}

TEST(KataGoCoords, GtpCenter) { EXPECT_EQ(katago::GtpToPos("E5", 9), 40); }

TEST(KataGoCoords, GtpPass) { EXPECT_EQ(katago::GtpToPos("pass", 9), -1); }

TEST(KataGoCoords, GtpEmpty) { EXPECT_EQ(katago::GtpToPos("", 9), -1); }

// --- Round-trip tests ---

TEST(KataGoCoords, RoundTripAllPositions) {
  for (int pos = 0; pos < 81; pos++) {
    std::string gtp = katago::PosToGtp(pos, 9);
    int back = katago::GtpToPos(gtp, 9);
    EXPECT_EQ(back, pos) << "Failed round-trip for pos " << pos
                         << " (gtp: " << gtp << ")";
  }
}

TEST(KataGoCoords, NoColumnI) {
  // Verify no GTP string uses the letter 'I'.
  for (int pos = 0; pos < 81; pos++) {
    std::string gtp = katago::PosToGtp(pos, 9);
    EXPECT_EQ(gtp.find('I'), std::string::npos)
        << "GTP string contains 'I' for pos " << pos << ": " << gtp;
  }
}

// --- StoneToGtp tests ---

TEST(KataGoCoords, StoneBlack) {
  EXPECT_EQ(katago::StoneToGtp(go::Stone::kBlack), "B");
}

TEST(KataGoCoords, StoneWhite) {
  EXPECT_EQ(katago::StoneToGtp(go::Stone::kWhite), "W");
}

// --- Specific positions that exercise the I-skip ---

TEST(KataGoCoords, Column8IsJ) {
  // All positions in column 8 should produce 'J', not 'I'.
  for (int row = 0; row < 9; row++) {
    int pos = row * 9 + 8;
    std::string gtp = katago::PosToGtp(pos, 9);
    EXPECT_EQ(gtp[0], 'J') << "Row " << row << " col 8 should be J, got "
                            << gtp;
  }
}

TEST(KataGoCoords, Column7IsH) {
  // All positions in column 7 should produce 'H'.
  for (int row = 0; row < 9; row++) {
    int pos = row * 9 + 7;
    std::string gtp = katago::PosToGtp(pos, 9);
    EXPECT_EQ(gtp[0], 'H') << "Row " << row << " col 7 should be H, got "
                            << gtp;
  }
}
