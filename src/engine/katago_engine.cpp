#include "engine/katago_engine.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>  // steady_clock for probe timeout
#include <cstdio>
#include <cstring>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace katago {

// ---------------------------------------------------------------------------
// Coordinate conversion
// ---------------------------------------------------------------------------

std::string PosToGtp(int pos, int board_size) {
  if (pos < 0) return "pass";
  int row = pos / board_size;
  int col = pos % board_size;
  // GTP columns: A=0, B=1, ..., H=7, J=8 (skip I).
  char col_char = 'A' + col;
  if (col >= 8) col_char++;  // skip 'I'
  // GTP rows: bottom row = 1, top row = board_size.
  int gtp_row = board_size - row;
  return std::string(1, col_char) + std::to_string(gtp_row);
}

int GtpToPos(const std::string& gtp, int board_size) {
  if (gtp == "pass" || gtp.empty()) return -1;
  char col_char = std::toupper(gtp[0]);
  int col = col_char - 'A';
  if (col_char > 'I') col--;  // adjust for skipped 'I'
  int gtp_row = std::stoi(gtp.substr(1));
  int row = board_size - gtp_row;
  return row * board_size + col;
}

std::string StoneToGtp(go::Stone color) {
  return (color == go::Stone::kBlack) ? "B" : "W";
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

Engine::Engine(const Config& config) : config_(config) {}

Engine::~Engine() { Stop(); }

bool Engine::Start() {
  if (running_) return true;

  // Verify katago binary exists and is executable.
  if (access(config_.katago_path.c_str(), X_OK) != 0) {
    fprintf(stderr, "KataGo: binary not found at %s\n",
            config_.katago_path.c_str());
    return false;
  }
  if (config_.model_path.empty()) {
    fprintf(stderr, "KataGo: no model path configured\n");
    return false;
  }

  int stdin_pipe[2], stdout_pipe[2];
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    fprintf(stderr, "KataGo: pipe() failed\n");
    return false;
  }

  child_pid_ = fork();
  if (child_pid_ < 0) {
    fprintf(stderr, "KataGo: fork() failed\n");
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return false;
  }

  if (child_pid_ == 0) {
    // Child process: become katago.
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Let KataGo's stderr pass through so we can see errors.

    // Build argv dynamically so -config is only passed when set.
    std::vector<const char*> argv;
    argv.push_back("katago");
    argv.push_back("analysis");
    if (!config_.config_path.empty()) {
      argv.push_back("-config");
      argv.push_back(config_.config_path.c_str());
    }
    argv.push_back("-model");
    argv.push_back(config_.model_path.c_str());
    argv.push_back("-human-model");
    argv.push_back(config_.human_model_path.c_str());
    argv.push_back(nullptr);
    execvp(config_.katago_path.c_str(),
           const_cast<char* const*>(argv.data()));
    _exit(1);  // exec failed
  }

  // Parent process.
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);
  stdin_fd_ = stdin_pipe[1];
  stdout_fd_ = stdout_pipe[0];

  fprintf(stderr, "KataGo: started (pid %d), verifying inference...\n",
          child_pid_);

  // Probe: send a minimal query and block-read the response before starting
  // the reader thread. Simple synchronous I/O — no async machinery needed.
  json probe;
  probe["id"] = "probe";
  probe["rules"] = config_.rules;
  probe["komi"] = config_.komi;
  probe["boardXSize"] = config_.board_size;
  probe["boardYSize"] = config_.board_size;
  probe["analyzeTurns"] = json::array({0});
  probe["maxVisits"] = 1;
  probe["moves"] = json::array();
  std::string probe_line = probe.dump() + "\n";

  // Write probe to stdin.
  const char* wr = probe_line.c_str();
  size_t rem = probe_line.size();
  while (rem > 0) {
    ssize_t n = write(stdin_fd_, wr, rem);
    if (n <= 0) {
      fprintf(stderr, "KataGo: failed to write probe query\n");
      close(stdin_fd_); stdin_fd_ = -1;
      close(stdout_fd_); stdout_fd_ = -1;
      kill(child_pid_, SIGTERM);
      waitpid(child_pid_, nullptr, 0);
      child_pid_ = -1;
      return false;
    }
    wr += n;
    rem -= n;
  }

  // Block-read response with 120s timeout (OpenCL autotuning can take minutes).
  std::string buf;
  char chunk[4096];
  auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(120);
  bool probe_ok = false;

  while (std::chrono::steady_clock::now() < deadline) {
    struct pollfd pfd = {stdout_fd_, POLLIN, 0};
    int ready = poll(&pfd, 1, 500);  // 500ms chunks
    if (ready < 0) break;
    if (ready == 0) continue;

    ssize_t n = read(stdout_fd_, chunk, sizeof(chunk) - 1);
    if (n <= 0) {
      fprintf(stderr, "KataGo: process died during startup\n");
      break;
    }
    chunk[n] = '\0';
    buf += chunk;

    // Process all complete lines in the buffer.
    size_t nl;
    while ((nl = buf.find('\n')) != std::string::npos) {
      std::string line = buf.substr(0, nl);
      buf.erase(0, nl + 1);
      if (line.empty()) continue;
      AnalysisResult result = ParseResponse(line);
      if (result.valid) {
        probe_ok = true;
        break;
      }
    }
    if (probe_ok) break;
  }

  if (!probe_ok) {
    fprintf(stderr, "KataGo: startup probe failed — cannot run inference.\n");
    close(stdin_fd_); stdin_fd_ = -1;
    close(stdout_fd_); stdout_fd_ = -1;
    kill(child_pid_, SIGTERM);
    waitpid(child_pid_, nullptr, 0);
    child_pid_ = -1;
    return false;
  }

  fprintf(stderr, "KataGo: inference verified OK\n");
  running_ = true;
  reader_thread_ = std::thread(&Engine::ReaderLoop, this);
  return true;
}

void Engine::Stop() {
  if (!running_) return;
  running_ = false;

  // Close stdin to signal KataGo to exit.
  if (stdin_fd_ >= 0) {
    close(stdin_fd_);
    stdin_fd_ = -1;
  }

  // Wait for reader thread.
  if (reader_thread_.joinable()) reader_thread_.join();

  // Close stdout.
  if (stdout_fd_ >= 0) {
    close(stdout_fd_);
    stdout_fd_ = -1;
  }

  // Wait for child process (with timeout via SIGTERM).
  if (child_pid_ > 0) {
    int status;
    pid_t result = waitpid(child_pid_, &status, WNOHANG);
    if (result == 0) {
      // Still running, send SIGTERM and wait.
      kill(child_pid_, SIGTERM);
      waitpid(child_pid_, &status, 0);
    }
    child_pid_ = -1;
  }
}

bool Engine::IsRunning() const { return running_; }

// ---------------------------------------------------------------------------
// Async query/response
// ---------------------------------------------------------------------------

void Engine::SetHumanSLProfile(const std::string& profile) {
  human_sl_profile_ = profile;
}

void Engine::SetBoardSize(int size) {
  config_.board_size = size;
}

std::string Engine::AllocateId(const std::string& prefix) {
  return prefix + "_" + std::to_string(next_query_id_++);
}

void Engine::RequestAnalysis(const std::vector<HistoryMove>& move_history,
                             int analyze_turn, int max_visits) {
  if (!running_) return;
  std::string id = AllocateId("a");
  {
    std::lock_guard<std::mutex> lock(mutex_);
    analysis_ids_.insert(id);
  }
  SendQuery(id, move_history, analyze_turn, max_visits, true);
}

void Engine::RequestAnalysisForPosition(const go::Board& board,
                                        go::Stone to_play,
                                        int max_visits) {
  if (!running_) return;
  std::string id = AllocateId("a");
  {
    std::lock_guard<std::mutex> lock(mutex_);
    analysis_ids_.insert(id);
  }

  // Build initial stones list from the board.
  std::vector<std::pair<go::Stone, int>> initial_stones;
  for (int i = 0; i < board.NumPositions(); i++) {
    if (board.At(i) != go::Stone::kEmpty) {
      initial_stones.push_back({board.At(i), i});
    }
  }

  // Build the query JSON directly (no move history, uses initialStones).
  json query;
  query["id"] = id;
  query["rules"] = config_.rules;
  query["komi"] = config_.komi;
  query["boardXSize"] = config_.board_size;
  query["boardYSize"] = config_.board_size;
  query["analyzeTurns"] = json::array({0});

  int visits = (max_visits > 0) ? max_visits : config_.default_max_visits;
  query["maxVisits"] = visits;
  query["includeOwnership"] = true;
  query["includePolicy"] = true;

  // Initial player determines whose perspective score_lead reports from.
  query["initialPlayer"] = StoneToGtp(to_play);

  // Set up stones via initialStones (no move history needed).
  json stones = json::array();
  for (const auto& [color, pos] : initial_stones) {
    stones.push_back({StoneToGtp(color), PosToGtp(pos, config_.board_size)});
  }
  query["initialStones"] = stones;
  query["moves"] = json::array();

  std::string line = query.dump() + "\n";
  const char* data = line.c_str();
  size_t remaining = line.size();
  while (remaining > 0 && running_) {
    ssize_t written = write(stdin_fd_, data, remaining);
    if (written <= 0) {
      fprintf(stderr, "KataGo: write to stdin failed\n");
      running_ = false;
      break;
    }
    data += written;
    remaining -= written;
  }
}

void Engine::RequestBestMove(const std::vector<HistoryMove>& move_history,
                             int analyze_turn) {
  if (!running_) return;
  std::string id = AllocateId("m");
  {
    std::lock_guard<std::mutex> lock(mutex_);
    last_bestmove_id_ = id;
    pending_best_move_.reset();
  }
  // 40 visits: enough for pass/resign judgment per KataGo human config docs.
  SendQuery(id, move_history, analyze_turn, 40, false);
}

std::optional<AnalysisResult> Engine::PollAnalysis() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (pending_analyses_.empty()) return std::nullopt;
  auto result = std::move(pending_analyses_.front());
  pending_analyses_.pop_front();
  return result;
}

std::optional<int> Engine::PollBestMove() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!pending_best_move_.has_value()) return std::nullopt;
  auto& result = pending_best_move_.value();

  // If KataGo's top move is pass, pass.
  if (!result.moves.empty() && result.moves[0].pos < 0) {
    pending_best_move_.reset();
    return -1;
  }

  // Sample from humanPolicy, excluding pass (last entry).
  int board_positions = config_.board_size * config_.board_size;
  if (static_cast<int>(result.human_policy.size()) >= board_positions) {
    // Build weights for board positions only (exclude pass = last entry).
    std::vector<double> weights(board_positions);
    for (int i = 0; i < board_positions; i++) {
      weights[i] = (result.human_policy[i] > 0) ? result.human_policy[i] : 0;
    }
    std::discrete_distribution<int> dist(weights.begin(), weights.end());
    int pos = dist(rng_);
    pending_best_move_.reset();
    return pos;
  }

  // Fallback: use KataGo's top move.
  int best_pos = -1;
  if (!result.moves.empty()) {
    best_pos = result.moves[0].pos;
  }
  pending_best_move_.reset();
  return best_pos;
}

// ---------------------------------------------------------------------------
// Query serialization
// ---------------------------------------------------------------------------

void Engine::SendQuery(const std::string& id,
                       const std::vector<HistoryMove>& move_history,
                       int analyze_turn, int max_visits,
                       bool include_ownership) {
  json query;
  query["id"] = id;
  query["rules"] = config_.rules;
  query["komi"] = config_.komi;
  query["boardXSize"] = config_.board_size;
  query["boardYSize"] = config_.board_size;
  query["analyzeTurns"] = json::array({analyze_turn});

  int visits = (max_visits > 0) ? max_visits : config_.default_max_visits;
  query["maxVisits"] = visits;

  if (include_ownership) {
    query["includeOwnership"] = true;
  }

  // Always request policy so we get humanPolicy.
  query["includePolicy"] = true;

  // Human SL overrideSettings for human-like play.
  json overrides;
  overrides["humanSLProfile"] = human_sl_profile_;
  overrides["ignorePreRootHistory"] = false;
  overrides["rootNumSymmetriesToSample"] = 2;
  query["overrideSettings"] = overrides;

  // Build moves array: [["B","D4"],["W","F6"],...].
  json moves = json::array();
  for (const auto& m : move_history) {
    std::string gtp_color = StoneToGtp(m.color);
    std::string gtp_pos =
        (m.pos < 0) ? "pass" : PosToGtp(m.pos, config_.board_size);
    moves.push_back({gtp_color, gtp_pos});
  }
  query["moves"] = moves;

  std::string line = query.dump() + "\n";

  // Write to KataGo's stdin. Use a loop in case of partial writes.
  const char* data = line.c_str();
  size_t remaining = line.size();
  while (remaining > 0 && running_) {
    ssize_t written = write(stdin_fd_, data, remaining);
    if (written <= 0) {
      fprintf(stderr, "KataGo: write to stdin failed\n");
      running_ = false;
      break;
    }
    data += written;
    remaining -= written;
  }
}

// ---------------------------------------------------------------------------
// Response parsing
// ---------------------------------------------------------------------------

AnalysisResult Engine::ParseResponse(const std::string& json_line) {
  AnalysisResult result;
  try {
    json j = json::parse(json_line);

    // Check for error responses.
    if (j.contains("error")) {
      fprintf(stderr, "KataGo error: %s\n",
              j["error"].get<std::string>().c_str());
      return result;
    }

    result.query_id = j.value("id", "");
    result.turn_number = j.value("turnNumber", 0);

    // Parse moveInfos.
    if (j.contains("moveInfos")) {
      for (const auto& mi : j["moveInfos"]) {
        MoveInfo info;
        info.gtp_move = mi.value("move", "pass");
        info.pos = GtpToPos(info.gtp_move, config_.board_size);
        info.visits = mi.value("visits", 0);
        info.winrate = mi.value("winrate", 0.5);
        info.score_lead = mi.value("scoreLead", 0.0);
        info.order = mi.value("order", 0);
        result.moves.push_back(info);
      }
      // Sort by order (should already be sorted, but be safe).
      std::sort(result.moves.begin(), result.moves.end(),
                [](const MoveInfo& a, const MoveInfo& b) {
                  return a.order < b.order;
                });
    }

    // Parse rootInfo.
    if (j.contains("rootInfo")) {
      const auto& ri = j["rootInfo"];
      result.winrate = ri.value("winrate", 0.5);
      result.score_lead = ri.value("scoreLead", 0.0);
      result.visits = ri.value("visits", 0);
    }

    // Parse ownership array.
    if (j.contains("ownership")) {
      for (const auto& v : j["ownership"]) {
        result.ownership.push_back(v.get<double>());
      }
    }

    // Parse humanPolicy array (available when -human-model + humanSLProfile).
    if (j.contains("humanPolicy")) {
      for (const auto& v : j["humanPolicy"]) {
        result.human_policy.push_back(v.get<double>());
      }
    }

    result.valid = true;
  } catch (const json::exception& e) {
    fprintf(stderr, "KataGo: JSON parse error: %s\n", e.what());
  }
  return result;
}

// ---------------------------------------------------------------------------
// Reader thread
// ---------------------------------------------------------------------------

void Engine::ReaderLoop() {
  std::string buffer;
  char chunk[4096];

  while (running_) {
    ssize_t n = read(stdout_fd_, chunk, sizeof(chunk) - 1);
    if (n <= 0) break;  // EOF or error
    chunk[n] = '\0';
    buffer += chunk;

    // Process complete lines.
    size_t pos;
    while ((pos = buffer.find('\n')) != std::string::npos) {
      std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 1);

      if (line.empty()) continue;

      AnalysisResult result = ParseResponse(line);
      if (!result.valid) continue;

      std::lock_guard<std::mutex> lock(mutex_);
      if (analysis_ids_.count(result.query_id)) {
        analysis_ids_.erase(result.query_id);
        pending_analyses_.push_back(std::move(result));
      } else if (result.query_id == last_bestmove_id_) {
        pending_best_move_ = std::move(result);
      }
      // Responses for old/superseded queries are silently dropped.
    }
  }
}

}  // namespace katago
