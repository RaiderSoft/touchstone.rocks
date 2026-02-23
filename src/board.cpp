#include "go/board.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace go {

Stone Opponent(Stone s) {
  return (s == Stone::kBlack) ? Stone::kWhite : Stone::kBlack;
}

Board::Board(int size) : size_(size), grid_(size * size, Stone::kEmpty) {}

Board::Board(int size, const std::string& layout) : size_(size) {
  grid_.reserve(size * size);
  for (char c : layout) {
    if (c == '.' ) grid_.push_back(Stone::kEmpty);
    else if (c == 'B') grid_.push_back(Stone::kBlack);
    else if (c == 'W') grid_.push_back(Stone::kWhite);
    // Ignore all other characters (whitespace, etc.)
  }
  if (static_cast<int>(grid_.size()) != size * size) {
    throw std::invalid_argument(
        "Board layout has " + std::to_string(grid_.size()) +
        " cells, expected " + std::to_string(size * size));
  }
}

int Board::Size() const { return size_; }
int Board::NumPositions() const { return size_ * size_; }

Stone Board::At(int pos) const { return grid_[pos]; }
void Board::Set(int pos, Stone s) { grid_[pos] = s; }

void Board::RemoveStones(const std::vector<int>& positions) {
  for (int pos : positions) {
    grid_[pos] = Stone::kEmpty;
  }
}

std::vector<int> Board::Neighbors(int pos) const {
  std::vector<int> result;
  int row = pos / size_;
  int col = pos % size_;
  if (row > 0) result.push_back(pos - size_);          // up
  if (row < size_ - 1) result.push_back(pos + size_);  // down
  if (col > 0) result.push_back(pos - 1);              // left
  if (col < size_ - 1) result.push_back(pos + 1);      // right
  return result;
}

int Board::PosFromRowCol(int row, int col) const {
  return row * size_ + col;
}

int Board::RowOf(int pos) const { return pos / size_; }
int Board::ColOf(int pos) const { return pos % size_; }

bool Board::InBounds(int pos) const {
  return pos >= 0 && pos < size_ * size_;
}

bool Board::operator==(const Board& other) const {
  return size_ == other.size_ && grid_ == other.grid_;
}

bool Board::operator!=(const Board& other) const {
  return !(*this == other);
}

std::string Board::ToString() const {
  std::ostringstream out;
  for (int row = 0; row < size_; ++row) {
    if (row > 0) out << '\n';
    for (int col = 0; col < size_; ++col) {
      if (col > 0) out << ' ';
      Stone s = grid_[row * size_ + col];
      if (s == Stone::kEmpty) out << '.';
      else if (s == Stone::kBlack) out << 'B';
      else out << 'W';
    }
  }
  return out.str();
}

int Board::CountStones(Stone s) const {
  return std::count(grid_.begin(), grid_.end(), s);
}

}  // namespace go
