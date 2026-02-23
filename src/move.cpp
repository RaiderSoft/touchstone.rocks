#include "go/move.hpp"

#include <algorithm>
#include <queue>
#include <set>

namespace go {

Group FindGroup(const Board& board, int pos) {
  Group group;
  Stone color = board.At(pos);
  if (color == Stone::kEmpty) {
    group.color = Stone::kEmpty;
    return group;
  }

  group.color = color;
  std::vector<bool> visited(board.NumPositions(), false);
  std::set<int> liberty_set;
  std::queue<int> queue;

  queue.push(pos);
  visited[pos] = true;

  while (!queue.empty()) {
    int cur = queue.front();
    queue.pop();
    group.stones.push_back(cur);

    for (int nbr : board.Neighbors(cur)) {
      if (visited[nbr]) continue;
      if (board.At(nbr) == color) {
        visited[nbr] = true;
        queue.push(nbr);
      } else if (board.At(nbr) == Stone::kEmpty) {
        liberty_set.insert(nbr);
      }
    }
  }

  std::sort(group.stones.begin(), group.stones.end());
  group.liberties.assign(liberty_set.begin(), liberty_set.end());
  return group;
}

std::vector<Group> FindAllGroups(const Board& board, Stone color) {
  std::vector<Group> groups;
  std::vector<bool> visited(board.NumPositions(), false);

  for (int pos = 0; pos < board.NumPositions(); ++pos) {
    if (board.At(pos) == color && !visited[pos]) {
      Group g = FindGroup(board, pos);
      for (int s : g.stones) {
        visited[s] = true;
      }
      groups.push_back(std::move(g));
    }
  }
  return groups;
}

int CountLiberties(const Board& board, int pos) {
  if (board.At(pos) == Stone::kEmpty) return 0;
  Group g = FindGroup(board, pos);
  return static_cast<int>(g.liberties.size());
}

std::vector<int> ApplyCaptures(Board& board, int pos) {
  Stone placed = board.At(pos);
  Stone opponent = Opponent(placed);
  std::vector<int> all_captured;

  // Check each neighbor for opponent groups with zero liberties.
  std::set<int> checked_roots;
  for (int nbr : board.Neighbors(pos)) {
    if (board.At(nbr) != opponent) continue;
    // Avoid re-checking the same group via different neighbors.
    if (checked_roots.count(nbr)) continue;

    Group g = FindGroup(board, nbr);
    for (int s : g.stones) checked_roots.insert(s);

    if (g.liberties.empty()) {
      for (int s : g.stones) {
        all_captured.push_back(s);
      }
    }
  }

  board.RemoveStones(all_captured);
  std::sort(all_captured.begin(), all_captured.end());
  return all_captured;
}

bool IsSuicide(const Board& board, int pos, Stone color) {
  // Simulate placing the stone.
  Board copy = board;
  copy.Set(pos, color);

  // Simulate captures of opponent groups.
  ApplyCaptures(copy, pos);

  // Check if the placed stone's group now has any liberties.
  Group g = FindGroup(copy, pos);
  return g.liberties.empty();
}

bool IsKoViolation(const Board& board, int pos, Stone color,
                   const Board& previous_board) {
  // Simulate the move on a copy.
  Board copy = board;
  copy.Set(pos, color);
  ApplyCaptures(copy, pos);

  return copy == previous_board;
}

MoveResult ValidateMove(const Board& board, int pos, Stone color,
                        const Board& previous_board) {
  if (!board.InBounds(pos)) return MoveResult::kOutOfBounds;
  if (board.At(pos) != Stone::kEmpty) return MoveResult::kOccupied;
  if (IsSuicide(board, pos, color)) return MoveResult::kSuicide;
  if (IsKoViolation(board, pos, color, previous_board)) {
    return MoveResult::kKoViolation;
  }
  return MoveResult::kOk;
}

MoveResult PlayMove(Board& board, int pos, Stone color,
                    const Board& previous_board,
                    std::vector<int>& captured_positions) {
  MoveResult result = ValidateMove(board, pos, color, previous_board);
  if (result != MoveResult::kOk) return result;

  board.Set(pos, color);
  captured_positions = ApplyCaptures(board, pos);
  return MoveResult::kOk;
}

}  // namespace go
