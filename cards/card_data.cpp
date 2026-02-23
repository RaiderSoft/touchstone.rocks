#include "cards.h"

namespace touchstone {

// Position helper: row * 9 + col for a 9x9 board.

static const std::vector<Card> kCards = {
    // --- Capture ---
    {
        "cap-01",
        CardType::kCapture,
        9,
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . B . ."
        ". . . . . B W . ."
        ". . . . . . B . .",
        go::Stone::kBlack,
        {70},  // row 7, col 7 — fill last liberty
        "The white stone is in atari.",
        "Black plays at (7,7) to fill White's last liberty and capture.",
    },
    {
        "cap-02",
        CardType::kCapture,
        9,
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . B B . ."
        ". . . . B W W . ."
        ". . . . . B B . .",
        go::Stone::kBlack,
        {70},  // row 7, col 7 — shared last liberty of 2-stone group
        "The white group has one liberty left.",
        "The two white stones share a single liberty at (7,7). Capture!",
    },

    // --- Defend ---
    {
        "def-01",
        CardType::kDefend,
        9,
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". W W W . . . . ."
        "W B B . . . . . ."
        ". W . . . . . . .",
        go::Stone::kBlack,
        {66},  // row 7, col 3 — extend to gain liberties
        "Your black group is in atari.",
        "Extend to (7,3) to escape atari and gain liberties.",
    },

    // --- Life & Death ---
    {
        "life-01",
        CardType::kLifeAndDeath,
        9,
        "W W W W W W W . ."
        "W B B B B B W . ."
        "W B . . . B W . ."
        "W B B B B B W . ."
        "W W W W W W W . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . .",
        go::Stone::kBlack,
        {21},  // row 2, col 3 — split into two eyes
        "Make two eyes to live.",
        "Play at the center of the eye space to divide it into two eyes.",
    },

    // --- Tesuji ---
    {
        "tesuji-01",
        CardType::kTesuji,
        9,
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . . . . . . . ."
        ". . B B . B B . ."
        ". B W W . W W B ."
        ". . B . . . B . ."
        ". . . . . . . . ."
        ". . . . . . . . .",
        go::Stone::kBlack,
        {49},  // row 5, col 4 — double atari on both white groups
        "One move threatens two groups.",
        "Black at (5,4) puts both white groups in atari. White can only save one.",
    },
};

const std::vector<Card>& AllCards() { return kCards; }

}  // namespace touchstone
