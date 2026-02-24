#pragma once

#include <string>
#include <vector>

#include "ui/game_mode.hpp"
#include "engine/katago_engine.hpp"

namespace touchstone {

// Metadata for a save file entry in the save list.
struct SaveInfo {
  std::string filename;       // e.g. "My Game.json"
  std::string filepath;       // Full path to the file.
  std::string display_name;   // User-chosen name.
  std::string timestamp;      // ISO 8601 when saved.
  int board_size;
  int move_count;
};

// All data needed to save/restore a game.
struct SaveData {
  int version = 1;
  std::string name;                  // User-chosen display name.
  std::string timestamp;             // ISO 8601 when saved.
  int board_size;
  int human_color;                   // 1=Black, 2=White
  std::string human_sl_profile;
  double komi;

  std::vector<katago::HistoryMove> move_history;
  std::vector<MoveRecord> move_log;

  // UI state.
  double prev_winrate;
  double prev_score_lead;
  int last_black_pos;
  int last_white_pos;
  float black_quality;
  float white_quality;
  bool black_quality_pending;
  bool white_quality_pending;
  bool has_black_move;
  bool has_white_move;
  std::string pre_move_top3;
};

// Returns "" on success, error message on failure.
std::string SaveGame(const std::string& path, const SaveData& data);

// Returns "" on success, error message on failure.
std::string LoadGame(const std::string& path, SaveData& data);

// Get the default save directory (~/.touchstone/saves/), creating it if needed.
std::string GetSaveDirectory();

// List all saves in the save directory, sorted by most recent first.
std::vector<SaveInfo> ListSaves();

// Delete a save file. Returns "" on success, error message on failure.
std::string DeleteSave(const std::string& path);

}  // namespace touchstone
