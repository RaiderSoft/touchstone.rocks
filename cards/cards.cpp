#include "cards.hpp"

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

const char* CardTypeName(CardType t) {
  switch (t) {
    case CardType::kCapture:      return "Capture";
    case CardType::kDefend:       return "Defend";
    case CardType::kLifeAndDeath: return "Life & Death";
    case CardType::kTesuji:       return "Tesuji";
  }
  return "?";
}

std::vector<int> ParseDiagram(const std::string& diagram) {
  std::vector<int> cells;
  for (char c : diagram) {
    if (c == '.') cells.push_back(0);
    else if (c == 'B') cells.push_back(1);
    else if (c == 'W') cells.push_back(2);
  }
  return cells;
}

}  // namespace touchstone
