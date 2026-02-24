#include "ui/game_context.hpp"

#include <algorithm>
#include <cstdio>

std::string BuildGameContext(
    const go::Game& game, int gp_val, go::Stone human_color,
    const go::Board& setup_board, bool setup_mode,
    const katago::AnalysisResult& last_analysis, bool vision_active,
    int board_size, const std::vector<MoveRecord>& move_log) {
  // gp_val: 0=setup, 1=chooseColor, 2=playerTurn, 3=computerTurn,
  //         4=computerAnnounce, 5=placeComputer, 6=scoring
  if (gp_val <= 1) {
    return "Setting up a board position before playing.\nBoard:\n" +
           setup_board.ToString() + "\n";
  }
  const char* opponent_str =
      (human_color == go::Stone::kBlack) ? "Black" : "White";
  const char* your_str =
      (human_color == go::Stone::kBlack) ? "White" : "Black";

  std::string ctx;
  ctx += std::to_string(board_size) + "x" +
         std::to_string(board_size) + " Go game.\n";
  ctx += std::string("Your opponent plays ") + opponent_str +
         ", you play " + your_str + ".\n";
  ctx += "Move: " + std::to_string(game.MoveNumber() + 1) + "\n";
  ctx += "Captures: B=" +
         std::to_string(game.CapturedBy(go::Stone::kBlack)) + " W=" +
         std::to_string(game.CapturedBy(go::Stone::kWhite)) + "\n";

  switch (gp_val) {
    case 2:
      ctx += std::string("Your opponent (") + opponent_str +
             ") is thinking about their move.\n";
      break;
    case 3:
      ctx += "You're deciding on your next move.\n";
      break;
    case 4:
      ctx += "You're announcing your next move.\n";
      break;
    case 5:
      ctx += "You just played; board is updating.\n";
      break;
    case 6:
      ctx += "Game is over, scoring.\n";
      break;
    default:
      break;
  }

  // Current position analysis (presented as your read of the board).
  if (last_analysis.valid) {
    ctx += "\n--- Your read of the position ---\n";
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Black winrate=%.0f%% Score=%+.1f\n",
             last_analysis.winrate * 100.0, last_analysis.score_lead);
    ctx += buf;

    ctx += "Top moves: ";
    int n = std::min(3, static_cast<int>(last_analysis.moves.size()));
    for (int i = 0; i < n; i++) {
      const auto& m = last_analysis.moves[i];
      snprintf(buf, sizeof(buf), "%s(%.0f%%) ", m.gtp_move.c_str(),
               m.winrate * 100.0);
      ctx += buf;
    }
    ctx += "\n";
  }

  // Full move history with analysis.
  if (!move_log.empty()) {
    ctx += "\n--- Move History ---\n";
    ctx += "# | Player | Move | WR Before | WR After | Delta | Best Was | Quality\n";
    for (const auto& rec : move_log) {
      const char* who = (rec.color == go::Stone::kBlack) ? "B" : "W";
      bool is_human = (rec.color == human_color);
      char buf[256];
      if (rec.analysis_complete) {
        // Compute delta from mover's perspective.
        double delta;
        if (rec.color == go::Stone::kBlack) {
          delta = rec.winrate_after - rec.winrate_before;
        } else {
          delta = rec.winrate_before - rec.winrate_after;
        }
        const char* grade;
        if (rec.quality > 0.5f) grade = "good";
        else if (rec.quality > -0.2f) grade = "ok";
        else if (rec.quality > -0.6f) grade = "inaccuracy";
        else grade = "blunder";

        snprintf(buf, sizeof(buf),
                 "%d | %s%s | %s | %.0f%% | %.0f%% | %+.1f%% | %s | %s\n",
                 rec.move_number, who, is_human ? "(opponent)" : "(you)",
                 rec.gtp_move.c_str(),
                 rec.winrate_before * 100.0, rec.winrate_after * 100.0,
                 delta * 100.0, rec.top3_moves.c_str(), grade);
      } else {
        snprintf(buf, sizeof(buf),
                 "%d | %s%s | %s | %.0f%% | ... | ... | %s | ...\n",
                 rec.move_number, who, is_human ? "(opponent)" : "(you)",
                 rec.gtp_move.c_str(),
                 rec.winrate_before * 100.0, rec.top3_moves.c_str());
      }
      ctx += buf;
    }
  }

  ctx += "\nBoard:\n" + game.GetBoard().ToString() + "\n";
  return ctx;
}
