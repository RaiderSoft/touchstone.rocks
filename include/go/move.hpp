#pragma once

#include <vector>

#include "go/board.hpp"

namespace go {

// A group is a connected set of same-color stones found by flood fill.
struct Group {
  Stone color;
  std::vector<int> stones;     // sorted positions of all stones in the group
  std::vector<int> liberties;  // sorted unique positions of empty neighbors
};

// Find the group containing the stone at pos.
// Returns an empty group (color=kEmpty, empty vectors) if pos is empty.
Group FindGroup(const Board& board, int pos);

// Find all distinct groups of a given color.
std::vector<Group> FindAllGroups(const Board& board, Stone color);

// Count liberties of the group containing the stone at pos.
// Returns 0 if pos is empty.
int CountLiberties(const Board& board, int pos);

enum class MoveResult {
  kOk,
  kOccupied,
  kSuicide,
  kKoViolation,
  kOutOfBounds,
};

// Remove all opponent groups adjacent to pos that have zero liberties.
// Returns positions of all captured stones.
std::vector<int> ApplyCaptures(Board& board, int pos);

// Would placing color at pos be suicide?
// (After capturing opponent stones, would the placed group have 0 liberties?)
bool IsSuicide(const Board& board, int pos, Stone color);

// Would placing color at pos reproduce previous_board (ko violation)?
bool IsKoViolation(const Board& board, int pos, Stone color,
                   const Board& previous_board);

// Validate without modifying the board.
MoveResult ValidateMove(const Board& board, int pos, Stone color,
                        const Board& previous_board);

// Place a stone and resolve captures. Board is modified if move is legal.
// captured_positions is filled with removed stones.
MoveResult PlayMove(Board& board, int pos, Stone color,
                    const Board& previous_board,
                    std::vector<int>& captured_positions);

}  // namespace go
