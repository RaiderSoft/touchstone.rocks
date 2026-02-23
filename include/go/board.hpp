#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace go {

enum class Stone : uint8_t {
  kEmpty = 0,
  kBlack = 1,
  kWhite = 2,
};

// Returns the opponent's stone color. Undefined for kEmpty.
Stone Opponent(Stone s);

class Board {
 public:
  explicit Board(int size);

  // Construct a board from a string representation.
  // Characters: '.' = empty, 'B' = black, 'W' = white.
  // All whitespace is ignored.
  Board(int size, const std::string& layout);

  int Size() const;
  int NumPositions() const;

  Stone At(int pos) const;
  void Set(int pos, Stone s);

  // Remove all stones at the given positions.
  void RemoveStones(const std::vector<int>& positions);

  // Returns the up-to-4 orthogonal neighbors of a position.
  std::vector<int> Neighbors(int pos) const;

  // Coordinate conversions.
  int PosFromRowCol(int row, int col) const;
  int RowOf(int pos) const;
  int ColOf(int pos) const;
  bool InBounds(int pos) const;

  // Board equality for ko detection.
  bool operator==(const Board& other) const;
  bool operator!=(const Board& other) const;

  // Render as multi-line string ('. B W' format, one space between cells).
  std::string ToString() const;

  int CountStones(Stone s) const;

 private:
  int size_;
  std::vector<Stone> grid_;
};

}  // namespace go
