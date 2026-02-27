#pragma once

#include <vector>

#include "go/board.hpp"
#include "go/game.hpp"
#include "go/move.hpp"
#include "raylib.h"
#include "vision/vision.hpp"

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

// Draw a thick red ring at a grid position (mismatch / error highlight).
void DrawMismatchRing(const GameBoard& gb, int pos);

// Draw thick red rings at fractional grid positions (off-grid pieces).
void DrawOffGridRings(const GameBoard& gb,
                      const touchstone::DetectionResult& det);

// Draw the board background, grid, and stones from a diagram string.
// diagram uses '.', 'B', 'W' characters.
void DrawGameBoardFromDiagram(const GameBoard& gb,
                              const std::string& diagram);

// Draw a single stone at a grid position.
void DrawStone(const GameBoard& gb, int pos, bool black);
