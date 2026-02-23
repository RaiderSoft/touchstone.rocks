#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "go/board.hpp"
#include "go/game.hpp"

namespace katago {

// A single candidate move from KataGo analysis.
struct MoveInfo {
  std::string gtp_move;  // e.g. "E5", "pass"
  int pos;               // internal position (0-80), -1 for pass
  int visits;
  double winrate;     // 0.0 to 1.0 from current player's perspective
  double score_lead;  // positive = current player leading
  int order;          // 0 = best move
};

// Full analysis result for one position.
struct AnalysisResult {
  std::string query_id;
  int turn_number = 0;

  // Top moves, sorted by order (0 = best).
  std::vector<MoveInfo> moves;

  // Root-level stats (whole-position evaluation).
  double winrate = 0.5;
  double score_lead = 0.0;
  int visits = 0;

  // Per-intersection ownership: -1.0 (white) to +1.0 (black).
  // Index i = row * board_size + col, row 0 = top.
  std::vector<double> ownership;

  // Human SL policy: length boardYSize*boardXSize+1, last entry is pass.
  // Positive values sum to 1, -1 means illegal.
  std::vector<double> human_policy;

  bool valid = false;
};

// One move in the game history for KataGo query format.
struct HistoryMove {
  go::Stone color;  // kBlack or kWhite
  int pos;          // 0-80 for a play, -1 for pass
};

// Configuration for the KataGo subprocess.
struct Config {
  std::string katago_path;        // e.g. "/opt/homebrew/bin/katago"
  std::string model_path;         // e.g. "kata1-b18c384nbt.bin.gz"
  std::string human_model_path;   // e.g. "b18c384nbt-humanv0.bin.gz"
  std::string config_path;        // e.g. "analysis_example.cfg"
  int board_size = 9;
  double komi = 7.5;
  std::string rules = "chinese";
  int default_max_visits = 200;
};

class Engine {
 public:
  explicit Engine(const Config& config);
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Start the KataGo subprocess. Returns false if katago binary not found
  // or failed to launch.
  bool Start();

  // Gracefully shut down: close stdin, wait for process exit.
  void Stop();

  bool IsRunning() const;

  // Set the active human SL profile for all queries.
  void SetHumanSLProfile(const std::string& profile);

  // Set the board size for all subsequent queries.
  void SetBoardSize(int size);

  // Request analysis of the current game position.
  // move_history is the sequence of all moves played so far.
  // analyze_turn is which turn to analyze (typically move_history.size()).
  // The analysis runs asynchronously; poll with PollAnalysis().
  void RequestAnalysis(const std::vector<HistoryMove>& move_history,
                       int analyze_turn, int max_visits = 0);

  // Request analysis of a static board position (no move history needed).
  // Uses KataGo's initialStones protocol field. Useful for scoring.
  // to_play determines whose turn it is (affects score_lead perspective).
  void RequestAnalysisForPosition(const go::Board& board,
                                  go::Stone to_play,
                                  int max_visits = 0);

  // Request the best move for the computer to play using human SL profile.
  // Same async pattern; poll with PollBestMove().
  void RequestBestMove(const std::vector<HistoryMove>& move_history,
                       int analyze_turn);

  // Non-blocking poll. Returns the latest analysis result if one has
  // arrived since the last call. Returns nullopt otherwise.
  std::optional<AnalysisResult> PollAnalysis();

  // Non-blocking poll for computer move. Returns the best move position
  // (-1 for pass) if analysis completed. Returns nullopt otherwise.
  std::optional<int> PollBestMove();

 private:
  Config config_;

  // Subprocess handles.
  pid_t child_pid_ = -1;
  int stdin_fd_ = -1;   // write end: send queries to katago
  int stdout_fd_ = -1;  // read end: receive responses from katago

  // Reader thread: continuously reads lines from katago stdout.
  std::thread reader_thread_;
  std::atomic<bool> running_{false};

  // Pending results, protected by mutex.
  std::mutex mutex_;
  std::deque<AnalysisResult> pending_analyses_;  // Queue of analysis results.
  std::optional<AnalysisResult> pending_best_move_;

  // Query ID tracking.
  int next_query_id_ = 0;
  std::set<std::string> analysis_ids_;  // All outstanding analysis query IDs.
  std::string last_bestmove_id_;

  // Human SL profile for all queries.
  std::string human_sl_profile_;

  // RNG for sampling from humanPolicy.
  std::mt19937 rng_{std::random_device{}()};

  void ReaderLoop();
  void SendQuery(const std::string& id,
                 const std::vector<HistoryMove>& move_history,
                 int analyze_turn, int max_visits,
                 bool include_ownership);
  AnalysisResult ParseResponse(const std::string& json_line);
  std::string AllocateId(const std::string& prefix);
};

// --- Coordinate conversion utilities ---

// Convert internal position (0-80, row-major, row 0 = top) to GTP string.
// For 9x9: pos 0 (row 0, col 0) = "A9", pos 80 (row 8, col 8) = "J1".
// GTP columns: A B C D E F G H J (skipping I).
// GTP rows: 1 (bottom) to 9 (top).
// Returns "pass" for pos < 0.
std::string PosToGtp(int pos, int board_size);

// Convert GTP string to internal position. Returns -1 for "pass".
int GtpToPos(const std::string& gtp, int board_size);

// Convert stone color to GTP color string ("B" or "W").
std::string StoneToGtp(go::Stone color);

}  // namespace katago
