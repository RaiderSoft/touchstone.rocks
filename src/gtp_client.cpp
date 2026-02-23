#include "gtp_client.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "katago_engine.h"  // PosToGtp, StoneToGtp

namespace gtp {

Client::Client(const std::string& engine_path) : engine_path_(engine_path) {}

Client::~Client() { Stop(); }

bool Client::Start(const std::vector<std::string>& args) {
  if (running_) return true;

  int stdin_pipe[2], stdout_pipe[2];
  if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
    fprintf(stderr, "GTP: pipe() failed\n");
    return false;
  }

  child_pid_ = fork();
  if (child_pid_ < 0) {
    fprintf(stderr, "GTP: fork() failed\n");
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return false;
  }

  if (child_pid_ == 0) {
    // Child: become the GTP engine.
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    // Suppress stderr from the engine.
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Build argv: [engine_path, args..., nullptr].
    std::vector<const char*> argv;
    argv.push_back(engine_path_.c_str());
    for (const auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);

    execvp(argv[0], const_cast<char* const*>(argv.data()));
    _exit(1);
  }

  // Parent.
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);
  stdin_fd_ = stdin_pipe[1];
  stdout_fd_ = stdout_pipe[0];
  running_ = true;

  // Verify the engine is alive by sending protocol_version.
  bool ok;
  SendCommand("protocol_version", &ok);
  if (!ok) {
    fprintf(stderr, "GTP: engine at '%s' failed to respond\n",
            engine_path_.c_str());
    Stop();
    return false;
  }
  return true;
}

void Client::Stop() {
  if (!running_) return;
  running_ = false;

  // Try to quit gracefully.
  if (stdin_fd_ >= 0) {
    const char* quit = "quit\n";
    write(stdin_fd_, quit, strlen(quit));
    close(stdin_fd_);
    stdin_fd_ = -1;
  }
  if (stdout_fd_ >= 0) {
    close(stdout_fd_);
    stdout_fd_ = -1;
  }
  if (child_pid_ > 0) {
    int status;
    pid_t result = waitpid(child_pid_, &status, WNOHANG);
    if (result == 0) {
      kill(child_pid_, SIGTERM);
      waitpid(child_pid_, &status, 0);
    }
    child_pid_ = -1;
  }
}

bool Client::IsRunning() const { return running_; }

std::string Client::ReadResponse() {
  // GTP response ends with two consecutive newlines.
  std::string buf;
  char c;
  int consecutive_newlines = 0;
  while (true) {
    ssize_t n = read(stdout_fd_, &c, 1);
    if (n <= 0) break;
    buf += c;
    if (c == '\n') {
      consecutive_newlines++;
      if (consecutive_newlines >= 2) break;
    } else {
      consecutive_newlines = 0;
    }
  }
  return buf;
}

std::string Client::SendCommand(const std::string& command, bool* ok) {
  if (!running_) {
    if (ok) *ok = false;
    return "";
  }

  std::string line = command + "\n";
  const char* data = line.c_str();
  size_t remaining = line.size();
  while (remaining > 0) {
    ssize_t written = write(stdin_fd_, data, remaining);
    if (written <= 0) {
      if (ok) *ok = false;
      return "";
    }
    data += written;
    remaining -= written;
  }

  std::string response = ReadResponse();

  // Parse: "= response_text\n\n" for success, "? error\n\n" for failure.
  if (response.size() >= 2 && response[0] == '=') {
    if (ok) *ok = true;
    // Strip "= " prefix and trailing newlines.
    size_t start = 1;
    if (start < response.size() && response[start] == ' ') start++;
    size_t end = response.find_last_not_of('\n');
    if (end == std::string::npos || end < start) return "";
    return response.substr(start, end - start + 1);
  }

  if (ok) *ok = false;
  // Strip "? " prefix for error text.
  if (response.size() >= 2 && response[0] == '?') {
    size_t start = 1;
    if (start < response.size() && response[start] == ' ') start++;
    size_t end = response.find_last_not_of('\n');
    if (end == std::string::npos || end < start) return "";
    return response.substr(start, end - start + 1);
  }
  return response;
}

// --- Convenience methods ---

bool Client::SetBoardSize(int size) {
  bool ok;
  SendCommand("boardsize " + std::to_string(size), &ok);
  return ok;
}

bool Client::ClearBoard() {
  bool ok;
  SendCommand("clear_board", &ok);
  return ok;
}

bool Client::SetKomi(double komi) {
  bool ok;
  char buf[32];
  snprintf(buf, sizeof(buf), "komi %.1f", komi);
  SendCommand(buf, &ok);
  return ok;
}

bool Client::Play(go::Stone color, const std::string& vertex) {
  std::string cmd =
      "play " + katago::StoneToGtp(color) + " " + vertex;
  bool ok;
  SendCommand(cmd, &ok);
  return ok;
}

bool Client::Play(go::Stone color, int pos, int board_size) {
  return Play(color, katago::PosToGtp(pos, board_size));
}

std::string Client::FinalScore() {
  bool ok;
  std::string result = SendCommand("final_score", &ok);
  if (!ok) return "";
  return result;
}

bool Client::SetupPosition(const go::Board& board) {
  if (!SetBoardSize(board.Size())) return false;
  if (!ClearBoard()) return false;
  for (int i = 0; i < board.NumPositions(); i++) {
    if (board.At(i) == go::Stone::kEmpty) continue;
    if (!Play(board.At(i), i, board.Size())) return false;
  }
  return true;
}

// --- Score parsing ---

double ParseScoreLead(const std::string& score_str) {
  if (score_str.empty() || score_str == "0") return 0.0;
  if (score_str.size() < 3 || score_str[1] != '+') return 0.0;
  double margin = std::stod(score_str.substr(2));
  if (score_str[0] == 'B') return margin;
  if (score_str[0] == 'W') return -margin;
  return 0.0;
}

}  // namespace gtp
