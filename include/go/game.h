#pragma once

#include "go/board.h"
#include "go/move.h"
#include "go/scoring.h"

namespace go {

enum class GamePhase {
  kPlaying,
  kGameOver,
};

class Game {
 public:
  explicit Game(int board_size);

  // Start a game from an existing position with the given player to move.
  Game(const Board& initial_board, Stone next_player);

  const Board& GetBoard() const;
  Stone CurrentPlayer() const;
  GamePhase Phase() const;
  int MoveNumber() const;

  // Play a stone. Returns kOk and advances turn if legal.
  MoveResult Play(int pos);

  // Pass the current player's turn.
  // Two consecutive passes end the game.
  void Pass();

  int ConsecutivePasses() const;

  // Compute score (only meaningful when game is over).
  ScoreResult Score() const;

  const Board& PreviousBoard() const;

  int CapturedBy(Stone player) const;

 private:
  Board board_;
  Board previous_board_;
  Stone current_player_;
  GamePhase phase_;
  int move_number_;
  int consecutive_passes_;
  int black_captures_;
  int white_captures_;
};

}  // namespace go
