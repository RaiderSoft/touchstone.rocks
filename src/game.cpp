#include "go/game.h"

namespace go {

Game::Game(int board_size)
    : board_(board_size),
      previous_board_(board_size),
      current_player_(Stone::kBlack),
      phase_(GamePhase::kPlaying),
      move_number_(0),
      consecutive_passes_(0),
      black_captures_(0),
      white_captures_(0) {}

Game::Game(const Board& initial_board, Stone next_player)
    : board_(initial_board),
      previous_board_(initial_board),
      current_player_(next_player),
      phase_(GamePhase::kPlaying),
      move_number_(0),
      consecutive_passes_(0),
      black_captures_(0),
      white_captures_(0) {}

const Board& Game::GetBoard() const { return board_; }
Stone Game::CurrentPlayer() const { return current_player_; }
GamePhase Game::Phase() const { return phase_; }
int Game::MoveNumber() const { return move_number_; }
int Game::ConsecutivePasses() const { return consecutive_passes_; }
const Board& Game::PreviousBoard() const { return previous_board_; }

int Game::CapturedBy(Stone player) const {
  return (player == Stone::kBlack) ? black_captures_ : white_captures_;
}

MoveResult Game::Play(int pos) {
  if (phase_ == GamePhase::kGameOver) return MoveResult::kOutOfBounds;

  std::vector<int> captured;
  MoveResult result = ValidateMove(board_, pos, current_player_, previous_board_);
  if (result != MoveResult::kOk) return result;

  // Save board state before this move (for ko detection on next move).
  Board board_before_move = board_;

  board_.Set(pos, current_player_);
  captured = ApplyCaptures(board_, pos);

  // Update capture counts.
  int num_captured = static_cast<int>(captured.size());
  if (current_player_ == Stone::kBlack) {
    black_captures_ += num_captured;
  } else {
    white_captures_ += num_captured;
  }

  previous_board_ = board_before_move;
  current_player_ = Opponent(current_player_);
  move_number_++;
  consecutive_passes_ = 0;

  return MoveResult::kOk;
}

void Game::Pass() {
  if (phase_ == GamePhase::kGameOver) return;

  // Save board state before pass for ko (a pass doesn't change the board,
  // but we track previous_board as the state before the last "move").
  previous_board_ = board_;
  current_player_ = Opponent(current_player_);
  consecutive_passes_++;

  if (consecutive_passes_ >= 2) {
    phase_ = GamePhase::kGameOver;
  }
}

ScoreResult Game::Score() const {
  return CalculateScore(board_);
}

}  // namespace go
