#pragma once

#include <string>
#include <vector>

#include "chat/chat_overlay.hpp"
#include "go/game.hpp"
#include "engine/katago_engine.hpp"
#include "vision/vision.hpp"

// Default komi by board size: 9x9=5.5, 13x13=6.5, 19x19=7.5.
inline double DefaultKomi(int board_size) {
  if (board_size <= 9) return 5.5;
  if (board_size <= 13) return 6.5;
  return 7.5;
}

struct GameSettings {
  go::Stone human_color = go::Stone::kBlack;
  std::string human_sl_profile = "preaz_20k";
  int board_size = 9;
  double komi = DefaultKomi(9);
  bool setup_mode = false;
  bool cancelled = false;
  std::string load_save_path;  // If non-empty, load this save on game start.
};

// Per-move analysis history for chat context and save/load.
struct MoveRecord {
  int move_number;           // 1-indexed
  go::Stone color;           // Who played
  std::string gtp_move;      // "D4", "pass", etc.
  double winrate_before;     // Black winrate before this move
  double winrate_after;      // Black winrate after this move
  double score_before;       // Score lead (Black perspective) before
  double score_after;        // Score lead (Black perspective) after
  std::string top3_moves;    // Top 3 KataGo suggestions, e.g. "D4 E5 C3"
  float quality;             // -1 (blunder) to +1 (great)
  bool analysis_complete;    // Whether post-move analysis arrived
  int analysis_turn;         // KataGo history length after this move
};

// Snapshot of full game state for undo/redo and save/load.
struct GameSnapshot {
  go::Game game;
  std::vector<katago::HistoryMove> move_history;
  std::vector<MoveRecord> move_log;
  double winrate;
  double score_lead;
  int black_pos;
  int white_pos;
  float b_quality;
  float w_quality;
  bool b_quality_pending;
  bool w_quality_pending;
  bool has_black;
  bool has_white;
  std::string top3;
};

// Result of a single frame of the settings screen.
enum class SettingsAction { kNone, kStart, kCancel };

// Draws one frame of the settings screen. Caller owns the loop.
// selected_preset is persistent state across frames.
// If vision_board_size > 0, board size is locked to that value.
SettingsAction DrawGameSettings(GameSettings& settings, int& selected_preset,
                                bool chat_consumed, int vision_board_size = 0);

void RunGame(ChatOverlay& chat, katago::Engine* katago,
             touchstone::VisionSystem* vision, const GameSettings& settings);
