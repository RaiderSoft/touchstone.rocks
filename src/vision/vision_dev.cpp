#include "vision/vision_dev.hpp"

#include <cstdio>

#include "go/board.hpp"
#include "raylib.h"

// ---------------------------------------------------------------------------
// Vision move detection
// ---------------------------------------------------------------------------

int DetectVisionMove(touchstone::VisionSystem& vision,
                     const touchstone::Card& card,
                     touchstone::DetectionResult& baseline,
                     bool& baseline_captured) {
  static int last_pos = -1;
  static int confirm_count = 0;
  const int REQUIRED = 5;

  auto det = vision.GetLatestDetection();
  if (!det.board_found) return -1;

  if (!baseline_captured) {
    baseline = det;
    baseline_captured = true;
    last_pos = -1;
    confirm_count = 0;
    return -1;
  }

  touchstone::StoneColor expected =
      (card.player_to_move == go::Stone::kBlack)
          ? touchstone::StoneColor::kBlack
          : touchstone::StoneColor::kWhite;

  int total = card.board_size * card.board_size;
  int new_pos = -1;
  int diff_count = 0;

  for (int i = 0; i < total; i++) {
    if (baseline.board[i] == touchstone::StoneColor::kEmpty &&
        det.board[i] == expected) {
      new_pos = i;
      diff_count++;
    }
  }

  if (diff_count == 1) {
    if (new_pos == last_pos) {
      confirm_count++;
      if (confirm_count >= REQUIRED) {
        confirm_count = 0;
        last_pos = -1;
        return new_pos;
      }
    } else {
      last_pos = new_pos;
      confirm_count = 1;
    }
  } else {
    last_pos = -1;
    confirm_count = 0;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Simple UI helpers for vision dev mode
// ---------------------------------------------------------------------------

struct Slider {
  Rectangle bounds;
  const char* label;
  float min_val, max_val;
  float* value;
};

struct Button {
  Rectangle bounds;
  const char* label;
};

static bool DrawSlider(const Slider& s) {
  bool changed = false;
  Vector2 mouse = GetMousePosition();
  bool hovering = CheckCollisionPointRec(mouse, s.bounds);

  if (hovering && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    float t = (mouse.x - s.bounds.x) / s.bounds.width;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    *s.value = s.min_val + t * (s.max_val - s.min_val);
    changed = true;
  }

  DrawRectangleRec(s.bounds, Color{50, 50, 50, 255});
  DrawRectangleLinesEx(s.bounds, 1, hovering ? SKYBLUE : GRAY);

  float t = (*s.value - s.min_val) / (s.max_val - s.min_val);
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  Rectangle fill = {s.bounds.x, s.bounds.y, s.bounds.width * t,
                    s.bounds.height};
  DrawRectangleRec(fill, Color{70, 130, 180, 180});

  float thumb_x = s.bounds.x + s.bounds.width * t;
  DrawCircle((int)thumb_x, (int)(s.bounds.y + s.bounds.height / 2), 6,
             hovering ? WHITE : LIGHTGRAY);

  DrawText(TextFormat("%s: %.0f", s.label, *s.value),
           (int)s.bounds.x, (int)s.bounds.y - 16, 14, RAYWHITE);

  return changed;
}

static bool DrawButton(const Button& b, Color color) {
  Vector2 mouse = GetMousePosition();
  bool hovering = CheckCollisionPointRec(mouse, b.bounds);
  bool clicked = hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

  Color bg = hovering ? Color{80, 80, 80, 255} : Color{55, 55, 55, 255};
  DrawRectangleRec(b.bounds, bg);
  DrawRectangleLinesEx(b.bounds, 1, color);

  int tw = MeasureText(b.label, 14);
  DrawText(b.label, (int)(b.bounds.x + (b.bounds.width - tw) / 2),
           (int)(b.bounds.y + (b.bounds.height - 14) / 2), 14, color);

  return clicked;
}

// ---------------------------------------------------------------------------
// RunVisionDevMode
// ---------------------------------------------------------------------------

void RunVisionDevMode() {
  const int CAM_W = 640;
  const int CAM_H = 480;
  const int PANEL_W = 340;
  const int WIN_W = CAM_W + PANEL_W;
  const int WIN_H = CAM_H + 60;

  // Load saved calibration to get persisted board size.
  // board_size 0 means "None" (vision disabled).
  auto saved_cal = touchstone::LoadCalibrationData();
  int board_size = saved_cal.board_size;  // 0, 9, or 13.
  if (board_size != 0 && board_size != 9 && board_size != 13) board_size = 9;

  touchstone::VisionSystem vision(board_size > 0 ? board_size : 9);
  if (board_size > 0) vision.LoadCalibration();

  InitWindow(WIN_W, WIN_H, "Touchstone Vision");
  SetTargetFPS(30);

  Image img = GenImageColor(CAM_W, CAM_H, DARKGRAY);
  Texture2D cam_tex = LoadTextureFromImage(img);
  UnloadImage(img);

  bool camera_started = vision.Start(0);
  int corners_set = 0;
  bool calibrated = vision.IsCalibrated();
  if (calibrated) corners_set = 4;

  const char* corner_labels[] = {"TL", "TR", "BL", "BR"};
  bool show_canny = false;

  while (!WindowShouldClose()) {
    if (camera_started) {
      int fw = 0, fh = 0;
      auto& cal_ref = vision.GetCalibration();
      auto pixels = show_canny
                        ? vision.GetLatestFrameCannyRGBA(fw, fh,
                                                         cal_ref.canny_threshold)
                        : vision.GetLatestFrameRGBA(fw, fh);
      if (!pixels.empty() && fw > 0 && fh > 0) {
        if (fw != cam_tex.width || fh != cam_tex.height) {
          UnloadTexture(cam_tex);
          Image resized = GenImageColor(fw, fh, DARKGRAY);
          cam_tex = LoadTextureFromImage(resized);
          UnloadImage(resized);
        }
        UpdateTexture(cam_tex, pixels.data());
      }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 mouse = GetMousePosition();
      if (mouse.x >= 0 && mouse.x < cam_tex.width && mouse.y >= 0 &&
          mouse.y < CAM_H) {
        if (corners_set < 4) {
          vision.SetCalibrationCorner(corners_set, mouse.x, mouse.y);
          corners_set++;
          if (corners_set == 4) {
            vision.GetCalibration().valid = true;
            vision.GetCalibration().board_size = board_size;
            vision.UpdateTransform();
            vision.SaveCalibration();
            calibrated = true;
          }
        }
      }
    }

    BeginDrawing();
    ClearBackground(Color{30, 30, 30, 255});

    DrawTexture(cam_tex, 0, 0, WHITE);

    auto& cal = vision.GetCalibration();
    for (int i = 0; i < corners_set; i++) {
      float cx = cal.corners[i][0];
      float cy = cal.corners[i][1];
      DrawCircle((int)cx, (int)cy, 6, RED);
      DrawText(corner_labels[i], (int)cx + 8, (int)cy - 4, 14, RED);
    }

    if (calibrated) {
      auto det = vision.GetLatestDetection();
      for (int row = 0; row < board_size; row++) {
        for (int col = 0; col < board_size; col++) {
          float gx, gy;
          vision.GetGridPoint(row, col, gx, gy);
          int ix = (int)gx, iy = (int)gy;

          Color dot = GREEN;
          if (det.board_found) {
            int pos = row * board_size + col;
            if (pos < (int)det.board.size()) {
              if (det.board[pos] == touchstone::StoneColor::kBlack)
                dot = Color{40, 40, 40, 255};
              else if (det.board[pos] == touchstone::StoneColor::kWhite)
                dot = WHITE;
            }
          }
          DrawCircle(ix, iy, 5, dot);
          DrawCircleLines(ix, iy, 5, YELLOW);
        }
      }
      for (int i = 0; i < board_size; i++) {
        float x0, y0, x1, y1;
        vision.GetGridPoint(i, 0, x0, y0);
        vision.GetGridPoint(i, board_size - 1, x1, y1);
        DrawLine((int)x0, (int)y0, (int)x1, (int)y1, YELLOW);
        vision.GetGridPoint(0, i, x0, y0);
        vision.GetGridPoint(board_size - 1, i, x1, y1);
        DrawLine((int)x0, (int)y0, (int)x1, (int)y1, YELLOW);
      }
    }

    // Right panel.
    int px = CAM_W + 15;
    int py = 10;
    const int SLIDER_W = PANEL_W - 30;
    const int SLIDER_H = 16;
    const int ROW_H = 50;

    DrawText("BOARD SIZE", px, py, 16, SKYBLUE);
    py += 22;
    {
      const int SZ_BTN_W = 80;
      const int SZ_BTN_H = 24;
      const int SZ_GAP = 10;
      // 0 = None (vision disabled), 9 = 9x9, 13 = 13x13.
      static const int kVisionSizes[] = {9, 13, 0};
      static const char* kVisionLabels[] = {"9x9", "13x13", "None"};
      for (int i = 0; i < 3; i++) {
        int bx = px + i * (SZ_BTN_W + SZ_GAP);
        Button sz_btn = {{(float)bx, (float)py, (float)SZ_BTN_W,
                           (float)SZ_BTN_H},
                          kVisionLabels[i]};
        bool sel = (board_size == kVisionSizes[i]);
        Color color = sel ? GREEN : GRAY;
        if (DrawButton(sz_btn, color) && !sel) {
          board_size = kVisionSizes[i];
          if (board_size == 0) {
            // Disable vision: invalidate calibration and save.
            corners_set = 0;
            calibrated = false;
            vision.GetCalibration().valid = false;
            vision.GetCalibration().board_size = 0;
            vision.SaveCalibration();
          } else {
            vision.SetBoardSize(board_size);
            // Reset calibration — corners are specific to a board size.
            corners_set = 0;
            calibrated = false;
            vision.GetCalibration().valid = false;
            vision.GetCalibration().board_size = board_size;
          }
        }
      }
    }
    py += 34;

    DrawText("CALIBRATION", px, py, 16, SKYBLUE);
    py += 22;
    if (!calibrated && corners_set < 4) {
      DrawText(TextFormat("Click corner %d/4: %s", corners_set + 1,
                          corner_labels[corners_set]),
               px, py, 14, YELLOW);
    } else if (calibrated) {
      DrawText("Calibrated", px, py, 14, GREEN);
    }
    py += 20;

    Button reset_btn = {{(float)px, (float)py, 120, 24}, "Reset Corners"};
    if (DrawButton(reset_btn, RED)) {
      corners_set = 0;
      calibrated = false;
      cal.valid = false;
    }

    Button save_btn = {{(float)(px + 130), (float)py, 80, 24}, "Save"};
    if (DrawButton(save_btn, GREEN) && calibrated) {
      vision.GetCalibration().board_size = board_size;
      vision.SaveCalibration();
    }
    py += 40;

    DrawText("DETECTION", px, py, 16, SKYBLUE);
    py += 22;

    Slider canny_sl = {
        {(float)px, (float)py, (float)SLIDER_W, (float)SLIDER_H},
        "Canny threshold",
        20,
        300,
        &cal.canny_threshold};
    DrawSlider(canny_sl);
    py += ROW_H;

    Slider sens_sl = {
        {(float)px, (float)py, (float)SLIDER_W, (float)SLIDER_H},
        "Sensitivity",
        10,
        80,
        &cal.detect_sensitivity};
    DrawSlider(sens_sl);
    py += ROW_H;

    Slider mid_sl = {
        {(float)px, (float)py, (float)SLIDER_W, (float)SLIDER_H},
        "B/W midpoint",
        40,
        220,
        &cal.brightness_midpoint};
    DrawSlider(mid_sl);
    py += ROW_H;

    Slider confirm_sl = {
        {(float)px, (float)py, (float)SLIDER_W, (float)SLIDER_H},
        "Auto-confirm (sec, 0=off)",
        0,
        10,
        &cal.auto_confirm_seconds};
    DrawSlider(confirm_sl);
    py += 34;

    Button canny_btn = {{(float)px, (float)py, (float)SLIDER_W, 24},
                         show_canny ? "View: CANNY" : "View: NORMAL"};
    if (DrawButton(canny_btn, show_canny ? ORANGE : GREEN)) {
      show_canny = !show_canny;
    }
    py += 34;

    DrawText("DETECTED BOARD", px, py, 16, SKYBLUE);
    py += 22;
    if (calibrated) {
      auto det = vision.GetLatestDetection();
      if (det.board_found) {
        for (int row = 0; row < board_size; row++) {
          char line[64];
          int off = 0;
          for (int col = 0; col < board_size; col++) {
            int pos = row * board_size + col;
            char ch = '.';
            if (pos < (int)det.board.size()) {
              if (det.board[pos] == touchstone::StoneColor::kBlack)
                ch = 'B';
              else if (det.board[pos] == touchstone::StoneColor::kWhite)
                ch = 'W';
            }
            line[off++] = ch;
            line[off++] = ' ';
          }
          line[off] = '\0';
          DrawText(line, px, py + row * 16, 14, RAYWHITE);
        }
      }
    } else {
      DrawText("(not calibrated)", px, py, 14, GRAY);
    }

    DrawText(TextFormat("FPS: %d", GetFPS()), 10, WIN_H - 22, 14, LIME);

    EndDrawing();
  }

  // Auto-save detection params on close.
  vision.GetCalibration().board_size = board_size;
  vision.SaveCalibration();

  UnloadTexture(cam_tex);
  vision.Stop();
  CloseWindow();
}
