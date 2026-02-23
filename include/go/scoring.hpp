#pragma once

#include <vector>

#include "go/board.hpp"

namespace go {

constexpr double kKomi = 7.5;

// A connected region of empty intersections and its ownership.
struct Territory {
  std::vector<int> positions;  // sorted empty positions in this region
  Stone owner;                 // kBlack, kWhite, or kEmpty (neutral/dame)
};

// Find all connected empty regions and determine ownership.
// A region bordered only by Black is Black territory.
// A region bordered only by White is White territory.
// A region bordered by both (or neither) is neutral (kEmpty).
std::vector<Territory> FindTerritories(const Board& board);

// Find the single territory region starting from an empty position.
Territory FindTerritoryRegion(const Board& board, int pos);

struct ScoreResult {
  double black_score;    // stones + territory
  double white_score;    // stones + territory + komi
  int black_stones;
  int white_stones;
  int black_territory;
  int white_territory;
  Stone winner;
};

// Chinese scoring: stones on board + territory + komi for White.
ScoreResult CalculateScore(const Board& board);

}  // namespace go
