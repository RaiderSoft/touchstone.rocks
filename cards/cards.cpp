#include "cards.h"

#include <sstream>

namespace touchstone {

bool IsCorrectMove(const Card& card, int pos) {
  for (int m : card.correct_moves) {
    if (m == pos) return true;
  }
  return false;
}

std::string FormatBoard(int size, const std::string& diagram, int marked_pos) {
  // Parse the diagram into cells (skip whitespace).
  std::vector<char> cells;
  for (char c : diagram) {
    if (c == '.' || c == 'B' || c == 'W') {
      cells.push_back(c);
    }
  }

  std::ostringstream out;
  out << "\n";
  for (int row = 0; row < size; ++row) {
    out << "  ";
    for (int col = 0; col < size; ++col) {
      int pos = row * size + col;
      if (col > 0) out << ' ';

      char cell = (pos < static_cast<int>(cells.size())) ? cells[pos] : '?';

      if (pos == marked_pos) {
        out << '[' << cell << ']';
      } else {
        out << ' ' << cell << ' ';
      }
    }
    out << '\n';
  }
  return out.str();
}

}  // namespace touchstone
