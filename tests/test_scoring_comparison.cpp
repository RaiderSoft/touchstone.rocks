#include <gtest/gtest.h>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

#include "go/board.h"
#include "go/scoring.h"

// Integration tests: score end-game positions with both our internal
// CalculateScore() and John Tromp's reference Tromp-Taylor scorer,
// verify they produce identical results.
// Requires: tools/tromp_score (compiled from Tromp's Haskell code).

static std::string FindTrompScorer() {
  // Try relative to the test binary (build dir is one level up from tools/).
  const char* paths[] = {
      "tools/tromp_score",
      "../tools/tromp_score",
      "../../tools/tromp_score",
  };
  for (const char* p : paths) {
    if (access(p, X_OK) == 0) return p;
  }
  return "";
}

class ScoringComparison : public ::testing::Test {
 protected:
  static std::string scorer_path_;

  static void SetUpTestSuite() {
    scorer_path_ = FindTrompScorer();
    if (scorer_path_.empty()) {
      GTEST_SKIP() << "tools/tromp_score not found — build it with: "
                      "ghc -o tools/tromp_score tools/tromp_score.hs";
    }
  }

  // Build stdin input for tromp_score from a Board.
  static std::string BoardToInput(const go::Board& board) {
    std::ostringstream ss;
    ss << board.Size() << "\n";
    for (int row = 0; row < board.Size(); row++) {
      for (int col = 0; col < board.Size(); col++) {
        if (col > 0) ss << " ";
        go::Stone s = board.At(row * board.Size() + col);
        if (s == go::Stone::kBlack)
          ss << "B";
        else if (s == go::Stone::kWhite)
          ss << "W";
        else
          ss << ".";
      }
      ss << "\n";
    }
    ss << "7.5\n";
    return ss.str();
  }

  // Run tromp_score and parse output "B=<n> W=<n>".
  static bool RunTrompScorer(const std::string& input, double& b_score,
                             double& w_score) {
    std::string cmd = scorer_path_ + " <<'BOARD_EOF'\n" + input + "BOARD_EOF";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return false;
    char buf[256];
    std::string output;
    while (fgets(buf, sizeof(buf), fp)) output += buf;
    int status = pclose(fp);
    if (status != 0) return false;

    // Parse "B=81.0 W=7.5" or "B=81 W=7.5".
    if (sscanf(output.c_str(), "B=%lf W=%lf", &b_score, &w_score) != 2) {
      return false;
    }
    return true;
  }

  void CompareScoring(const go::Board& board) {
    ASSERT_FALSE(scorer_path_.empty()) << "Tromp scorer not available";

    // Internal scoring.
    go::ScoreResult ours = go::CalculateScore(board);

    // Tromp-Taylor reference scoring.
    std::string input = BoardToInput(board);
    double tromp_b = 0, tromp_w = 0;
    ASSERT_TRUE(RunTrompScorer(input, tromp_b, tromp_w))
        << "tromp_score failed to run";

    fprintf(stderr,
            "  Internal: B=%.1f W=%.1f  |  Tromp: B=%.1f W=%.1f\n",
            ours.black_score, ours.white_score, tromp_b, tromp_w);

    EXPECT_DOUBLE_EQ(ours.black_score, tromp_b)
        << "Black score mismatch";
    EXPECT_DOUBLE_EQ(ours.white_score, tromp_w)
        << "White score mismatch";
  }
};

std::string ScoringComparison::scorer_path_;

// --- Test cases ---

TEST_F(ScoringComparison, AllBlack) {
  go::Board b(9,
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B"
      "B B B B B B B B B");
  CompareScoring(b);
}

TEST_F(ScoringComparison, AllWhite) {
  go::Board b(9,
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W"
      "W W W W W W W W W");
  CompareScoring(b);
}

TEST_F(ScoringComparison, BlackDominates) {
  go::Board b(9,
      "B . . . . . . . B"
      "B . . . . . . . B"
      "B . . . . . . . B"
      "B . . . . . . . B"
      "B . . . . . . . B"
      "B B B B B B B B B"
      "W W W W W W W W W"
      "W . . . . . . . W"
      "W W W W W W W W W");
  CompareScoring(b);
}

TEST_F(ScoringComparison, WhiteDominates) {
  go::Board b(9,
      "B B B W . . . . W"
      "B . B W . . . . W"
      "B B B W . . . . W"
      "W W W W . . . . W"
      "W . . . . . . . W"
      "W . . . . . . . W"
      "W . . . . . . . W"
      "W . . . . . . . W"
      "W W W W W W W W W");
  CompareScoring(b);
}

TEST_F(ScoringComparison, CloseGame) {
  go::Board b(9,
      "B B B B B W W W W"
      "B . . . B W . . W"
      "B . . . B W . . W"
      "B . . . B W . . W"
      "B . . . B W . . W"
      "B . . . B W . . W"
      "B . . . B W . . W"
      "B . . . B W . . W"
      "B B B B B W W W W");
  CompareScoring(b);
}

TEST_F(ScoringComparison, MultipleRegions) {
  go::Board b(9,
      "B B B B B B B B B"
      "B . . B W . . W B"
      "B . . B W . . W B"
      "B B B B W W W W B"
      "B B B B B B B B B"
      "W W W W B W W W W"
      "W . . W B W . . W"
      "W . . W B W . . W"
      "W W W W B W W W W");
  CompareScoring(b);
}

TEST_F(ScoringComparison, EmptyBoard) {
  go::Board b(9);
  CompareScoring(b);
}

TEST_F(ScoringComparison, NeutralTerritory) {
  // Empty region borders both colors — not counted for either.
  go::Board b(9,
      "B . . . . . . . W"
      ". . . . . . . . ."
      ". . . . . . . . ."
      ". . . . . . . . ."
      ". . . . . . . . ."
      ". . . . . . . . ."
      ". . . . . . . . ."
      ". . . . . . . . ."
      "B . . . . . . . W");
  CompareScoring(b);
}
