#include "go/scoring.h"

#include <algorithm>
#include <queue>
#include <set>

namespace go {

Territory FindTerritoryRegion(const Board& board, int pos) {
  Territory territory;
  territory.owner = Stone::kEmpty;

  if (board.At(pos) != Stone::kEmpty) return territory;

  std::vector<bool> visited(board.NumPositions(), false);
  std::queue<int> queue;
  bool borders_black = false;
  bool borders_white = false;

  queue.push(pos);
  visited[pos] = true;

  while (!queue.empty()) {
    int cur = queue.front();
    queue.pop();
    territory.positions.push_back(cur);

    for (int nbr : board.Neighbors(cur)) {
      if (visited[nbr]) continue;
      Stone s = board.At(nbr);
      if (s == Stone::kEmpty) {
        visited[nbr] = true;
        queue.push(nbr);
      } else if (s == Stone::kBlack) {
        borders_black = true;
      } else {
        borders_white = true;
      }
    }
  }

  std::sort(territory.positions.begin(), territory.positions.end());

  if (borders_black && !borders_white) {
    territory.owner = Stone::kBlack;
  } else if (borders_white && !borders_black) {
    territory.owner = Stone::kWhite;
  }
  // Otherwise stays kEmpty (neutral/dame).

  return territory;
}

std::vector<Territory> FindTerritories(const Board& board) {
  std::vector<Territory> territories;
  std::vector<bool> visited(board.NumPositions(), false);

  for (int pos = 0; pos < board.NumPositions(); ++pos) {
    if (board.At(pos) == Stone::kEmpty && !visited[pos]) {
      Territory t = FindTerritoryRegion(board, pos);
      for (int p : t.positions) {
        visited[p] = true;
      }
      territories.push_back(std::move(t));
    }
  }
  return territories;
}

ScoreResult CalculateScore(const Board& board) {
  ScoreResult result{};
  result.black_stones = board.CountStones(Stone::kBlack);
  result.white_stones = board.CountStones(Stone::kWhite);

  auto territories = FindTerritories(board);
  for (const auto& t : territories) {
    int count = static_cast<int>(t.positions.size());
    if (t.owner == Stone::kBlack) {
      result.black_territory += count;
    } else if (t.owner == Stone::kWhite) {
      result.white_territory += count;
    }
  }

  result.black_score = result.black_stones + result.black_territory;
  result.white_score = result.white_stones + result.white_territory + kKomi;

  if (result.black_score > result.white_score) {
    result.winner = Stone::kBlack;
  } else if (result.white_score > result.black_score) {
    result.winner = Stone::kWhite;
  } else {
    result.winner = Stone::kEmpty;  // tie (shouldn't happen with 0.5 komi)
  }

  return result;
}

}  // namespace go
