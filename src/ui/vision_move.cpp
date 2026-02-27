#include "ui/vision_move.hpp"

#include "raylib.h"

void VisionMoveState::Reset() {
  detect_pos = -1;
  detect_confirm = 0;
  pending_pos = -1;
  pending_timer = 0.0;
}

VisionMoveResult UpdateVisionMove(
    VisionMoveState& state,
    const std::vector<touchstone::StoneColor>& expected,
    touchstone::StoneColor move_color,
    const touchstone::DetectionResult& det,
    float auto_confirm_secs, float dt,
    std::function<bool(int)> validate) {
  VisionMoveResult result;

  if (!det.board_found) return result;

  int n = static_cast<int>(expected.size());

  // --- Pending state: move detected, waiting for confirmation ---
  if (state.pending_pos >= 0) {
    bool still_there =
        state.pending_pos < static_cast<int>(det.board.size()) &&
        det.board[state.pending_pos] == move_color;

    if (!still_there) {
      state.pending_pos = -1;
      state.pending_timer = 0.0;
    } else {
      state.pending_timer += dt;

      bool auto_confirmed =
          auto_confirm_secs > 0 && state.pending_timer >= auto_confirm_secs;

      if (IsKeyPressed(KEY_SPACE) || auto_confirmed) {
        result.confirmed_pos = state.pending_pos;
        state.Reset();
        return result;
      }

      result.pending_pos = state.pending_pos;
      if (auto_confirm_secs > 0) {
        result.pending_progress =
            static_cast<float>(state.pending_timer) / auto_confirm_secs;
        if (result.pending_progress > 1.0f) result.pending_progress = 1.0f;
      }
      return result;
    }
  }

  // --- Detection state: looking for a new stone ---

  // Compare expected vs detected. Allow exactly one new stone of move_color.
  int new_pos = -1;
  int new_count = 0;
  for (int i = 0; i < n && i < static_cast<int>(det.board.size()); i++) {
    if (expected[i] == touchstone::StoneColor::kEmpty &&
        det.board[i] == move_color) {
      // This is a new stone of the expected color.
      if (!new_count) new_pos = i;
      new_count++;
    } else if (expected[i] != det.board[i]) {
      // Mismatch: expected stone missing or wrong color.
      result.mismatches.push_back(i);
    }
  }

  // If there are real mismatches (beyond the one new stone), report them
  // and don't try to detect a move.
  if (!result.mismatches.empty()) {
    state.detect_pos = -1;
    state.detect_confirm = 0;
    return result;
  }

  // Try to detect exactly one new stone.
  if (new_count == 1) {
    if (new_pos == state.detect_pos) {
      state.detect_confirm++;
      if (state.detect_confirm >= VisionMoveState::kDetectFrames) {
        bool valid = !validate || validate(new_pos);
        if (valid) {
          state.pending_pos = new_pos;
          state.pending_timer = 0.0;
        }
        state.detect_pos = -1;
        state.detect_confirm = 0;
      }
    } else {
      state.detect_pos = new_pos;
      state.detect_confirm = 1;
    }
  } else {
    state.detect_pos = -1;
    state.detect_confirm = 0;
  }

  return result;
}

void DrawPendingMove(const GameBoard& gb, int pos, bool black,
                     float progress, float auto_confirm_secs) {
  Vector2 pp = GameBoardPos(gb, pos);

  // Draw the stone.
  if (black) {
    DrawCircle(pp.x, pp.y, gb.piece_r, BLACK);
  } else {
    DrawCircle(pp.x, pp.y, gb.piece_r, WHITE);
    DrawCircleLines(pp.x, pp.y, gb.piece_r, DARKGRAY);
  }

  // Green confirmation outline.
  DrawCircleLines(pp.x, pp.y, gb.piece_r + 3, GREEN);

  // Countdown pie overlay (fills clockwise from top).
  if (auto_confirm_secs > 0) {
    float start_angle = -90.0f;
    float end_angle = start_angle + progress * 360.0f;
    float pie_r = gb.piece_r + 2;
    DrawCircleSector({pp.x, pp.y}, pie_r, start_angle, end_angle, 36,
                     Color{100, 220, 100, 100});
    DrawCircleSectorLines({pp.x, pp.y}, pie_r, start_angle, end_angle, 36,
                          Color{100, 220, 100, 200});
  }
}

std::vector<touchstone::StoneColor> BoardToVisionColors(
    const go::Board& board) {
  int n = board.NumPositions();
  std::vector<touchstone::StoneColor> colors(n);
  for (int i = 0; i < n; i++) {
    switch (board.At(i)) {
      case go::Stone::kBlack:
        colors[i] = touchstone::StoneColor::kBlack;
        break;
      case go::Stone::kWhite:
        colors[i] = touchstone::StoneColor::kWhite;
        break;
      default:
        colors[i] = touchstone::StoneColor::kEmpty;
        break;
    }
  }
  return colors;
}

std::vector<touchstone::StoneColor> DiagramToVisionColors(
    const std::string& diagram) {
  std::vector<touchstone::StoneColor> colors;
  for (char c : diagram) {
    if (c == '.')
      colors.push_back(touchstone::StoneColor::kEmpty);
    else if (c == 'B')
      colors.push_back(touchstone::StoneColor::kBlack);
    else if (c == 'W')
      colors.push_back(touchstone::StoneColor::kWhite);
  }
  return colors;
}
