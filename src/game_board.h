#pragma once

#include <vector>

#include "go/board.h"
#include "go/game.h"
#include "go/move.h"
#include "raylib.h"
#include "vision.h"

// Board rendering helpers for fullscreen game mode.
struct GameBoard {
  int size;
  float cell;
  float margin;
  float offset_x, offset_y;
  float piece_r;
  float click_r;
  int status_y;
  int win_w, win_h;
};

GameBoard CalcGameBoard(int size, int screen_w, int screen_h);
Vector2 GameBoardPos(const GameBoard& gb, int pos);
void DrawGameBoard(const GameBoard& gb, const go::Board& board);
int GameBoardClick(const GameBoard& gb);
void DrawGameStatus(const GameBoard& gb, const char* text);

// Simple AI: prioritize captures, save own groups, avoid filling eyes, else
// random legal move. Returns -1 to pass.
int ComputerMove(const go::Game& game);

// Move the current window fullscreen to the secondary monitor if one exists.
void MoveToSecondMonitor();

// Compare expected go::Board state against vision detection.
// Returns positions where the physical board doesn't match.
std::vector<int> FindBoardMismatches(const go::Board& expected,
                                     const touchstone::DetectionResult& det);
