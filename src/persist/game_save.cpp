#include "persist/game_save.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// JSON serialization for katago::HistoryMove
// ---------------------------------------------------------------------------

static void to_json(json& j, const katago::HistoryMove& m) {
  j = json{{"color", static_cast<int>(m.color)}, {"pos", m.pos}};
}

static void from_json(const json& j, katago::HistoryMove& m) {
  m.color = static_cast<go::Stone>(j.at("color").get<int>());
  m.pos = j.at("pos").get<int>();
}

// ---------------------------------------------------------------------------
// JSON serialization for MoveRecord
// ---------------------------------------------------------------------------

static void to_json(json& j, const MoveRecord& r) {
  j = json{
      {"move_number", r.move_number},
      {"color", static_cast<int>(r.color)},
      {"gtp_move", r.gtp_move},
      {"winrate_before", r.winrate_before},
      {"winrate_after", r.winrate_after},
      {"score_before", r.score_before},
      {"score_after", r.score_after},
      {"top3_moves", r.top3_moves},
      {"quality", r.quality},
      {"analysis_complete", r.analysis_complete},
      {"analysis_turn", r.analysis_turn},
  };
}

static void from_json(const json& j, MoveRecord& r) {
  r.move_number = j.at("move_number").get<int>();
  r.color = static_cast<go::Stone>(j.at("color").get<int>());
  r.gtp_move = j.at("gtp_move").get<std::string>();
  r.winrate_before = j.at("winrate_before").get<double>();
  r.winrate_after = j.at("winrate_after").get<double>();
  r.score_before = j.at("score_before").get<double>();
  r.score_after = j.at("score_after").get<double>();
  r.top3_moves = j.at("top3_moves").get<std::string>();
  r.quality = j.at("quality").get<float>();
  r.analysis_complete = j.at("analysis_complete").get<bool>();
  r.analysis_turn = j.at("analysis_turn").get<int>();
}

// ---------------------------------------------------------------------------
// Directory utilities
// ---------------------------------------------------------------------------

// Create directory recursively (like mkdir -p).
static bool MakeDirRecursive(const std::string& path) {
  size_t pos = 0;
  while (pos != std::string::npos) {
    pos = path.find('/', pos + 1);
    std::string sub = path.substr(0, pos);
    if (sub.empty()) continue;
    struct stat st;
    if (stat(sub.c_str(), &st) != 0) {
      if (mkdir(sub.c_str(), 0755) != 0) return false;
    }
  }
  return true;
}

namespace touchstone {

std::string GetSaveDirectory() {
  const char* home = std::getenv("HOME");
  if (!home) home = "/tmp";
  std::string dir = std::string(home) + "/.touchstone/saves";
  MakeDirRecursive(dir);
  return dir;
}

// ---------------------------------------------------------------------------
// SaveGame
// ---------------------------------------------------------------------------

std::string SaveGame(const std::string& path, const SaveData& data) {
  json j;
  j["version"] = data.version;
  j["name"] = data.name;
  j["timestamp"] = data.timestamp;
  j["board_size"] = data.board_size;
  j["human_color"] = data.human_color;
  j["human_sl_profile"] = data.human_sl_profile;
  j["komi"] = data.komi;

  // Move history.
  json hist = json::array();
  for (const auto& m : data.move_history) {
    json mj;
    to_json(mj, m);
    hist.push_back(mj);
  }
  j["move_history"] = hist;

  // Move log.
  json log = json::array();
  for (const auto& r : data.move_log) {
    json rj;
    to_json(rj, r);
    log.push_back(rj);
  }
  j["move_log"] = log;

  // Chat history.
  json chat = json::array();
  for (const auto& msg : data.chat_messages) {
    chat.push_back({{"role", msg.role}, {"content", msg.content}});
  }
  j["chat_messages"] = chat;

  // UI state.
  j["state"] = {
      {"prev_winrate", data.prev_winrate},
      {"prev_score_lead", data.prev_score_lead},
      {"last_black_pos", data.last_black_pos},
      {"last_white_pos", data.last_white_pos},
      {"black_quality", data.black_quality},
      {"white_quality", data.white_quality},
      {"black_quality_pending", data.black_quality_pending},
      {"white_quality_pending", data.white_quality_pending},
      {"has_black_move", data.has_black_move},
      {"has_white_move", data.has_white_move},
      {"pre_move_top3", data.pre_move_top3},
  };

  std::ofstream ofs(path);
  if (!ofs.is_open()) {
    return "Failed to open file for writing: " + path;
  }
  ofs << j.dump(2) << std::endl;
  if (ofs.fail()) {
    return "Failed to write save file: " + path;
  }
  return "";
}

// ---------------------------------------------------------------------------
// LoadGame
// ---------------------------------------------------------------------------

std::string LoadGame(const std::string& path, SaveData& data) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return "Save file not found: " + path;
  }

  json j;
  try {
    ifs >> j;
  } catch (const json::parse_error& e) {
    return std::string("Failed to parse save file: ") + e.what();
  }

  // Version check.
  if (!j.contains("version") || j["version"].get<int>() != 1) {
    return "Unsupported save file version.";
  }

  // Core fields.
  data.version = j["version"].get<int>();
  data.name = j.value("name", "");
  data.timestamp = j.value("timestamp", "");
  data.board_size = j.at("board_size").get<int>();
  data.human_color = j.at("human_color").get<int>();
  data.human_sl_profile = j.value("human_sl_profile", "preaz_20k");
  data.komi = j.value("komi", DefaultKomi(data.board_size));

  if (data.board_size != 9 && data.board_size != 13 && data.board_size != 19) {
    return "Invalid board size in save file: " +
           std::to_string(data.board_size);
  }
  if (data.human_color != 1 && data.human_color != 2) {
    return "Invalid human color in save file: " +
           std::to_string(data.human_color);
  }

  // Move history.
  data.move_history.clear();
  if (j.contains("move_history")) {
    for (const auto& mj : j["move_history"]) {
      katago::HistoryMove m;
      from_json(mj, m);
      data.move_history.push_back(m);
    }
  }

  // Move log.
  data.move_log.clear();
  if (j.contains("move_log")) {
    for (const auto& rj : j["move_log"]) {
      MoveRecord r;
      from_json(rj, r);
      data.move_log.push_back(r);
    }
  }

  // Chat history.
  data.chat_messages.clear();
  if (j.contains("chat_messages")) {
    for (const auto& cj : j["chat_messages"]) {
      ChatMessage msg;
      msg.role = cj.at("role").get<std::string>();
      msg.content = cj.at("content").get<std::string>();
      msg.timestamp = 0;
      data.chat_messages.push_back(msg);
    }
  }

  // UI state.
  if (j.contains("state")) {
    const auto& s = j["state"];
    data.prev_winrate = s.value("prev_winrate", 0.5);
    data.prev_score_lead = s.value("prev_score_lead", 0.0);
    data.last_black_pos = s.value("last_black_pos", -1);
    data.last_white_pos = s.value("last_white_pos", -1);
    data.black_quality = s.value("black_quality", 0.0f);
    data.white_quality = s.value("white_quality", 0.0f);
    data.black_quality_pending = s.value("black_quality_pending", true);
    data.white_quality_pending = s.value("white_quality_pending", true);
    data.has_black_move = s.value("has_black_move", false);
    data.has_white_move = s.value("has_white_move", false);
    data.pre_move_top3 = s.value("pre_move_top3", std::string(""));
  }

  return "";
}

// ---------------------------------------------------------------------------
// ListSaves
// ---------------------------------------------------------------------------

std::vector<SaveInfo> ListSaves() {
  std::vector<SaveInfo> saves;
  std::string dir = GetSaveDirectory();

  DIR* dp = opendir(dir.c_str());
  if (!dp) return saves;

  struct dirent* entry;
  while ((entry = readdir(dp)) != nullptr) {
    std::string name = entry->d_name;
    if (name.size() < 5 || name.substr(name.size() - 5) != ".json") continue;

    std::string filepath = dir + "/" + name;

    // Try to read metadata from the file.
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) continue;

    json j;
    try {
      ifs >> j;
    } catch (...) {
      continue;
    }

    SaveInfo info;
    info.filename = name;
    info.filepath = filepath;
    info.display_name = j.value("name", name.substr(0, name.size() - 5));
    info.timestamp = j.value("timestamp", "");
    info.board_size = j.value("board_size", 0);
    if (j.contains("move_history")) {
      info.move_count = static_cast<int>(j["move_history"].size());
    } else {
      info.move_count = 0;
    }
    saves.push_back(info);
  }
  closedir(dp);

  // Sort by timestamp descending (most recent first).
  std::sort(saves.begin(), saves.end(),
            [](const SaveInfo& a, const SaveInfo& b) {
              return a.timestamp > b.timestamp;
            });

  return saves;
}

// ---------------------------------------------------------------------------
// DeleteSave
// ---------------------------------------------------------------------------

std::string DeleteSave(const std::string& path) {
  if (std::remove(path.c_str()) != 0) {
    return "Failed to delete save file: " + path;
  }
  return "";
}

}  // namespace touchstone
