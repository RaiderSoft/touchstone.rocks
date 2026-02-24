#include "ui/game_mode.hpp"

#include "raylib.h"

struct DifficultyPreset {
  const char* label;
  const char* profile;  // humanSLProfile value
};

static const DifficultyPreset kPresets[] = {
    {"Novice (20k)", "preaz_20k"},
    {"Beginner (18k)", "preaz_18k"},
    {"Casual (9k)", "preaz_9k"},
    {"Intermediate (5k)", "preaz_5k"},
    {"Advanced (1k)", "preaz_1k"},
};
static const int kNumPresets = 5;

GameSettings RunGameSettings(ChatOverlay& chat) {
  GameSettings settings;
  settings.human_color = go::Stone::kBlack;
  settings.human_sl_profile = "preaz_20k";
  settings.cancelled = false;

  int selected_preset = 0;  // Novice (20k)

  while (!WindowShouldClose()) {
    bool chat_consumed = chat.HandleInput();
    int scr_w = GetScreenWidth();
    int scr_h = GetScreenHeight();
    Vector2 mouse = GetMousePosition();

    BeginDrawing();
    ClearBackground(Color{35, 30, 25, 255});

    // Title.
    const char* title = "GAME SETTINGS";
    int title_sz = 36;
    int tw = MeasureText(title, title_sz);
    DrawText(title, (scr_w - tw) / 2, 40, title_sz,
             Color{220, 180, 100, 255});

    int cx = scr_w / 2;  // Center x.
    int y = 120;

    // --- Color selection ---
    DrawText("Play as:", cx - 170, y, 22, RAYWHITE);
    y += 36;

    const int COLOR_BTN_W = 160;
    const int COLOR_BTN_H = 44;
    const int COLOR_GAP = 20;
    int color_x = cx - COLOR_BTN_W - COLOR_GAP / 2;

    // Black button.
    {
      Rectangle btn = {(float)color_x, (float)y, (float)COLOR_BTN_W,
                        (float)COLOR_BTN_H};
      bool hover = CheckCollisionPointRec(mouse, btn);
      bool selected = (settings.human_color == go::Stone::kBlack);
      Color bg = selected ? Color{30, 30, 30, 255} : Color{50, 50, 50, 255};
      if (hover && !selected) bg = Color{40, 40, 40, 255};
      DrawRectangleRec(btn, bg);
      Color border = selected ? Color{100, 200, 100, 255}
                               : Color{80, 80, 80, 255};
      DrawRectangleLinesEx(btn, selected ? 2.0f : 1.0f, border);
      DrawCircle(color_x + 24, y + COLOR_BTN_H / 2, 10, BLACK);
      DrawCircleLines(color_x + 24, y + COLOR_BTN_H / 2, 10, GRAY);
      DrawText("Black (first)", color_x + 42, y + 12, 18,
               selected ? Color{180, 255, 180, 255} : RAYWHITE);
      if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        settings.human_color = go::Stone::kBlack;
      }
    }

    // White button.
    {
      int wx = cx + COLOR_GAP / 2;
      Rectangle btn = {(float)wx, (float)y, (float)COLOR_BTN_W,
                        (float)COLOR_BTN_H};
      bool hover = CheckCollisionPointRec(mouse, btn);
      bool selected = (settings.human_color == go::Stone::kWhite);
      Color bg = selected ? Color{50, 50, 50, 255} : Color{50, 50, 50, 255};
      if (hover && !selected) bg = Color{60, 60, 60, 255};
      if (selected) bg = Color{60, 60, 60, 255};
      DrawRectangleRec(btn, bg);
      Color border = selected ? Color{100, 200, 100, 255}
                               : Color{80, 80, 80, 255};
      DrawRectangleLinesEx(btn, selected ? 2.0f : 1.0f, border);
      DrawCircle(wx + 24, y + COLOR_BTN_H / 2, 10, WHITE);
      DrawCircleLines(wx + 24, y + COLOR_BTN_H / 2, 10, DARKGRAY);
      DrawText("White", wx + 42, y + 12, 18,
               selected ? Color{180, 255, 180, 255} : RAYWHITE);
      if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        settings.human_color = go::Stone::kWhite;
      }
    }
    y += COLOR_BTN_H + 40;

    // --- Board size ---
    DrawText("Board size:", cx - 170, y, 22, RAYWHITE);
    y += 36;

    {
      static const int kSizes[] = {9, 13, 19};
      static const char* kSizeLabels[] = {"9x9", "13x13", "19x19"};
      const int NUM_SIZES = 3;
      const int SIZE_BTN_W = 100;
      const int SIZE_BTN_H = 38;
      const int SIZE_GAP = 10;
      int total_size_w = NUM_SIZES * SIZE_BTN_W + (NUM_SIZES - 1) * SIZE_GAP;
      int size_x = cx - total_size_w / 2;

      for (int i = 0; i < NUM_SIZES; i++) {
        int bx = size_x + i * (SIZE_BTN_W + SIZE_GAP);
        Rectangle btn = {(float)bx, (float)y, (float)SIZE_BTN_W,
                          (float)SIZE_BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        bool sel = (settings.board_size == kSizes[i]);
        Color bg = sel ? Color{40, 60, 40, 255} : Color{50, 50, 50, 255};
        if (hover && !sel) bg = Color{60, 60, 60, 255};
        DrawRectangleRec(btn, bg);
        Color border =
            sel ? Color{100, 200, 100, 255} : Color{80, 80, 80, 255};
        DrawRectangleLinesEx(btn, sel ? 2.0f : 1.0f, border);
        int ltw = MeasureText(kSizeLabels[i], 18);
        DrawText(kSizeLabels[i], bx + (SIZE_BTN_W - ltw) / 2, y + 9, 18,
                 sel ? Color{180, 255, 180, 255} : RAYWHITE);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          settings.board_size = kSizes[i];
          settings.komi = DefaultKomi(kSizes[i]);
        }
      }
    }
    y += 38 + 40;

    // --- Komi ---
    DrawText("Komi:", cx - 170, y, 22, RAYWHITE);
    y += 36;

    {
      const int KOMI_BTN_SZ = 38;
      const int KOMI_GAP = 12;
      // Layout: [ - ]  7.5  [ + ]
      int total_w = KOMI_BTN_SZ * 2 + KOMI_GAP * 2 + 60;  // 60 for number
      int kx = cx - total_w / 2;

      // Minus button.
      Rectangle minus_btn = {(float)kx, (float)y, (float)KOMI_BTN_SZ,
                              (float)KOMI_BTN_SZ};
      bool m_hover = CheckCollisionPointRec(mouse, minus_btn);
      DrawRectangleRec(minus_btn, m_hover ? Color{70, 50, 50, 255}
                                          : Color{50, 50, 50, 255});
      DrawRectangleLinesEx(minus_btn, 1, Color{180, 100, 100, 255});
      int mw = MeasureText("-", 22);
      DrawText("-", kx + (KOMI_BTN_SZ - mw) / 2, y + 8, 22, RAYWHITE);
      if (m_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        settings.komi -= 0.5;
      }

      // Value display.
      int vx = kx + KOMI_BTN_SZ + KOMI_GAP;
      const char* komi_str = TextFormat("%.1f", settings.komi);
      int kw = MeasureText(komi_str, 22);
      DrawText(komi_str, vx + (60 - kw) / 2, y + 8, 22,
               Color{220, 220, 180, 255});

      // Plus button.
      int px = vx + 60 + KOMI_GAP;
      Rectangle plus_btn = {(float)px, (float)y, (float)KOMI_BTN_SZ,
                             (float)KOMI_BTN_SZ};
      bool p_hover = CheckCollisionPointRec(mouse, plus_btn);
      DrawRectangleRec(plus_btn, p_hover ? Color{50, 70, 50, 255}
                                         : Color{50, 50, 50, 255});
      DrawRectangleLinesEx(plus_btn, 1, Color{100, 180, 100, 255});
      int pw = MeasureText("+", 22);
      DrawText("+", px + (KOMI_BTN_SZ - pw) / 2, y + 8, 22, RAYWHITE);
      if (p_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        settings.komi += 0.5;
      }
    }
    y += 38 + 40;

    // --- Difficulty presets ---
    DrawText("Difficulty:", cx - 170, y, 22, RAYWHITE);
    y += 36;

    const int PRESET_BTN_W = 160;
    const int PRESET_BTN_H = 38;
    const int PRESET_GAP = 10;
    const int ROW1_COUNT = 3;
    const int ROW2_COUNT = kNumPresets - ROW1_COUNT;

    // Row 1: first 3 presets.
    {
      int row_w = ROW1_COUNT * PRESET_BTN_W + (ROW1_COUNT - 1) * PRESET_GAP;
      int rx = cx - row_w / 2;
      for (int i = 0; i < ROW1_COUNT; i++) {
        int bx = rx + i * (PRESET_BTN_W + PRESET_GAP);
        Rectangle btn = {(float)bx, (float)y, (float)PRESET_BTN_W,
                          (float)PRESET_BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        bool sel = (i == selected_preset);
        Color bg = sel ? Color{40, 60, 40, 255} : Color{50, 50, 50, 255};
        if (hover && !sel) bg = Color{60, 60, 60, 255};
        DrawRectangleRec(btn, bg);
        Color border =
            sel ? Color{100, 200, 100, 255} : Color{80, 80, 80, 255};
        DrawRectangleLinesEx(btn, sel ? 2.0f : 1.0f, border);
        int ltw = MeasureText(kPresets[i].label, 16);
        DrawText(kPresets[i].label, bx + (PRESET_BTN_W - ltw) / 2, y + 10, 16,
                 sel ? Color{180, 255, 180, 255} : RAYWHITE);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          selected_preset = i;
          settings.human_sl_profile = kPresets[i].profile;
        }
      }
    }
    y += PRESET_BTN_H + PRESET_GAP;

    // Row 2: remaining presets.
    {
      int row_w = ROW2_COUNT * PRESET_BTN_W + (ROW2_COUNT - 1) * PRESET_GAP;
      int rx = cx - row_w / 2;
      for (int i = 0; i < ROW2_COUNT; i++) {
        int pi = ROW1_COUNT + i;
        int bx = rx + i * (PRESET_BTN_W + PRESET_GAP);
        Rectangle btn = {(float)bx, (float)y, (float)PRESET_BTN_W,
                          (float)PRESET_BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        bool sel = (pi == selected_preset);
        Color bg = sel ? Color{40, 60, 40, 255} : Color{50, 50, 50, 255};
        if (hover && !sel) bg = Color{60, 60, 60, 255};
        DrawRectangleRec(btn, bg);
        Color border =
            sel ? Color{100, 200, 100, 255} : Color{80, 80, 80, 255};
        DrawRectangleLinesEx(btn, sel ? 2.0f : 1.0f, border);
        int ltw = MeasureText(kPresets[pi].label, 16);
        DrawText(kPresets[pi].label, bx + (PRESET_BTN_W - ltw) / 2, y + 10, 16,
                 sel ? Color{180, 255, 180, 255} : RAYWHITE);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          selected_preset = pi;
          settings.human_sl_profile = kPresets[pi].profile;
        }
      }
    }
    y += PRESET_BTN_H + 40;

    // --- Start / Cancel buttons ---
    const int ACTION_BTN_W = 180;
    const int ACTION_BTN_H = 48;
    const int ACTION_GAP = 30;

    // Start button.
    {
      int bx = cx - ACTION_BTN_W - ACTION_GAP / 2;
      Rectangle btn = {(float)bx, (float)y, (float)ACTION_BTN_W,
                        (float)ACTION_BTN_H};
      bool hover = CheckCollisionPointRec(mouse, btn);
      Color bg = hover ? Color{50, 90, 50, 255} : Color{35, 65, 35, 255};
      DrawRectangleRec(btn, bg);
      DrawRectangleLinesEx(btn, 2, Color{100, 220, 100, 255});
      const char* start_label = "Start Game";
      int stw = MeasureText(start_label, 22);
      DrawText(start_label, bx + (ACTION_BTN_W - stw) / 2, y + 12, 22,
               Color{180, 255, 180, 255});
      if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        EndDrawing();
        return settings;
      }
    }

    // Cancel button.
    {
      int bx = cx + ACTION_GAP / 2;
      Rectangle btn = {(float)bx, (float)y, (float)ACTION_BTN_W,
                        (float)ACTION_BTN_H};
      bool hover = CheckCollisionPointRec(mouse, btn);
      Color bg = hover ? Color{80, 50, 50, 255} : Color{55, 40, 40, 255};
      DrawRectangleRec(btn, bg);
      DrawRectangleLinesEx(btn, 1, Color{200, 100, 100, 255});
      const char* cancel_label = "Cancel";
      int ctw = MeasureText(cancel_label, 22);
      DrawText(cancel_label, bx + (ACTION_BTN_W - ctw) / 2, y + 12, 22,
               Color{255, 160, 160, 255});
      if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        settings.cancelled = true;
        EndDrawing();
        return settings;
      }
    }

    // ESC to cancel.
    if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
      settings.cancelled = true;
      EndDrawing();
      return settings;
    }

    chat.Draw(scr_w, scr_h);
    EndDrawing();
  }

  // Window close requested.
  settings.cancelled = true;
  return settings;
}
