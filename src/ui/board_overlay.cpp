#include "ui/board_overlay.hpp"

#include <cstdio>

#include "go/scoring.hpp"
#include "raylib.h"

void DrawAtariIndicator(const GameBoard& gb, const go::Board& board) {
  // Always-visible badge so user knows mode is active.
  {
    const char* label = "ATARI";
    int fsz = 12;
    int tw = MeasureText(label, fsz);
    int lx = static_cast<int>(gb.offset_x) + 2;
    int ly = static_cast<int>(gb.offset_y) - fsz - 4;
    Color badge_bg = {255, 120, 60, 180};
    Color badge_fg = {255, 255, 255, 240};
    DrawRectangleRounded(
        Rectangle{(float)(lx - 4), (float)(ly - 2), (float)(tw + 8),
                  (float)(fsz + 4)},
        0.4f, 4, badge_bg);
    DrawText(label, lx, ly, fsz, badge_fg);
  }

  for (auto color : {go::Stone::kBlack, go::Stone::kWhite}) {
    auto groups = go::FindAllGroups(board, color);
    for (const auto& g : groups) {
      if (g.liberties.size() != 1) continue;
      for (int pos : g.stones) {
        Vector2 p = GameBoardPos(gb, pos);
        // Diamond positioned at top-right of stone.
        float dx = gb.piece_r * 0.65f;
        float dy = -gb.piece_r * 0.65f;
        float cx = p.x + dx;
        float cy = p.y + dy;
        float sz = gb.piece_r * 0.25f;
        // Filled diamond (two triangles).
        Color dc = {255, 120, 60, 220};
        DrawTriangle(
            Vector2{cx, cy - sz}, Vector2{cx - sz, cy}, Vector2{cx + sz, cy},
            dc);
        DrawTriangle(
            Vector2{cx - sz, cy}, Vector2{cx, cy + sz}, Vector2{cx + sz, cy},
            dc);
      }
    }
  }
}

void DrawLibertyCount(const GameBoard& gb, const go::Board& board,
                      bool atari_also_on) {
  // Always-visible badge so user knows mode is active.
  {
    const char* label = "LIBS";
    int fsz = 12;
    int tw = MeasureText(label, fsz);
    int lx = static_cast<int>(gb.offset_x) + 2;
    int ly = static_cast<int>(gb.offset_y) - fsz - 4;
    if (atari_also_on) lx += 52;  // Clear "ATARI" badge.
    Color badge_bg = {80, 160, 220, 180};
    Color badge_fg = {255, 255, 255, 240};
    DrawRectangleRounded(
        Rectangle{(float)(lx - 4), (float)(ly - 2), (float)(tw + 8),
                  (float)(fsz + 4)},
        0.4f, 4, badge_bg);
    DrawText(label, lx, ly, fsz, badge_fg);
  }

  for (auto color : {go::Stone::kBlack, go::Stone::kWhite}) {
    auto groups = go::FindAllGroups(board, color);
    for (const auto& g : groups) {
      int libs = static_cast<int>(g.liberties.size());
      Color text_color = (color == go::Stone::kBlack)
                             ? Color{255, 255, 255, 220}
                             : Color{0, 0, 0, 220};
      for (int pos : g.stones) {
        Vector2 p = GameBoardPos(gb, pos);
        const char* buf = TextFormat("%d", libs);
        int font_sz = static_cast<int>(gb.piece_r * 1.1f);
        if (font_sz < 10) font_sz = 10;
        int tw = MeasureText(buf, font_sz);
        DrawText(buf, static_cast<int>(p.x - tw / 2),
                 static_cast<int>(p.y - font_sz / 2), font_sz, text_color);
      }
    }
  }
}

void DrawScoreboard(const GameBoard& gb, const go::Board& board) {
  go::ScoreResult s = go::CalculateScore(board);

  const int FONT_SZ = 20;
  const int PAD = 10;
  const int STONE_R = 7;
  const int ROW_H = 28;
  const int W = 110;
  const int H = ROW_H * 2 + PAD * 2;
  int x = 10;
  // Vertically center on the board area.
  int y = static_cast<int>(gb.offset_y + (gb.size - 1) * gb.cell / 2.0f) - H / 2;

  // Background.
  DrawRectangle(x, y, W, H, Color{20, 20, 20, 200});
  DrawRectangleLinesEx(
      Rectangle{(float)x, (float)y, (float)W, (float)H},
      1, Color{80, 80, 80, 180});

  // Black row.
  int row_y = y + PAD;
  int cx = x + PAD;
  DrawCircle(cx + STONE_R, row_y + ROW_H / 2, STONE_R, BLACK);
  DrawCircleLines(cx + STONE_R, row_y + ROW_H / 2, STONE_R, GRAY);
  DrawText(TextFormat("%.1f", s.black_score),
           cx + STONE_R * 2 + 8, row_y + (ROW_H - FONT_SZ) / 2,
           FONT_SZ, Color{255, 255, 255, 255});

  // Separator line.
  DrawLine(x + 8, row_y + ROW_H, x + W - 8, row_y + ROW_H,
           Color{80, 80, 80, 150});

  // White row.
  row_y += ROW_H;
  DrawCircle(cx + STONE_R, row_y + ROW_H / 2, STONE_R, WHITE);
  DrawCircleLines(cx + STONE_R, row_y + ROW_H / 2, STONE_R, DARKGRAY);
  DrawText(TextFormat("%.1f", s.white_score),
           cx + STONE_R * 2 + 8, row_y + (ROW_H - FONT_SZ) / 2,
           FONT_SZ, Color{255, 255, 255, 255});
}

void DrawHelpScreen(int scr_w, int scr_h) {
  // Semi-transparent backdrop.
  DrawRectangle(0, 0, scr_w, scr_h, Color{0, 0, 0, 180});

  const int panel_w = 380;
  const int panel_h = 420;
  int px = (scr_w - panel_w) / 2;
  int py = (scr_h - panel_h) / 2;
  DrawRectangle(px, py, panel_w, panel_h, Color{25, 25, 30, 245});
  DrawRectangleLinesEx(
      Rectangle{(float)px, (float)py, (float)panel_w, (float)panel_h}, 2,
      Color{120, 120, 140, 255});

  const char* title = "KEYBOARD SHORTCUTS";
  int tw = MeasureText(title, 22);
  DrawText(title, px + (panel_w - tw) / 2, py + 16, 22, RAYWHITE);

  struct Entry {
    const char* key;
    const char* desc;
  };
  Entry entries[] = {
      {"H", "Toggle this help screen"},
      {"P", "Pass"},
      {"Backspace", "Undo"},
      {"R", "Redo"},
      {"O", "Toggle territory overlay"},
      {"A", "Toggle top move suggestions"},
      {"K", "Toggle KataGo stats panel"},
      {"T", "Toggle atari indicator"},
      {"G", "Toggle liberty count"},
      {"ESC", "Pause / menu"},
      {"Click", "Place stone"},
      {"Mic btn", "Voice input (hold)"},
  };
  int n = sizeof(entries) / sizeof(entries[0]);

  Color key_col = {180, 220, 255, 255};
  Color desc_col = {200, 200, 200, 230};
  int y = py + 52;
  int line_h = 28;
  int key_x = px + 24;
  int desc_x = px + 140;

  for (int i = 0; i < n; i++) {
    // Key badge.
    int kw = MeasureText(entries[i].key, 16);
    DrawRectangleRounded(
        Rectangle{(float)(key_x - 4), (float)(y - 2), (float)(kw + 12), 22},
        0.3f, 4, Color{50, 55, 70, 255});
    DrawText(entries[i].key, key_x + 2, y, 16, key_col);
    // Description.
    DrawText(entries[i].desc, desc_x, y, 16, desc_col);
    y += line_h;
  }

  // Footer.
  const char* footer = "Press H to close";
  int fw = MeasureText(footer, 14);
  DrawText(footer, px + (panel_w - fw) / 2, py + panel_h - 30, 14,
           Color{140, 140, 140, 200});
}
