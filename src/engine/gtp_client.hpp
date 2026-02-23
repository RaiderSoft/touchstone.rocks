#pragma once

#include <string>
#include <vector>

#include "go/board.hpp"

namespace gtp {

// Synchronous GTP (Go Text Protocol) client.
// Spawns a GTP engine subprocess and sends commands over stdin/stdout.
// Works with any GTP-compliant engine (GNU Go, KataGo --gtp, etc.).
class Client {
 public:
  explicit Client(const std::string& engine_path);
  ~Client();

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Start the engine subprocess with optional extra arguments.
  bool Start(const std::vector<std::string>& args = {});

  // Shut down the engine.
  void Stop();

  bool IsRunning() const;

  // Send a raw GTP command and block until the response arrives.
  // Returns the response text (without the "= " prefix).
  // On error, returns "" and sets ok to false.
  std::string SendCommand(const std::string& command, bool* ok = nullptr);

  // --- Convenience methods ---

  bool SetBoardSize(int size);
  bool ClearBoard();
  bool SetKomi(double komi);

  // Play a stone at a GTP vertex (e.g. "D4", "pass").
  bool Play(go::Stone color, const std::string& vertex);

  // Play a stone at an internal position index.
  bool Play(go::Stone color, int pos, int board_size);

  // Get the final score string (e.g. "B+12.5", "W+0.5", "0").
  std::string FinalScore();

  // Set up a full board position by playing all stones.
  bool SetupPosition(const go::Board& board);

 private:
  std::string engine_path_;
  pid_t child_pid_ = -1;
  int stdin_fd_ = -1;
  int stdout_fd_ = -1;
  bool running_ = false;

  // Read one GTP response (everything up to the double-newline).
  std::string ReadResponse();
};

// Parse a GTP final_score string like "B+12.5" or "W+0.5" into a
// score lead from Black's perspective (+ve = Black leads).
// Returns 0.0 on parse failure or "0" (jigo).
double ParseScoreLead(const std::string& score_str);

}  // namespace gtp
