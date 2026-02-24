#pragma once

#include "go/board.hpp"
#include "ui/game_board.hpp"

void DrawAtariIndicator(const GameBoard& gb, const go::Board& board);
void DrawLibertyCount(const GameBoard& gb, const go::Board& board,
                      bool atari_also_on);
void DrawScoreboard(const GameBoard& gb, const go::Board& board);
void DrawHelpScreen(int scr_w, int scr_h);
