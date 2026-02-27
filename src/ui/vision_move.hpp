#pragma once

#include <functional>
#include <vector>

#include "ui/game_board.hpp"
#include "vision/vision.hpp"

// State for the vision move detection state machine.
// One instance per mode (game mode, puzzle mode).
struct VisionMoveState {
  int detect_pos = -1;
  int detect_confirm = 0;
  int pending_pos = -1;
  double pending_timer = 0.0;
  static constexpr int kDetectFrames = 5;
  void Reset();
};

// Result of one frame of vision move detection.
struct VisionMoveResult {
  int confirmed_pos = -1;        // Move confirmed (SPACE or auto-timer)
  int pending_pos = -1;          // Move in pending state
  float pending_progress = 0.0f; // 0..1 for pie chart
  std::vector<int> mismatches;   // Board errors (excluding the one new stone)
};

// Core detection logic. Call once per frame.
// expected: what the board should look like right now (before the new move)
// move_color: the color of the stone being placed
// det: latest vision detection result
// auto_confirm_secs: 0 = require SPACE, >0 = auto-confirm after N seconds
// dt: frame delta time (GetFrameTime())
// validate: optional callback to check move legality before entering pending
VisionMoveResult UpdateVisionMove(
    VisionMoveState& state,
    const std::vector<touchstone::StoneColor>& expected,
    touchstone::StoneColor move_color,
    const touchstone::DetectionResult& det,
    float auto_confirm_secs, float dt,
    std::function<bool(int)> validate = nullptr);

// Draw pending stone + pie chart overlay. Call after UpdateVisionMove.
void DrawPendingMove(const GameBoard& gb, int pos, bool black,
                     float progress, float auto_confirm_secs);

// Convert a go::Board to a StoneColor vector for vision comparison.
std::vector<touchstone::StoneColor> BoardToVisionColors(const go::Board& board);

// Convert a diagram string ('.', 'B', 'W') to a StoneColor vector.
std::vector<touchstone::StoneColor> DiagramToVisionColors(
    const std::string& diagram);
