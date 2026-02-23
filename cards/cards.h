#pragma once

#include <string>
#include <vector>

#include "go/board.h"

namespace touchstone {

enum class CardType {
  kCapture,       // Capture opponent stones
  kDefend,        // Save your group from capture
  kLifeAndDeath,  // Kill or make live
  kTesuji,        // Clever tactical moves
};

struct Card {
  std::string id;
  CardType type;
  int board_size;
  std::string diagram;
  go::Stone player_to_move;
  std::vector<int> correct_moves;   // acceptable move positions
  std::string hint;                 // shown on H key (can be empty)
  std::string explanation;
};

// Returns all cards in the deck.
const std::vector<Card>& AllCards();

// Check if a position is a correct move for this card.
bool IsCorrectMove(const Card& card, int pos);

// Display a board diagram.  marked_pos < 0 means no highlighting.
std::string FormatBoard(int size, const std::string& diagram,
                        int marked_pos = -1);

}  // namespace touchstone
