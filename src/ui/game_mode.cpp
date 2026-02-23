#include "ui/game_mode.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "board.hpp"
#include "ui/game_board.hpp"
#include "persist/game_save.hpp"
#include "go/game.hpp"
#include "go/move.hpp"
#include "go/scoring.hpp"
#include "raylib.h"

// ---------------------------------------------------------------------------
// KataGo overlay rendering
// ---------------------------------------------------------------------------

static void DrawOwnershipOverlay(const GameBoard& gb,
                                 const katago::AnalysisResult& analysis) {
  if (!analysis.valid || analysis.ownership.empty()) return;
  int n = gb.size * gb.size;
  for (int i = 0; i < n &&
       i < static_cast<int>(analysis.ownership.size()); i++) {
    double own = analysis.ownership[i];
    if (std::abs(own) < 0.05) continue;
    Vector2 p = GameBoardPos(gb, i);
    float sz = gb.cell * 0.4f;
    unsigned char opacity =
        static_cast<unsigned char>(std::abs(own) * 0.4 * 255);
    Color c;
    if (own > 0) {
      c = Color{0, 0, 0, opacity};
    } else {
      c = Color{255, 255, 255, opacity};
    }
    DrawRectangle(static_cast<int>(p.x - sz), static_cast<int>(p.y - sz),
                  static_cast<int>(sz * 2), static_cast<int>(sz * 2), c);
  }
}

static void DrawTopMoves(const GameBoard& gb,
                         const katago::AnalysisResult& analysis) {
  if (!analysis.valid || analysis.moves.empty()) return;
  int n = std::min(3, static_cast<int>(analysis.moves.size()));
  for (int i = 0; i < n; i++) {
    const auto& m = analysis.moves[i];
    if (m.pos < 0) continue;
    Vector2 p = GameBoardPos(gb, m.pos);
    Color rank_color = (i == 0) ? GREEN : (i == 1) ? YELLOW : ORANGE;
    DrawCircle(static_cast<int>(p.x), static_cast<int>(p.y),
               gb.piece_r * 0.3f, rank_color);
    char rank[4];
    snprintf(rank, sizeof(rank), "%d", i + 1);
    DrawText(rank, static_cast<int>(p.x) - 4,
             static_cast<int>(p.y) - 6, 12, WHITE);
  }
}

// MoveRecord is defined in game_mode.h

// ---------------------------------------------------------------------------
// Atari and liberty overlays
// ---------------------------------------------------------------------------

// Draw a small filled diamond at the top-right of each stone in atari.
static void DrawAtariIndicator(const GameBoard& gb, const go::Board& board) {
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

// Draw liberty count as a number centered on each stone.
static void DrawLibertyCount(const GameBoard& gb, const go::Board& board,
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
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", libs);
        int font_sz = static_cast<int>(gb.piece_r * 1.1f);
        if (font_sz < 10) font_sz = 10;
        int tw = MeasureText(buf, font_sz);
        DrawText(buf, static_cast<int>(p.x - tw / 2),
                 static_cast<int>(p.y - font_sz / 2), font_sz, text_color);
      }
    }
  }
}

// Help screen overlay showing all keyboard shortcuts.
static void DrawHelpScreen(int scr_w, int scr_h) {
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

// Maps quality [-1,+1] to a color: red (blunder) -> yellow -> green (great).
static Color QualityColor(float quality) {
  float t = (quality + 1.0f) * 0.5f;  // map [-1,1] to [0,1]
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  unsigned char red, green;
  if (t < 0.5f) {
    red = 255;
    green = static_cast<unsigned char>(t * 2.0f * 255.0f);
  } else {
    red = static_cast<unsigned char>((1.0f - t) * 2.0f * 255.0f);
    green = 255;
  }
  return {red, green, 0, 255};
}

// ---------------------------------------------------------------------------
// KataGo statistics panel (toggled with K key)
// ---------------------------------------------------------------------------

static void DrawKataGoStatsPanel(const GameBoard& gb,
                                  const katago::AnalysisResult& analysis,
                                  go::Stone current_player,
                                  const std::vector<MoveRecord>& move_log,
                                  go::Stone human_color) {
  const int PANEL_W = 360;
  const int PANEL_MARGIN = 10;
  int px = gb.win_w - PANEL_W - PANEL_MARGIN;
  int py = 30;
  const int LINE_H = 20;
  const int FONT_SZ = 16;
  const Color BG = {15, 15, 15, 230};
  const Color BORDER = {80, 80, 80, 200};
  const Color LABEL_COL = {160, 160, 160, 255};
  const Color VALUE_COL = {255, 255, 255, 255};

  // Compute total panel height: analysis section + move log.
  int analysis_h = 0;
  if (!analysis.valid) {
    analysis_h = 60;
  } else {
    analysis_h = 36 + 30 + LINE_H * 2 + 24;
  }

  int log_header_h = move_log.empty() ? 0 : 28;
  int max_log_lines = 20;
  int visible_moves = std::min(static_cast<int>(move_log.size()), max_log_lines);
  int log_h = log_header_h + visible_moves * LINE_H + (move_log.empty() ? 0 : 8);
  int panel_h = analysis_h + log_h;

  // Clamp panel to screen height.
  int max_panel_h = gb.win_h - py - 20;
  if (panel_h > max_panel_h) panel_h = max_panel_h;

  DrawRectangle(px, py, PANEL_W, panel_h, BG);
  DrawRectangleLinesEx(
      Rectangle{(float)px, (float)py, (float)PANEL_W, (float)panel_h}, 1,
      BORDER);

  int y = py + 8;
  int x = px + 10;
  int content_w = PANEL_W - 20;

  if (!analysis.valid) {
    DrawText("Waiting for analysis...", x, y + 12, FONT_SZ, LABEL_COL);
    y += 40;
  } else {
    bool black_winning = analysis.winrate >= 0.5;
    double win_pct = analysis.winrate * 100.0;
    double white_pct = 100.0 - win_pct;

    // Large winner banner.
    {
      char banner[64];
      if (black_winning) {
        snprintf(banner, sizeof(banner), "BLACK  %.0f%%", win_pct);
      } else {
        snprintf(banner, sizeof(banner), "WHITE  %.0f%%", white_pct);
      }
      Color banner_col = black_winning ? Color{80, 80, 80, 255}
                                       : Color{240, 240, 240, 255};
      Color banner_bg = black_winning ? Color{30, 30, 30, 255}
                                      : Color{200, 200, 200, 255};
      Color banner_text = black_winning ? Color{255, 255, 255, 255}
                                        : Color{20, 20, 20, 255};

      DrawRectangle(x, y, content_w, 28, banner_bg);
      DrawRectangleLinesEx(
          Rectangle{(float)x, (float)y, (float)content_w, 28.0f}, 1,
          banner_col);
      int tw = MeasureText(banner, 22);
      DrawText(banner, x + (content_w - tw) / 2, y + 3, 22, banner_text);
      y += 36;
    }

    // Winrate bar.
    {
      int bar_h = 20;
      int black_w = static_cast<int>(analysis.winrate * content_w);
      if (black_w < 1) black_w = 1;
      if (black_w > content_w - 1) black_w = content_w - 1;
      int white_w = content_w - black_w;

      DrawRectangle(x, y, black_w, bar_h, Color{40, 40, 40, 255});
      DrawRectangle(x + black_w, y, white_w, bar_h, Color{220, 220, 220, 255});
      DrawRectangleLinesEx(
          Rectangle{(float)x, (float)y, (float)content_w, (float)bar_h}, 1,
          BORDER);

      char b_lbl[16], w_lbl[16];
      snprintf(b_lbl, sizeof(b_lbl), "%.0f%%", win_pct);
      snprintf(w_lbl, sizeof(w_lbl), "%.0f%%", white_pct);
      if (win_pct >= 10)
        DrawText(b_lbl, x + 4, y + 2, 14, Color{200, 200, 200, 255});
      if (white_pct >= 10)
        DrawText(w_lbl, x + content_w - MeasureText(w_lbl, 14) - 4, y + 2, 14,
                 Color{60, 60, 60, 255});
      y += bar_h + 8;
    }

    // Score lead.
    {
      char buf[64];
      if (std::abs(analysis.score_lead) < 0.5) {
        snprintf(buf, sizeof(buf), "Score: Even");
      } else if (analysis.score_lead > 0) {
        snprintf(buf, sizeof(buf), "Score: Black +%.1f", analysis.score_lead);
      } else {
        snprintf(buf, sizeof(buf), "Score: White +%.1f", -analysis.score_lead);
      }
      DrawText(buf, x, y, FONT_SZ, VALUE_COL);
      y += LINE_H;
    }

    // Territory estimate.
    {
      int black_territory = 0, white_territory = 0;
      for (double own : analysis.ownership) {
        if (own > 0.5) black_territory++;
        else if (own < -0.5) white_territory++;
      }
      char buf[64];
      snprintf(buf, sizeof(buf), "Territory: B~%d  W~%d", black_territory,
               white_territory);
      DrawText(buf, x, y, FONT_SZ, LABEL_COL);
      y += LINE_H + 8;
    }
  }

  // --- Move log ---
  if (!move_log.empty()) {
    // Separator line.
    DrawLine(px + 4, y, px + PANEL_W - 4, y, BORDER);
    y += 4;

    // Header row.
    DrawText("#", x, y, 14, LABEL_COL);
    DrawText("Move", x + 28, y, 14, LABEL_COL);
    DrawText("WR", x + 80, y, 14, LABEL_COL);
    DrawText("Delta", x + 126, y, 14, LABEL_COL);
    DrawText("Best was", x + 182, y, 14, LABEL_COL);
    y += LINE_H + 4;

    // Show most recent moves first, limited by space.
    int start = std::max(0, static_cast<int>(move_log.size()) - max_log_lines);
    int bottom_limit = py + panel_h - 8;

    for (int i = static_cast<int>(move_log.size()) - 1;
         i >= start && y + LINE_H <= bottom_limit; i--) {
      const auto& rec = move_log[i];
      bool is_human = (rec.color == human_color);

      // Quality color dot.
      Color qc = rec.analysis_complete ? QualityColor(rec.quality)
                                       : Color{128, 128, 128, 255};
      DrawCircle(x - 2, y + LINE_H / 2, 3, qc);

      // Move number.
      char num_buf[8];
      snprintf(num_buf, sizeof(num_buf), "%d", rec.move_number);
      DrawText(num_buf, x + 4, y, 14, LABEL_COL);

      // Move played (colored by player).
      Color move_col = is_human ? Color{140, 200, 255, 255}
                                : Color{255, 180, 140, 255};
      char move_buf[16];
      snprintf(move_buf, sizeof(move_buf), "%s%s",
               rec.color == go::Stone::kBlack ? "B " : "W ",
               rec.gtp_move.c_str());
      DrawText(move_buf, x + 28, y, 14, move_col);

      if (rec.analysis_complete) {
        // Winrate after.
        char wr_buf[16];
        snprintf(wr_buf, sizeof(wr_buf), "%.0f%%", rec.winrate_after * 100.0);
        DrawText(wr_buf, x + 80, y, 14, VALUE_COL);

        // Delta from mover's perspective.
        double delta;
        if (rec.color == go::Stone::kBlack) {
          delta = rec.winrate_after - rec.winrate_before;
        } else {
          delta = rec.winrate_before - rec.winrate_after;
        }
        char delta_buf[16];
        snprintf(delta_buf, sizeof(delta_buf), "%+.1f%%", delta * 100.0);
        DrawText(delta_buf, x + 126, y, 14, qc);

        // Top 3 KataGo suggestions.
        DrawText(rec.top3_moves.c_str(), x + 182, y, 14, LABEL_COL);
      } else {
        DrawText("...", x + 80, y, 14, LABEL_COL);
      }

      y += LINE_H;
    }
  }
}

// ---------------------------------------------------------------------------
// Live scoreboard (actual area scoring, not engine prediction)
// ---------------------------------------------------------------------------

static void DrawScoreboard(const GameBoard& gb, const go::Board& board) {
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
  char b_buf[32];
  snprintf(b_buf, sizeof(b_buf), "%.1f", s.black_score);
  DrawText(b_buf, cx + STONE_R * 2 + 8, row_y + (ROW_H - FONT_SZ) / 2,
           FONT_SZ, Color{255, 255, 255, 255});

  // Separator line.
  DrawLine(x + 8, row_y + ROW_H, x + W - 8, row_y + ROW_H,
           Color{80, 80, 80, 150});

  // White row.
  row_y += ROW_H;
  DrawCircle(cx + STONE_R, row_y + ROW_H / 2, STONE_R, WHITE);
  DrawCircleLines(cx + STONE_R, row_y + ROW_H / 2, STONE_R, DARKGRAY);
  char w_buf[32];
  snprintf(w_buf, sizeof(w_buf), "%.1f", s.white_score);
  DrawText(w_buf, cx + STONE_R * 2 + 8, row_y + (ROW_H - FONT_SZ) / 2,
           FONT_SZ, Color{255, 255, 255, 255});
}

// ---------------------------------------------------------------------------
// Chat context builder
// ---------------------------------------------------------------------------

static std::string BuildGameContext(
    const go::Game& game, int gp_val, go::Stone human_color,
    const go::Board& setup_board, bool setup_mode,
    const katago::AnalysisResult& last_analysis, bool vision_active,
    int board_size, const std::vector<MoveRecord>& move_log) {
  // gp_val: 0=setup, 1=chooseColor, 2=playerTurn, 3=computerTurn,
  //         4=computerAnnounce, 5=placeComputer, 6=scoring
  if (gp_val <= 1) {
    return "Setting up a board position before playing.\nBoard:\n" +
           setup_board.ToString() + "\n";
  }
  const char* opponent_str =
      (human_color == go::Stone::kBlack) ? "Black" : "White";
  const char* your_str =
      (human_color == go::Stone::kBlack) ? "White" : "Black";

  std::string ctx;
  ctx += std::to_string(board_size) + "x" +
         std::to_string(board_size) + " Go game.\n";
  ctx += std::string("Your opponent plays ") + opponent_str +
         ", you play " + your_str + ".\n";
  ctx += "Move: " + std::to_string(game.MoveNumber() + 1) + "\n";
  ctx += "Captures: B=" +
         std::to_string(game.CapturedBy(go::Stone::kBlack)) + " W=" +
         std::to_string(game.CapturedBy(go::Stone::kWhite)) + "\n";

  switch (gp_val) {
    case 2:
      ctx += std::string("Your opponent (") + opponent_str +
             ") is thinking about their move.\n";
      break;
    case 3:
      ctx += "You're deciding on your next move.\n";
      break;
    case 4:
      ctx += "You're announcing your next move.\n";
      break;
    case 5:
      ctx += "You just played; board is updating.\n";
      break;
    case 6:
      ctx += "Game is over, scoring.\n";
      break;
    default:
      break;
  }

  // Current position analysis (presented as your read of the board).
  if (last_analysis.valid) {
    ctx += "\n--- Your read of the position ---\n";
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Black winrate=%.0f%% Score=%+.1f\n",
             last_analysis.winrate * 100.0, last_analysis.score_lead);
    ctx += buf;

    ctx += "Top moves: ";
    int n = std::min(3, static_cast<int>(last_analysis.moves.size()));
    for (int i = 0; i < n; i++) {
      const auto& m = last_analysis.moves[i];
      snprintf(buf, sizeof(buf), "%s(%.0f%%) ", m.gtp_move.c_str(),
               m.winrate * 100.0);
      ctx += buf;
    }
    ctx += "\n";
  }

  // Full move history with analysis.
  if (!move_log.empty()) {
    ctx += "\n--- Move History ---\n";
    ctx += "# | Player | Move | WR Before | WR After | Delta | Best Was | Quality\n";
    for (const auto& rec : move_log) {
      const char* who = (rec.color == go::Stone::kBlack) ? "B" : "W";
      bool is_human = (rec.color == human_color);
      char buf[256];
      if (rec.analysis_complete) {
        // Compute delta from mover's perspective.
        double delta;
        if (rec.color == go::Stone::kBlack) {
          delta = rec.winrate_after - rec.winrate_before;
        } else {
          delta = rec.winrate_before - rec.winrate_after;
        }
        const char* grade;
        if (rec.quality > 0.5f) grade = "good";
        else if (rec.quality > -0.2f) grade = "ok";
        else if (rec.quality > -0.6f) grade = "inaccuracy";
        else grade = "blunder";

        snprintf(buf, sizeof(buf),
                 "%d | %s%s | %s | %.0f%% | %.0f%% | %+.1f%% | %s | %s\n",
                 rec.move_number, who, is_human ? "(opponent)" : "(you)",
                 rec.gtp_move.c_str(),
                 rec.winrate_before * 100.0, rec.winrate_after * 100.0,
                 delta * 100.0, rec.top3_moves.c_str(), grade);
      } else {
        snprintf(buf, sizeof(buf),
                 "%d | %s%s | %s | %.0f%% | ... | ... | %s | ...\n",
                 rec.move_number, who, is_human ? "(opponent)" : "(you)",
                 rec.gtp_move.c_str(),
                 rec.winrate_before * 100.0, rec.top3_moves.c_str());
      }
      ctx += buf;
    }
  }

  ctx += "\nBoard:\n" + game.GetBoard().ToString() + "\n";
  return ctx;
}

// ---------------------------------------------------------------------------
// Last-move quality ring
// ---------------------------------------------------------------------------

// Draws a bold ring around the last move, colored by quality or plain blue.
// For passes (pos < 0), draws a colored quality dot in the status area.
static void DrawMoveQuality(const GameBoard& gb, int pos, float quality,
                            bool pending, bool show_winrate) {
  Color c;
  if (!show_winrate) {
    c = Color{80, 140, 255, 255};  // Plain blue when winrate highlighting off.
  } else {
    c = pending ? Color{128, 128, 128, 255} : QualityColor(quality);
  }

  if (pos >= 0) {
    // Board move: draw a thick ring.
    Vector2 p = GameBoardPos(gb, pos);
    float r = gb.piece_r + 4.0f;
    DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), r, c);
    DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), r + 1, c);
    DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), r + 2, c);
  } else {
    // Pass: draw a colored dot near the status bar.
    int dot_x = gb.win_w / 2 - 100;
    int dot_y = gb.status_y + 10;
    DrawCircle(dot_x, dot_y, 8, c);
    DrawCircleLines(dot_x, dot_y, 8, Color{40, 40, 40, 255});
  }
}

// ---------------------------------------------------------------------------
// Helpers for recording moves and requesting analysis
// ---------------------------------------------------------------------------

static void RecordAndAnalyze(std::vector<katago::HistoryMove>& history,
                             go::Stone color, int pos,
                             katago::Engine* katago) {
  history.push_back({color, pos});
  if (katago && katago->IsRunning()) {
    katago->RequestAnalysis(history, static_cast<int>(history.size()));
  }
}

// ---------------------------------------------------------------------------
// Pre-game settings screen
// ---------------------------------------------------------------------------

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
        }
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

// ---------------------------------------------------------------------------
// RunGame
// ---------------------------------------------------------------------------

void RunGame(ChatOverlay& chat, katago::Engine* katago,
             touchstone::VisionSystem* vision, const GameSettings& settings) {
  int mon_count = GetMonitorCount();
  int target_mon = (mon_count > 1) ? 1 : 0;

  CloseBoard();

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(1280, 720, "Touchstone");
  SetWindowMonitor(target_mon);
  MaximizeWindow();
  SetTargetFPS(60);
  SetExitKey(0);

  // Reload chat font after new window (OpenGL context changed).
  chat.LoadChatFont("fonts/Inter-Regular.ttf");

  // In-character opponent persona for the game.
  chat.SetSystemPrompt(
      "You are a Go player sitting across the board from your opponent. "
      "You're experienced and thoughtful — you take the game seriously but "
      "you're good company. When they talk to you, respond the way a "
      "player would over the board: share your impressions of the position, "
      "comment on interesting moves, talk about your plans or theirs. "
      "The position analysis in your context is your own reading of the "
      "board — refer to it naturally ('I think I'm a bit ahead', "
      "'that last move was sharp') without ever mentioning engines, bots, "
      "AI, software, or KataGo. You're just a person playing Go. "
      "Keep responses to 1-2 short sentences — brevity matters because "
      "your words are spoken aloud. Use Go terminology naturally. "
      "Coordinates use GTP format (e.g. E5, C3). "
      "Use plain text only — no markdown, no bold, no asterisks, no formatting.");

  int scr_w = GetScreenWidth();
  int scr_h = GetScreenHeight();

  std::srand(static_cast<unsigned>(std::time(nullptr)));

  const int BOARD_SZ = settings.board_size;
  GameBoard gb = CalcGameBoard(BOARD_SZ, scr_w, scr_h);

  // Tell KataGo the board size for this game.
  if (katago) katago->SetBoardSize(BOARD_SZ);

  enum class GP {
    kSetup,
    kChooseColor,
    kPlayerTurn,
    kComputerTurn,
    kComputerAnnounce,  // Waiting for TTS to finish announcing move.
    kPlaceComputer,
    kScoring
  };

  go::Board setup_board(BOARD_SZ);
  enum class SetupTool { kBlack, kWhite, kErase };
  SetupTool setup_tool = SetupTool::kBlack;
  go::Stone human_color = settings.human_color;

  go::Game game(BOARD_SZ);
  GP gp = settings.setup_mode ? GP::kSetup
          : (human_color == go::Stone::kWhite) ? GP::kComputerTurn
          : GP::kPlayerTurn;
  int think_frames = 0;

  bool vision_active = (vision != nullptr && vision->IsRunning());
  int pending_pos = -1;
  int detect_pos = -1;
  int detect_confirm = 0;
  const int DETECT_NEEDED = 5;
  int computer_move_pos = -1;
  int place_confirm = 0;
  const int PLACE_NEEDED = 5;
  int announce_move = -2;  // Move pending TTS announcement (-2 = none).

  std::vector<go::Territory> territories;
  go::ScoreResult score = {};

  // KataGo state.
  std::vector<katago::HistoryMove> move_history;
  katago::AnalysisResult last_analysis;
  bool katago_move_requested = false;
  bool computer_passed = false;
  bool show_ownership = false;
  bool show_top_moves = false;
  bool show_katago_stats = false;
  bool show_winrate_colors = true;
  bool show_atari = false;
  bool show_liberties = false;
  bool show_help = false;
  bool paused = false;

  // Per-color quality tracking: show rings for BOTH last black and last white.
  int last_black_pos = -1;           // Position of the last black move (-1=pass).
  int last_white_pos = -1;           // Position of the last white move (-1=pass).
  float black_quality = 0.0f;        // Quality of last black move.
  float white_quality = 0.0f;        // Quality of last white move.
  bool black_quality_pending = true;  // Waiting for analysis of last black move.
  bool white_quality_pending = true;  // Waiting for analysis of last white move.
  bool has_black_move = false;       // Whether black has played at least once.
  bool has_white_move = false;       // Whether white has played at least once.
  double prev_winrate = 0.5;        // Black winrate before the last move.
  double prev_score_lead = 0.0;     // Score lead before the last move.

  // Move-by-move analysis log for chat context.
  std::vector<MoveRecord> move_log;
  // Top 3 KataGo suggestions before the next move is played.
  std::string pre_move_top3;

  // Undo/redo stack: full snapshots of game state (GameSnapshot in game_mode.h).
  std::vector<GameSnapshot> undo_stack;
  std::vector<GameSnapshot> redo_stack;

  // Save/load state.
  std::string save_status_msg;
  double save_status_time = 0.0;
  enum class PauseScreen { kMain, kSave, kLoad };
  PauseScreen pause_screen = PauseScreen::kMain;
  char save_name_buf[128] = {};
  int save_name_len = 0;
  std::vector<touchstone::SaveInfo> save_list;
  int save_list_scroll = 0;

  // Helper: populate SaveData from current game state.
  auto BuildSaveData = [&](const std::string& name) -> touchstone::SaveData {
    touchstone::SaveData sd;
    sd.version = 1;
    sd.name = name;
    // ISO 8601 timestamp.
    std::time_t now = std::time(nullptr);
    char tbuf[32];
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%S",
                  std::localtime(&now));
    sd.timestamp = tbuf;
    sd.board_size = BOARD_SZ;
    sd.human_color = static_cast<int>(human_color);
    sd.human_sl_profile = settings.human_sl_profile;
    sd.move_history = move_history;
    sd.move_log = move_log;
    sd.prev_winrate = prev_winrate;
    sd.prev_score_lead = prev_score_lead;
    sd.last_black_pos = last_black_pos;
    sd.last_white_pos = last_white_pos;
    sd.black_quality = black_quality;
    sd.white_quality = white_quality;
    sd.black_quality_pending = black_quality_pending;
    sd.white_quality_pending = white_quality_pending;
    sd.has_black_move = has_black_move;
    sd.has_white_move = has_white_move;
    sd.pre_move_top3 = pre_move_top3;
    return sd;
  };

  // Helper: restore game state from SaveData via move replay.
  auto RestoreFromSave = [&](const touchstone::SaveData& sd) -> std::string {
    // Validate board size matches (can't change board size mid-game).
    if (sd.board_size != BOARD_SZ) {
      return "Save file board size (" + std::to_string(sd.board_size) +
             ") doesn't match current game (" + std::to_string(BOARD_SZ) + ")";
    }

    // Replay move history to reconstruct go::Game.
    go::Game new_game(sd.board_size);
    for (size_t i = 0; i < sd.move_history.size(); i++) {
      const auto& m = sd.move_history[i];
      // Ensure correct player is moving.
      if (new_game.CurrentPlayer() != m.color) {
        return "Move " + std::to_string(i + 1) +
               ": wrong player (expected " +
               (new_game.CurrentPlayer() == go::Stone::kBlack ? "Black"
                                                              : "White") +
               ")";
      }
      if (m.pos < 0) {
        new_game.Pass();
      } else {
        auto result = new_game.Play(m.pos);
        if (result != go::MoveResult::kOk) {
          return "Save file contains invalid move at move " +
                 std::to_string(i + 1);
        }
      }
    }

    // Success: replace all state.
    game = new_game;
    human_color = static_cast<go::Stone>(sd.human_color);
    move_history = sd.move_history;
    move_log = sd.move_log;
    prev_winrate = sd.prev_winrate;
    prev_score_lead = sd.prev_score_lead;
    last_black_pos = sd.last_black_pos;
    last_white_pos = sd.last_white_pos;
    black_quality = sd.black_quality;
    white_quality = sd.white_quality;
    black_quality_pending = sd.black_quality_pending;
    white_quality_pending = sd.white_quality_pending;
    has_black_move = sd.has_black_move;
    has_white_move = sd.has_white_move;
    pre_move_top3 = sd.pre_move_top3;

    // Clear undo/redo (stale).
    undo_stack.clear();
    redo_stack.clear();

    // Determine game phase.
    if (game.Phase() == go::GamePhase::kGameOver) {
      territories = go::FindTerritories(game.GetBoard());
      score = go::CalculateScore(game.GetBoard());
      gp = GP::kScoring;
    } else if (game.CurrentPlayer() == human_color) {
      gp = GP::kPlayerTurn;
    } else {
      gp = GP::kComputerTurn;
      katago_move_requested = false;
      think_frames = 0;
    }

    // Re-sync KataGo.
    if (katago && katago->IsRunning()) {
      katago->SetBoardSize(sd.board_size);
      katago->SetHumanSLProfile(sd.human_sl_profile);
      katago->RequestAnalysis(move_history,
                              static_cast<int>(move_history.size()));
    }

    return "";
  };

  // Chat context provider.
  chat.SetContextProvider(
      [&game, &gp, &human_color, &setup_board, &settings,
       &last_analysis, vision_active, BOARD_SZ, &move_log]() -> std::string {
        return BuildGameContext(game, static_cast<int>(gp), human_color,
                                setup_board, settings.setup_mode,
                                last_analysis, vision_active,
                                BOARD_SZ, move_log);
      });

  // Request initial analysis of the starting position.
  if (katago && katago->IsRunning()) {
    fprintf(stderr, "KataGo: requesting initial analysis (history=%d)\n",
            (int)move_history.size());
    katago->RequestAnalysis(move_history, static_cast<int>(move_history.size()));
  } else {
    fprintf(stderr, "KataGo: NOT requesting initial analysis (katago=%p, running=%d)\n",
            (void*)katago, katago ? katago->IsRunning() : false);
  }

  // Mic + speaker + winrate button positions (top-right of screen).
  constexpr float kMicRadius = 24.0f;
  constexpr float kSpkRadius = 20.0f;
  constexpr float kWrRadius = 20.0f;
  float mic_x = 0, mic_y = 0;
  float spk_x = 0, spk_y = 0;
  float wr_x = 0, wr_y = 0;

  while (!WindowShouldClose()) {
    bool chat_consumed = chat.HandleInput();

    // Mic + speaker button clicks — always available during game.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 mouse = GetMousePosition();
      if (mic_x > 0) {
        float dx = mouse.x - mic_x;
        float dy = mouse.y - mic_y;
        if (dx * dx + dy * dy <= kMicRadius * kMicRadius) {
          if (chat.IsVoiceRecording()) {
            chat.StopRecordingAndTranscribe();
          } else if (!chat.IsVoiceTranscribing()) {
            chat.StartRecording();
          }
        }
      }
      if (spk_x > 0) {
        float dx = mouse.x - spk_x;
        float dy = mouse.y - spk_y;
        if (dx * dx + dy * dy <= kSpkRadius * kSpkRadius) {
          chat.SetTTSEnabled(!chat.IsTTSEnabled());
        }
      }
      if (wr_x > 0) {
        float dx = mouse.x - wr_x;
        float dy = mouse.y - wr_y;
        if (dx * dx + dy * dy <= kWrRadius * kWrRadius) {
          show_winrate_colors = !show_winrate_colors;
        }
      }
    }

    // Poll KataGo each frame — drain all queued analysis results.
    if (katago && katago->IsRunning()) {
      for (;;) {
        auto analysis = katago->PollAnalysis();
        if (!analysis.has_value()) break;

        fprintf(stderr, "KataGo: got analysis turn=%d, valid=%d, moves=%d\n",
                analysis.value().turn_number, analysis.value().valid,
                (int)analysis.value().moves.size());

        if (analysis.value().valid) {
          double new_winrate = analysis.value().winrate;
          double new_score = analysis.value().score_lead;
          int turn = analysis.value().turn_number;

          // Find the MoveRecord that this analysis belongs to and compute
          // quality. Scan backward since recent moves are most likely.
          for (int mi = static_cast<int>(move_log.size()) - 1; mi >= 0; mi--) {
            auto& rec = move_log[mi];
            if (rec.analysis_complete) continue;
            if (rec.analysis_turn == turn) {
              rec.winrate_after = new_winrate;
              rec.score_after = new_score;
              // Compute delta from mover's perspective.
              double delta;
              if (rec.color == go::Stone::kBlack) {
                delta = new_winrate - rec.winrate_before;
              } else {
                delta = rec.winrate_before - new_winrate;
              }
              // Map delta to quality: +0.02 or more = great (+1),
              // -0.08 or worse = blunder (-1).
              float q = static_cast<float>((delta + 0.03) / 0.10);
              if (q > 1.0f) q = 1.0f;
              if (q < -1.0f) q = -1.0f;
              rec.quality = q;
              rec.analysis_complete = true;

              // Update the visual ring only if this is the most recent
              // move for this color (avoid stale updates from old queries).
              bool is_latest = true;
              for (int j = mi + 1; j < static_cast<int>(move_log.size()); j++) {
                if (move_log[j].color == rec.color) {
                  is_latest = false;
                  break;
                }
              }
              if (is_latest) {
                if (rec.color == go::Stone::kBlack) {
                  black_quality = q;
                  black_quality_pending = false;
                } else {
                  white_quality = q;
                  white_quality_pending = false;
                }
              }
              break;
            }
          }

          // Save winrate/score and top moves for next move's comparison.
          prev_winrate = new_winrate;
          prev_score_lead = new_score;
          pre_move_top3.clear();
          int nm = std::min(3, static_cast<int>(analysis.value().moves.size()));
          for (int i = 0; i < nm; i++) {
            if (i > 0) pre_move_top3 += " ";
            pre_move_top3 += analysis.value().moves[i].gtp_move;
          }
          if (pre_move_top3.empty()) pre_move_top3 = "?";
        }
        last_analysis = std::move(analysis.value());
      }
    } else if (katago) {
      // Log once if katago stopped running.
      static bool logged = false;
      if (!logged) {
        fprintf(stderr, "KataGo: not running (katago=%p, running=%d)\n",
                (void*)katago, katago->IsRunning());
        logged = true;
      }
    }

    // Toggle keys.
    if (!chat_consumed && !paused) {
      if (IsKeyPressed(KEY_O)) show_ownership = !show_ownership;
      if (IsKeyPressed(KEY_A)) show_top_moves = !show_top_moves;
      if (IsKeyPressed(KEY_K)) show_katago_stats = !show_katago_stats;
      if (IsKeyPressed(KEY_T)) show_atari = !show_atari;
      if (IsKeyPressed(KEY_G)) show_liberties = !show_liberties;
      if (IsKeyPressed(KEY_H)) show_help = !show_help;
      if (IsKeyPressed(KEY_ESCAPE)) paused = true;
    } else if (!chat_consumed && paused) {
      if (IsKeyPressed(KEY_ESCAPE)) {
        if (pause_screen != PauseScreen::kMain) {
          pause_screen = PauseScreen::kMain;
        } else {
          paused = false;
        }
      }
    }

    BeginDrawing();
    ClearBackground(Color{210, 180, 120, 255});
    DrawGameBoard(gb, game.GetBoard());

    // Live scoreboard: current area score.
    if (gp != GP::kSetup && gp != GP::kChooseColor) {
      DrawScoreboard(gb, game.GetBoard());
    }

    // Last move quality indicator: ring for both last black and last white.
    if (has_black_move) {
      DrawMoveQuality(gb, last_black_pos, black_quality, black_quality_pending,
                      show_winrate_colors);
    }
    if (has_white_move) {
      DrawMoveQuality(gb, last_white_pos, white_quality, white_quality_pending,
                      show_winrate_colors);
    }

    // KataGo overlays.
    if (show_ownership) DrawOwnershipOverlay(gb, last_analysis);
    if (show_top_moves && gp == GP::kPlayerTurn) {
      DrawTopMoves(gb, last_analysis);
    }
    if (show_katago_stats) {
      go::Stone current = game.CurrentPlayer();
      DrawKataGoStatsPanel(gb, last_analysis, current, move_log, human_color);
    }
    if (show_atari) DrawAtariIndicator(gb, game.GetBoard());
    if (show_liberties) DrawLibertyCount(gb, game.GetBoard(), show_atari);
    if (show_help) DrawHelpScreen(scr_w, scr_h);

    if (paused) {
      // Darken the board.
      DrawRectangle(0, 0, scr_w, scr_h, Color{0, 0, 0, 160});
      Vector2 mouse = GetMousePosition();

      if (pause_screen == PauseScreen::kMain) {
        // Main pause panel.
        const int panel_w = 300;
        const int panel_h = 310;
        int px = (scr_w - panel_w) / 2;
        int py = (scr_h - panel_h) / 2;
        DrawRectangle(px, py, panel_w, panel_h, Color{30, 30, 30, 240});
        DrawRectangleLinesEx(
            Rectangle{(float)px, (float)py, (float)panel_w, (float)panel_h},
            2, Color{100, 100, 100, 255});

        const char* title = "PAUSED";
        int tw = MeasureText(title, 30);
        DrawText(title, px + (panel_w - tw) / 2, py + 20, 30, RAYWHITE);

        // Resume button.
        Rectangle resume_btn = {(float)(px + 40), (float)(py + 70), 220, 40};
        bool r_hover = CheckCollisionPointRec(mouse, resume_btn);
        DrawRectangleRec(resume_btn, r_hover ? Color{60, 80, 60, 255}
                                             : Color{40, 55, 40, 255});
        DrawRectangleLinesEx(resume_btn, 1, Color{100, 200, 100, 255});
        const char* r_label = "Resume  [ESC]";
        int rw = MeasureText(r_label, 20);
        DrawText(r_label, (int)(resume_btn.x + (resume_btn.width - rw) / 2),
                 (int)(resume_btn.y + 10), 20, Color{180, 255, 180, 255});
        if (r_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          paused = false;
        }

        // Save Game button.
        Rectangle save_btn = {(float)(px + 40), (float)(py + 125), 220, 40};
        bool s_hover = CheckCollisionPointRec(mouse, save_btn);
        DrawRectangleRec(save_btn, s_hover ? Color{50, 60, 80, 255}
                                           : Color{35, 45, 60, 255});
        DrawRectangleLinesEx(save_btn, 1, Color{100, 150, 220, 255});
        const char* s_label = "Save Game";
        int sw = MeasureText(s_label, 20);
        DrawText(s_label, (int)(save_btn.x + (save_btn.width - sw) / 2),
                 (int)(save_btn.y + 10), 20, Color{180, 200, 255, 255});
        if (s_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          pause_screen = PauseScreen::kSave;
          save_list = touchstone::ListSaves();
          // Pre-fill save name.
          snprintf(save_name_buf, sizeof(save_name_buf), "%dx%d Game - %d moves",
                   BOARD_SZ, BOARD_SZ, (int)move_history.size());
          save_name_len = static_cast<int>(strlen(save_name_buf));
        }

        // Load Game button.
        Rectangle load_btn = {(float)(px + 40), (float)(py + 180), 220, 40};
        bool l_hover = CheckCollisionPointRec(mouse, load_btn);
        DrawRectangleRec(load_btn, l_hover ? Color{50, 60, 80, 255}
                                           : Color{35, 45, 60, 255});
        DrawRectangleLinesEx(load_btn, 1, Color{100, 150, 220, 255});
        const char* l_label = "Load Game";
        int lw = MeasureText(l_label, 20);
        DrawText(l_label, (int)(load_btn.x + (load_btn.width - lw) / 2),
                 (int)(load_btn.y + 10), 20, Color{180, 200, 255, 255});
        if (l_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          pause_screen = PauseScreen::kLoad;
          save_list = touchstone::ListSaves();
          save_list_scroll = 0;
        }

        // Quit button.
        Rectangle quit_btn = {(float)(px + 40), (float)(py + 245), 220, 40};
        bool q_hover = CheckCollisionPointRec(mouse, quit_btn);
        DrawRectangleRec(quit_btn, q_hover ? Color{80, 50, 50, 255}
                                           : Color{55, 35, 35, 255});
        DrawRectangleLinesEx(quit_btn, 1, Color{200, 100, 100, 255});
        const char* q_label = "Quit to Menu";
        int qw = MeasureText(q_label, 20);
        DrawText(q_label, (int)(quit_btn.x + (quit_btn.width - qw) / 2),
                 (int)(quit_btn.y + 10), 20, Color{255, 160, 160, 255});
        if (q_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          goto done;
        }

      } else if (pause_screen == PauseScreen::kSave) {
        // Save sub-panel.
        const int panel_w = 400;
        const int panel_h = 420;
        int px = (scr_w - panel_w) / 2;
        int py = (scr_h - panel_h) / 2;
        DrawRectangle(px, py, panel_w, panel_h, Color{30, 30, 30, 240});
        DrawRectangleLinesEx(
            Rectangle{(float)px, (float)py, (float)panel_w, (float)panel_h},
            2, Color{100, 100, 100, 255});

        const char* title = "SAVE GAME";
        int tw = MeasureText(title, 24);
        DrawText(title, px + (panel_w - tw) / 2, py + 15, 24, RAYWHITE);

        // Save name input.
        DrawText("Save name:", px + 20, py + 55, 16, Color{180, 180, 180, 255});
        Rectangle name_box = {(float)(px + 20), (float)(py + 75), 360, 30};
        DrawRectangleRec(name_box, Color{50, 50, 50, 255});
        DrawRectangleLinesEx(name_box, 1, Color{120, 120, 120, 255});
        DrawText(save_name_buf, px + 25, py + 81, 16, RAYWHITE);

        // Handle text input for save name.
        int key = GetCharPressed();
        while (key > 0) {
          if (key >= 32 && key < 127 && save_name_len < 120) {
            save_name_buf[save_name_len++] = static_cast<char>(key);
            save_name_buf[save_name_len] = '\0';
          }
          key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && save_name_len > 0) {
          save_name_buf[--save_name_len] = '\0';
        }

        // Save button.
        Rectangle save_confirm = {(float)(px + 20), (float)(py + 115), 170, 35};
        bool sc_hover = CheckCollisionPointRec(mouse, save_confirm);
        DrawRectangleRec(save_confirm, sc_hover ? Color{60, 80, 60, 255}
                                                : Color{40, 55, 40, 255});
        DrawRectangleLinesEx(save_confirm, 1, Color{100, 200, 100, 255});
        const char* sc_label = "Save";
        int scw = MeasureText(sc_label, 18);
        DrawText(sc_label,
                 (int)(save_confirm.x + (save_confirm.width - scw) / 2),
                 (int)(save_confirm.y + 9), 18, Color{180, 255, 180, 255});
        if (sc_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            save_name_len > 0) {
          // Sanitize filename: replace non-alphanumeric with _.
          std::string fname;
          for (int i = 0; i < save_name_len; i++) {
            char c = save_name_buf[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == ' ') {
              fname += c;
            } else {
              fname += '_';
            }
          }
          std::string dir = touchstone::GetSaveDirectory();
          std::string path = dir + "/" + fname + ".json";
          auto sd = BuildSaveData(save_name_buf);
          std::string err = touchstone::SaveGame(path, sd);
          save_status_msg = err.empty() ? "Game saved!" : err;
          save_status_time = GetTime();
          pause_screen = PauseScreen::kMain;
          paused = false;
        }

        // Back button.
        Rectangle back_btn = {(float)(px + 210), (float)(py + 115), 170, 35};
        bool b_hover = CheckCollisionPointRec(mouse, back_btn);
        DrawRectangleRec(back_btn, b_hover ? Color{60, 60, 60, 255}
                                           : Color{45, 45, 45, 255});
        DrawRectangleLinesEx(back_btn, 1, Color{150, 150, 150, 255});
        DrawText("Back", (int)(back_btn.x + 65), (int)(back_btn.y + 9), 18,
                 Color{200, 200, 200, 255});
        if (b_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          pause_screen = PauseScreen::kMain;
        }

        // Existing saves list (for overwrite).
        DrawText("Existing saves (click to overwrite):", px + 20, py + 165, 14,
                 Color{150, 150, 150, 255});
        int list_y = py + 185;
        int max_visible = 5;
        for (int i = save_list_scroll;
             i < static_cast<int>(save_list.size()) &&
             i < save_list_scroll + max_visible;
             i++) {
          const auto& si = save_list[i];
          int row_y = list_y + (i - save_list_scroll) * 42;
          Rectangle row = {(float)(px + 20), (float)row_y, 360, 38};
          bool row_hover = CheckCollisionPointRec(mouse, row);
          DrawRectangleRec(row, row_hover ? Color{55, 55, 65, 255}
                                         : Color{40, 40, 48, 255});
          DrawRectangleLinesEx(row, 1, Color{80, 80, 90, 255});

          // Name and metadata.
          DrawText(si.display_name.c_str(), px + 28, row_y + 4, 16, RAYWHITE);
          char meta[64];
          snprintf(meta, sizeof(meta), "%dx%d  %d moves  %s",
                   si.board_size, si.board_size, si.move_count,
                   si.timestamp.substr(0, 10).c_str());
          DrawText(meta, px + 28, row_y + 22, 12,
                   Color{140, 140, 140, 255});

          if (row_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            auto sd = BuildSaveData(si.display_name);
            std::string err = touchstone::SaveGame(si.filepath, sd);
            save_status_msg = err.empty() ? "Game saved!" : err;
            save_status_time = GetTime();
            pause_screen = PauseScreen::kMain;
            paused = false;
          }
        }
        // Scroll with mouse wheel.
        int wheel = static_cast<int>(GetMouseWheelMove());
        if (wheel != 0) {
          save_list_scroll -= wheel;
          if (save_list_scroll < 0) save_list_scroll = 0;
          int max_scroll = std::max(0, static_cast<int>(save_list.size()) - max_visible);
          if (save_list_scroll > max_scroll) save_list_scroll = max_scroll;
        }

      } else if (pause_screen == PauseScreen::kLoad) {
        // Load sub-panel.
        const int panel_w = 400;
        const int panel_h = 420;
        int px = (scr_w - panel_w) / 2;
        int py = (scr_h - panel_h) / 2;
        DrawRectangle(px, py, panel_w, panel_h, Color{30, 30, 30, 240});
        DrawRectangleLinesEx(
            Rectangle{(float)px, (float)py, (float)panel_w, (float)panel_h},
            2, Color{100, 100, 100, 255});

        const char* title = "LOAD GAME";
        int tw = MeasureText(title, 24);
        DrawText(title, px + (panel_w - tw) / 2, py + 15, 24, RAYWHITE);

        // Back button.
        Rectangle back_btn = {(float)(px + 20), (float)(py + 50), 100, 30};
        bool b_hover = CheckCollisionPointRec(mouse, back_btn);
        DrawRectangleRec(back_btn, b_hover ? Color{60, 60, 60, 255}
                                           : Color{45, 45, 45, 255});
        DrawRectangleLinesEx(back_btn, 1, Color{150, 150, 150, 255});
        DrawText("Back", (int)(back_btn.x + 30), (int)(back_btn.y + 7), 16,
                 Color{200, 200, 200, 255});
        if (b_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          pause_screen = PauseScreen::kMain;
        }

        if (save_list.empty()) {
          DrawText("No saved games found.", px + 20, py + 100, 16,
                   Color{150, 150, 150, 255});
        } else {
          int list_y = py + 90;
          int max_visible = 7;
          for (int i = save_list_scroll;
               i < static_cast<int>(save_list.size()) &&
               i < save_list_scroll + max_visible;
               i++) {
            const auto& si = save_list[i];
            int row_y = list_y + (i - save_list_scroll) * 44;
            Rectangle row = {(float)(px + 20), (float)row_y, 320, 40};
            bool row_hover = CheckCollisionPointRec(mouse, row);
            DrawRectangleRec(row, row_hover ? Color{55, 55, 65, 255}
                                           : Color{40, 40, 48, 255});
            DrawRectangleLinesEx(row, 1, Color{80, 80, 90, 255});

            DrawText(si.display_name.c_str(), px + 28, row_y + 4, 16,
                     RAYWHITE);
            char meta[64];
            snprintf(meta, sizeof(meta), "%dx%d  %d moves  %s",
                     si.board_size, si.board_size, si.move_count,
                     si.timestamp.substr(0, 10).c_str());
            DrawText(meta, px + 28, row_y + 22, 12,
                     Color{140, 140, 140, 255});

            // Click to load.
            if (row_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
              touchstone::SaveData sd;
              std::string err = touchstone::LoadGame(si.filepath, sd);
              if (err.empty()) {
                err = RestoreFromSave(sd);
              }
              save_status_msg = err.empty() ? "Game loaded!" : err;
              save_status_time = GetTime();
              if (err.empty()) {
                pause_screen = PauseScreen::kMain;
                paused = false;
              }
            }

            // Delete button (X).
            Rectangle del_btn = {(float)(px + 345), (float)(row_y + 8), 24, 24};
            bool d_hover = CheckCollisionPointRec(mouse, del_btn);
            DrawRectangleRec(del_btn, d_hover ? Color{100, 40, 40, 255}
                                              : Color{60, 30, 30, 255});
            DrawRectangleLinesEx(del_btn, 1, Color{180, 80, 80, 255});
            DrawText("X", (int)(del_btn.x + 7), (int)(del_btn.y + 4), 14,
                     Color{255, 150, 150, 255});
            if (d_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
              touchstone::DeleteSave(si.filepath);
              save_list = touchstone::ListSaves();
              if (save_list_scroll >= static_cast<int>(save_list.size())) {
                save_list_scroll = std::max(0, static_cast<int>(save_list.size()) - 1);
              }
            }
          }
          // Scroll with mouse wheel.
          int wheel = static_cast<int>(GetMouseWheelMove());
          if (wheel != 0) {
            save_list_scroll -= wheel;
            if (save_list_scroll < 0) save_list_scroll = 0;
            int max_scroll = std::max(0, static_cast<int>(save_list.size()) - max_visible);
            if (save_list_scroll > max_scroll) save_list_scroll = max_scroll;
          }
        }
      }
    }

    // Save/load status flash message.
    if (!save_status_msg.empty()) {
      double elapsed = GetTime() - save_status_time;
      if (elapsed < 2.5) {
        unsigned char alpha = 255;
        if (elapsed > 1.5) {
          alpha = static_cast<unsigned char>(255 * (2.5 - elapsed));
        }
        int msg_w = MeasureText(save_status_msg.c_str(), 20);
        int msg_x = (scr_w - msg_w) / 2;
        int msg_y = scr_h - 60;
        DrawRectangle(msg_x - 10, msg_y - 5, msg_w + 20, 30,
                      Color{30, 30, 30, alpha});
        bool is_error = save_status_msg.find("Game") == std::string::npos;
        Color msg_color = is_error ? Color{255, 150, 150, alpha}
                                   : Color{150, 255, 150, alpha};
        DrawText(save_status_msg.c_str(), msg_x, msg_y, 20, msg_color);
      } else {
        save_status_msg.clear();
      }
    }

    // Lambda: finalize the computer's move (pass or board play).
    // Called after TTS announcement finishes, or immediately when TTS is off.
    auto PlaceComputerMove = [&](int move) {
      go::Stone comp_color = go::Opponent(human_color);
      if (move < 0) {
        game.Pass();
        move_history.push_back({comp_color, -1});
        if (comp_color == go::Stone::kBlack) {
          last_black_pos = -1;
          black_quality = 0.0f;
          black_quality_pending = true;
          has_black_move = true;
        } else {
          last_white_pos = -1;
          white_quality = 0.0f;
          white_quality_pending = true;
          has_white_move = true;
        }
        move_log.push_back({static_cast<int>(move_log.size()) + 1,
            comp_color, "pass",
            prev_winrate, 0, prev_score_lead, 0,
            pre_move_top3, 0, false,
            static_cast<int>(move_history.size())});
        if (katago && katago->IsRunning()) {
          katago->RequestAnalysis(
              move_history, static_cast<int>(move_history.size()));
        }
        computer_passed = true;
        if (game.Phase() == go::GamePhase::kGameOver) {
          territories = go::FindTerritories(game.GetBoard());
          score = go::CalculateScore(game.GetBoard());
          gp = GP::kScoring;
        } else {
          if (vision_active) {
            computer_move_pos = -1;
            gp = GP::kPlaceComputer;
          } else {
            gp = GP::kPlayerTurn;
          }
        }
      } else {
        computer_passed = false;
        if (comp_color == go::Stone::kBlack) {
          last_black_pos = move;
          black_quality = 0.0f;
          black_quality_pending = true;
          has_black_move = true;
        } else {
          last_white_pos = move;
          white_quality = 0.0f;
          white_quality_pending = true;
          has_white_move = true;
        }
        move_log.push_back({static_cast<int>(move_log.size()) + 1,
            comp_color,
            katago::PosToGtp(move, BOARD_SZ),
            prev_winrate, 0, prev_score_lead, 0,
            pre_move_top3, 0, false,
            static_cast<int>(move_history.size()) + 1});
        game.Play(move);
        move_history.push_back({comp_color, move});
        if (katago && katago->IsRunning()) {
          katago->RequestAnalysis(
              move_history, static_cast<int>(move_history.size()));
        }
        if (vision_active) {
          computer_move_pos = move;
          gp = GP::kPlaceComputer;
        } else {
          gp = GP::kPlayerTurn;
        }
      }
    };

    if (!paused) switch (gp) {
      case GP::kSetup: {
        if (!chat_consumed) {
          if (IsKeyPressed(KEY_B)) setup_tool = SetupTool::kBlack;
          if (IsKeyPressed(KEY_W)) setup_tool = SetupTool::kWhite;
          if (IsKeyPressed(KEY_E)) setup_tool = SetupTool::kErase;
          if (IsKeyPressed(KEY_SPACE)) {
            gp = GP::kChooseColor;
            break;
          }
        }

        {
          int clicked = GameBoardClick(gb);
          if (clicked >= 0) {
            switch (setup_tool) {
              case SetupTool::kBlack:
                setup_board.Set(clicked, go::Stone::kBlack);
                break;
              case SetupTool::kWhite:
                setup_board.Set(clicked, go::Stone::kWhite);
                break;
              case SetupTool::kErase:
                setup_board.Set(clicked, go::Stone::kEmpty);
                break;
            }
          }
        }

        // Draw setup stones over the empty game board.
        {
          int n = setup_board.NumPositions();
          for (int i = 0; i < n; i++) {
            if (setup_board.At(i) == go::Stone::kEmpty) continue;
            Vector2 p = GameBoardPos(gb, i);
            if (setup_board.At(i) == go::Stone::kBlack) {
              DrawCircle(p.x, p.y, gb.piece_r, BLACK);
            } else {
              DrawCircle(p.x, p.y, gb.piece_r, WHITE);
              DrawCircleLines(p.x, p.y, gb.piece_r, DARKGRAY);
            }
          }
        }

        {
          const char* tool_name =
              (setup_tool == SetupTool::kBlack)  ? "Black"
              : (setup_tool == SetupTool::kWhite) ? "White"
                                                   : "Erase";
          char status[128];
          snprintf(
              status, sizeof(status),
              "Setup [%s]  B/W/E=color  SPACE=play  ESC=cancel",
              tool_name);
          DrawGameStatus(gb, status);
        }
        break;
      }

      case GP::kChooseColor: {
        {
          int n = setup_board.NumPositions();
          for (int i = 0; i < n; i++) {
            if (setup_board.At(i) == go::Stone::kEmpty) continue;
            Vector2 p = GameBoardPos(gb, i);
            if (setup_board.At(i) == go::Stone::kBlack) {
              DrawCircle(p.x, p.y, gb.piece_r, BLACK);
            } else {
              DrawCircle(p.x, p.y, gb.piece_r, WHITE);
              DrawCircleLines(p.x, p.y, gb.piece_r, DARKGRAY);
            }
          }
        }

        DrawGameStatus(gb, "Play as:  B=Black  W=White  ESC=back");

        if (!chat_consumed) {
          bool chosen = false;
          if (IsKeyPressed(KEY_B)) {
            human_color = go::Stone::kBlack;
            chosen = true;
          } else if (IsKeyPressed(KEY_W)) {
            human_color = go::Stone::kWhite;
            chosen = true;
          }
          if (chosen) {
            game = go::Game(setup_board, human_color);
            gp = GP::kPlayerTurn;
          }
        }
        break;
      }

      case GP::kPlayerTurn: {
        bool showing_confirm = false;

        if (vision_active) {
          if (pending_pos >= 0) {
            Vector2 pp = GameBoardPos(gb, pending_pos);
            if (human_color == go::Stone::kBlack) {
              DrawCircle(pp.x, pp.y, gb.piece_r, BLACK);
            } else {
              DrawCircle(pp.x, pp.y, gb.piece_r, WHITE);
              DrawCircleLines(pp.x, pp.y, gb.piece_r, DARKGRAY);
            }
            DrawCircleLines(pp.x, pp.y, gb.piece_r + 3, GREEN);

            auto det = vision->GetLatestDetection();
            auto expected_vision_color =
                (human_color == go::Stone::kBlack)
                    ? touchstone::StoneColor::kBlack
                    : touchstone::StoneColor::kWhite;
            bool still_there =
                det.board_found &&
                pending_pos < (int)det.board.size() &&
                det.board[pending_pos] == expected_vision_color;

            if (!still_there) {
              pending_pos = -1;
            } else {
              showing_confirm = true;
              DrawGameStatus(
                  gb, "Move detected. Press SPACE to confirm.  [ESC=quit]");

              if (IsKeyPressed(KEY_SPACE)) {
                // Save undo snapshot before playing.
                GameSnapshot snap = {game, move_history,
                    move_log, prev_winrate,
                    prev_score_lead, last_black_pos, last_white_pos,
                    black_quality, white_quality,
                    black_quality_pending, white_quality_pending,
                    has_black_move, has_white_move, pre_move_top3};
                auto result = game.Play(pending_pos);
                if (result == go::MoveResult::kOk) {
                  undo_stack.push_back(std::move(snap));
                  redo_stack.clear();  // New move = fork, discard redo.
                  if (human_color == go::Stone::kBlack) {
                    last_black_pos = pending_pos;
                    black_quality = 0.0f;
                    black_quality_pending = true;
                    has_black_move = true;
                  } else {
                    last_white_pos = pending_pos;
                    white_quality = 0.0f;
                    white_quality_pending = true;
                    has_white_move = true;
                  }
                  move_log.push_back({static_cast<int>(move_log.size()) + 1,
                      human_color,
                      katago::PosToGtp(pending_pos, BOARD_SZ),
                      prev_winrate, 0, prev_score_lead, 0,
                      pre_move_top3, 0, false,
                      static_cast<int>(move_history.size()) + 1});
                  RecordAndAnalyze(move_history, human_color, pending_pos,
                                   katago);
                  gp = GP::kComputerTurn;
                  think_frames = 0;
                  katago_move_requested = false;
                }
                pending_pos = -1;
                detect_pos = -1;
                detect_confirm = 0;
              }
            }
          }

          if (pending_pos < 0) {
            auto det = vision->GetLatestDetection();
            auto human_vision_color =
                (human_color == go::Stone::kBlack)
                    ? touchstone::StoneColor::kBlack
                    : touchstone::StoneColor::kWhite;
            if (det.board_found) {
              auto mismatches = FindBoardMismatches(game.GetBoard(), det);

              std::vector<int> real_mismatches;
              bool allowed_one = false;
              for (int pos : mismatches) {
                if (!allowed_one &&
                    game.GetBoard().At(pos) == go::Stone::kEmpty &&
                    det.board[pos] == human_vision_color) {
                  allowed_one = true;
                  continue;
                }
                real_mismatches.push_back(pos);
              }

              if (!real_mismatches.empty()) {
                for (int pos : real_mismatches) {
                  Vector2 p = GameBoardPos(gb, pos);
                  DrawCircleLines(p.x, p.y, gb.piece_r + 3, RED);
                }
                showing_confirm = true;
                DrawGameStatus(
                    gb,
                    "Board mismatch! Fix the board before moving.  [ESC=quit]");
                detect_pos = -1;
                detect_confirm = 0;
              } else {
                int n = game.GetBoard().NumPositions();
                int new_pos = -1;
                int diff_count = 0;
                for (int i = 0; i < n && i < (int)det.board.size(); i++) {
                  if (game.GetBoard().At(i) == go::Stone::kEmpty &&
                      det.board[i] == human_vision_color) {
                    new_pos = i;
                    diff_count++;
                  }
                }
                if (diff_count == 1) {
                  if (new_pos == detect_pos) {
                    detect_confirm++;
                    if (detect_confirm >= DETECT_NEEDED) {
                      auto vr = go::ValidateMove(game.GetBoard(), new_pos,
                                                  human_color,
                                                  game.PreviousBoard());
                      if (vr == go::MoveResult::kOk) {
                        pending_pos = new_pos;
                      }
                      detect_pos = -1;
                      detect_confirm = 0;
                    }
                  } else {
                    detect_pos = new_pos;
                    detect_confirm = 1;
                  }
                } else {
                  detect_pos = -1;
                  detect_confirm = 0;
                }
              }
            }
          }
        }

        if (!showing_confirm) {
          const char* hc =
              (human_color == go::Stone::kBlack) ? "Black" : "White";
          char status[192];
          snprintf(
              status, sizeof(status),
              "%s%s to play. Move %d. Captures: B=%d W=%d  [H=help]",
              computer_passed ? "Bot passed! " : "",
              hc, game.MoveNumber() + 1,
              game.CapturedBy(go::Stone::kBlack),
              game.CapturedBy(go::Stone::kWhite));
          DrawGameStatus(gb, status);

          if (!chat_consumed && IsKeyPressed(KEY_P)) {
            undo_stack.push_back({game, move_history,
                move_log, prev_winrate,
                prev_score_lead, last_black_pos, last_white_pos,
                black_quality, white_quality,
                black_quality_pending, white_quality_pending,
                has_black_move, has_white_move, pre_move_top3});
            redo_stack.clear();  // New move = fork, discard redo.
            game.Pass();
            if (human_color == go::Stone::kBlack) {
              last_black_pos = -1;  // Pass — no ring to draw.
              black_quality_pending = true;
              has_black_move = true;
            } else {
              last_white_pos = -1;
              white_quality_pending = true;
              has_white_move = true;
            }
            move_log.push_back({static_cast<int>(move_log.size()) + 1,
                human_color, "pass",
                prev_winrate, 0, prev_score_lead, 0,
                pre_move_top3, 0, false,
                static_cast<int>(move_history.size()) + 1});
            RecordAndAnalyze(move_history, human_color, -1, katago);
            if (game.Phase() == go::GamePhase::kGameOver) {
              territories = go::FindTerritories(game.GetBoard());
              score = go::CalculateScore(game.GetBoard());
              gp = GP::kScoring;
            } else {
              gp = GP::kComputerTurn;
              think_frames = 0;
              katago_move_requested = false;
            }
            break;
          }

          // Undo: Backspace restores to before the last human move.
          if (!chat_consumed && IsKeyPressed(KEY_BACKSPACE) && !undo_stack.empty()) {
            // Save current state to redo stack before restoring.
            redo_stack.push_back({game, move_history,
                move_log, prev_winrate,
                prev_score_lead, last_black_pos, last_white_pos,
                black_quality, white_quality,
                black_quality_pending, white_quality_pending,
                has_black_move, has_white_move, pre_move_top3});
            auto& snap = undo_stack.back();
            game = snap.game;
            move_history = snap.move_history;
            move_log = snap.move_log;
            prev_winrate = snap.winrate;
            prev_score_lead = snap.score_lead;
            last_black_pos = snap.black_pos;
            last_white_pos = snap.white_pos;
            black_quality = snap.b_quality;
            white_quality = snap.w_quality;
            black_quality_pending = snap.b_quality_pending;
            white_quality_pending = snap.w_quality_pending;
            has_black_move = snap.has_black;
            has_white_move = snap.has_white;
            pre_move_top3 = snap.top3;
            computer_passed = false;
            undo_stack.pop_back();
            // Re-request analysis for the restored position.
            if (katago && katago->IsRunning()) {
              katago->RequestAnalysis(
                  move_history, static_cast<int>(move_history.size()));
            }
            break;
          }

          // Redo: R restores the next undone state (until a fork is taken).
          if (!chat_consumed && IsKeyPressed(KEY_R) && !redo_stack.empty()) {
            // Save current state to undo stack before restoring.
            undo_stack.push_back({game, move_history,
                move_log, prev_winrate,
                prev_score_lead, last_black_pos, last_white_pos,
                black_quality, white_quality,
                black_quality_pending, white_quality_pending,
                has_black_move, has_white_move, pre_move_top3});
            auto& snap = redo_stack.back();
            game = snap.game;
            move_history = snap.move_history;
            move_log = snap.move_log;
            prev_winrate = snap.winrate;
            prev_score_lead = snap.score_lead;
            last_black_pos = snap.black_pos;
            last_white_pos = snap.white_pos;
            black_quality = snap.b_quality;
            white_quality = snap.w_quality;
            black_quality_pending = snap.b_quality_pending;
            white_quality_pending = snap.w_quality_pending;
            has_black_move = snap.has_black;
            has_white_move = snap.has_white;
            pre_move_top3 = snap.top3;
            computer_passed = false;
            redo_stack.pop_back();
            // Re-request analysis for the restored position.
            if (katago && katago->IsRunning()) {
              katago->RequestAnalysis(
                  move_history, static_cast<int>(move_history.size()));
            }
            break;
          }

          int clicked = chat_consumed ? -1 : GameBoardClick(gb);
          if (clicked >= 0) {
            // Save undo snapshot before playing.
            GameSnapshot snap = {game, move_history,
                move_log, prev_winrate,
                prev_score_lead, last_black_pos, last_white_pos,
                black_quality, white_quality,
                black_quality_pending, white_quality_pending,
                has_black_move, has_white_move, pre_move_top3};
            auto result = game.Play(clicked);
            if (result == go::MoveResult::kOk) {
              undo_stack.push_back(std::move(snap));
              redo_stack.clear();  // New move = fork, discard redo.
              if (human_color == go::Stone::kBlack) {
                last_black_pos = clicked;
                black_quality = 0.0f;
                black_quality_pending = true;
                has_black_move = true;
              } else {
                last_white_pos = clicked;
                white_quality = 0.0f;
                white_quality_pending = true;
                has_white_move = true;
              }
              move_log.push_back({static_cast<int>(move_log.size()) + 1,
                  human_color,
                  katago::PosToGtp(clicked, BOARD_SZ),
                  prev_winrate, 0, prev_score_lead, 0,
                  pre_move_top3, 0, false,
                  static_cast<int>(move_history.size()) + 1});
              RecordAndAnalyze(move_history, human_color, clicked, katago);
              gp = GP::kComputerTurn;
              think_frames = 0;
              katago_move_requested = false;
            }
          }
        }
        break;
      }

      case GP::kComputerTurn: {
        DrawGameStatus(gb, "Thinking...");

        // Use KataGo for computer move if available.
        if (katago && katago->IsRunning()) {
          if (!katago_move_requested) {
            katago->SetHumanSLProfile(settings.human_sl_profile);
            katago->RequestBestMove(move_history,
                                    static_cast<int>(move_history.size()));
            katago_move_requested = true;
          }

          auto best = katago->PollBestMove();
          if (best.has_value()) {
            katago_move_requested = false;
            int move = best.value();

            if (chat.IsTTSEnabled()) {
              // Announce the move via TTS before placing the stone.
              std::string announcement;
              if (move < 0) {
                announcement = "I pass.";
              } else {
                announcement = "I play at " +
                    katago::PosToGtp(move, BOARD_SZ) + ".";
              }
              chat.SpeakText(announcement);
              announce_move = move;
              gp = GP::kComputerAnnounce;
            } else {
              PlaceComputerMove(move);
            }
          }
        } else {
          // KataGo is required — should never reach here.
          fprintf(stderr, "FATAL: KataGo not available for computer turn.\n");
          DrawGameStatus(gb, "ERROR: KataGo not running!");
        }
        break;
      }

      case GP::kComputerAnnounce: {
        DrawGameStatus(gb, "Speaking...");
        if (!chat.IsTTSSpeaking()) {
          PlaceComputerMove(announce_move);
          announce_move = -2;
        }
        break;
      }

      case GP::kPlaceComputer: {
        if (computer_move_pos >= 0) {
          Vector2 cp = GameBoardPos(gb, computer_move_pos);
          DrawCircleLines(cp.x, cp.y, gb.piece_r + 3, RED);

          auto det = vision->GetLatestDetection();
          auto mismatches = FindBoardMismatches(game.GetBoard(), det);

          for (int pos : mismatches) {
            if (pos == computer_move_pos) continue;
            Vector2 p = GameBoardPos(gb, pos);
            DrawCircleLines(p.x, p.y, gb.piece_r + 3, ORANGE);
          }

          if (det.board_found && mismatches.empty()) {
            place_confirm++;
            if (place_confirm >= PLACE_NEEDED) {
              computer_move_pos = -1;
              place_confirm = 0;
              detect_pos = -1;
              detect_confirm = 0;
              gp = GP::kPlayerTurn;
            }
          } else {
            place_confirm = 0;
          }

          int row = computer_move_pos / 9;
          int col = computer_move_pos % 9;
          char status[128];
          const char* comp_color_name =
              (human_color == go::Stone::kBlack) ? "white" : "black";
          bool has_other_issues = false;
          for (int pos : mismatches) {
            if (pos != computer_move_pos) {
              has_other_issues = true;
              break;
            }
          }
          if (has_other_issues) {
            snprintf(status, sizeof(status),
                     "Computer plays (%d,%d). Update board to match.",
                     row + 1, col + 1);
          } else {
            snprintf(status, sizeof(status),
                     "Computer plays at (%d,%d). Place %s stone.",
                     row + 1, col + 1, comp_color_name);
          }
          DrawGameStatus(gb, status);
        } else {
          DrawGameStatus(gb,
                         "Computer passes. Press SPACE to continue.");
          if (IsKeyPressed(KEY_SPACE)) {
            computer_move_pos = -1;
            detect_pos = -1;
            detect_confirm = 0;
            gp = GP::kPlayerTurn;
          }
        }
        break;
      }

      case GP::kScoring: {
        for (const auto& t : territories) {
          if (t.owner == go::Stone::kEmpty) continue;
          int player = (t.owner == go::Stone::kBlack) ? 1 : 2;
          for (int pos : t.positions) {
            Vector2 p = GameBoardPos(gb, pos);
            float sz = gb.piece_r * 0.35f;
            Color c = (player == 1) ? BLACK : WHITE;
            DrawRectangle(p.x - sz, p.y - sz, sz * 2, sz * 2, c);
            if (player == 2) {
              DrawRectangleLines(p.x - sz, p.y - sz, sz * 2, sz * 2,
                                 DARKGRAY);
            }
          }
        }

        const char* winner_str =
            (score.winner == go::Stone::kBlack)  ? "Black"
            : (score.winner == go::Stone::kWhite) ? "White"
                                                   : "Tie";
        char status[128];
        snprintf(status, sizeof(status),
                 "Game Over! Black: %.1f  White: %.1f  %s wins!",
                 score.black_score, score.white_score, winner_str);
        DrawGameStatus(gb, status);
        break;
      }
    }

    // Mic button on game board (top-right corner).
    {
      mic_x = static_cast<float>(scr_w - 50);
      mic_y = 50.0f;

      if (chat.IsVoiceRecording()) {
        // Pulsing red circle when recording.
        int pulse = (static_cast<int>(GetTime() * 4.0)) % 2;
        Color red = pulse ? Color{220, 40, 40, 255} : Color{180, 30, 30, 200};
        DrawCircle(static_cast<int>(mic_x), static_cast<int>(mic_y),
                   kMicRadius, red);
        // Stop square icon.
        DrawRectangle(static_cast<int>(mic_x) - 6, static_cast<int>(mic_y) - 6,
                      12, 12, Color{255, 255, 255, 220});
      } else if (chat.IsVoiceTranscribing()) {
        // Yellow circle when transcribing.
        DrawCircle(static_cast<int>(mic_x), static_cast<int>(mic_y),
                   kMicRadius, Color{180, 150, 40, 255});
        DrawText("...", static_cast<int>(mic_x) - 8,
                 static_cast<int>(mic_y) - 6, 16, WHITE);
      } else {
        // Idle mic button.
        Vector2 mouse = GetMousePosition();
        float dx = mouse.x - mic_x;
        float dy = mouse.y - mic_y;
        bool hover = (dx * dx + dy * dy <= kMicRadius * kMicRadius);
        Color bg = hover ? Color{70, 70, 80, 230} : Color{40, 40, 50, 200};
        DrawCircle(static_cast<int>(mic_x), static_cast<int>(mic_y),
                   kMicRadius, bg);
        // Mic icon.
        int mx = static_cast<int>(mic_x);
        int my = static_cast<int>(mic_y);
        DrawRectangle(mx - 3, my - 8, 6, 12, Color{200, 200, 200, 255});
        DrawRectangleRounded(
            Rectangle{(float)(mx - 3), (float)(my - 10), 6.0f, 4.0f},
            1.0f, 4, Color{200, 200, 200, 255});
        DrawRectangle(mx - 1, my + 5, 2, 4, Color{200, 200, 200, 255});
        DrawRectangle(mx - 4, my + 9, 8, 2, Color{200, 200, 200, 255});
      }

      // Status label below mic button.
      if (chat.IsVoiceRecording()) {
        const char* lbl = "Recording...";
        int lw = MeasureText(lbl, 14);
        DrawText(lbl, static_cast<int>(mic_x) - lw / 2,
                 static_cast<int>(mic_y + kMicRadius + 8), 14,
                 Color{255, 80, 80, 255});
      } else if (chat.IsVoiceTranscribing()) {
        const char* lbl = "Transcribing...";
        int lw = MeasureText(lbl, 14);
        DrawText(lbl, static_cast<int>(mic_x) - lw / 2,
                 static_cast<int>(mic_y + kMicRadius + 8), 14,
                 Color{255, 200, 80, 255});
      }
    }

    // Speaker (TTS) toggle button — below mic button.
    {
      spk_x = mic_x;
      spk_y = mic_y + kMicRadius + kSpkRadius + 16;
      int sx = static_cast<int>(spk_x);
      int sy = static_cast<int>(spk_y);
      bool tts_on = chat.IsTTSEnabled();

      Vector2 mouse = GetMousePosition();
      float dx = mouse.x - spk_x;
      float dy = mouse.y - spk_y;
      bool hover = (dx * dx + dy * dy <= kSpkRadius * kSpkRadius);

      Color bg = tts_on ? Color{50, 70, 50, 230} : Color{40, 40, 50, 200};
      if (hover) bg = tts_on ? Color{60, 90, 60, 240} : Color{60, 60, 70, 230};
      DrawCircle(sx, sy, kSpkRadius, bg);

      Color ic = tts_on ? Color{180, 255, 180, 255} : Color{150, 150, 150, 255};

      // Speaker icon: rectangular body on the left, flared cone to the right.
      // Body (small box, left side).
      DrawRectangle(sx - 8, sy - 3, 5, 6, ic);
      // Cone (trapezoid via two triangles: top-left, bottom-left, top-right
      // and top-right, bottom-left, bottom-right — CCW for Raylib).
      DrawTriangle(
          Vector2{(float)(sx + 2), (float)(sy - 8)},
          Vector2{(float)(sx - 3), (float)(sy - 3)},
          Vector2{(float)(sx - 3), (float)(sy + 3)}, ic);
      DrawTriangle(
          Vector2{(float)(sx + 2), (float)(sy - 8)},
          Vector2{(float)(sx - 3), (float)(sy + 3)},
          Vector2{(float)(sx + 2), (float)(sy + 8)}, ic);

      if (tts_on) {
        // Sound wave arcs (drawn as short line segments in an arc shape).
        for (int wave = 0; wave < 2; wave++) {
          float r = 7.0f + wave * 5.0f;
          unsigned char alpha = wave == 0 ? 200 : 120;
          Color wc = Color{180, 255, 180, alpha};
          float cx = (float)(sx + 3);
          for (int a = -40; a < 40; a += 5) {
            float rad1 = (float)a * 3.14159f / 180.0f;
            float rad2 = (float)(a + 5) * 3.14159f / 180.0f;
            DrawLineEx(
                Vector2{cx + r * cosf(rad1), (float)sy + r * sinf(rad1)},
                Vector2{cx + r * cosf(rad2), (float)sy + r * sinf(rad2)},
                1.5f, wc);
          }
        }
      } else {
        // Diagonal slash.
        DrawLineEx(Vector2{(float)(sx - 10), (float)(sy + 10)},
                   Vector2{(float)(sx + 10), (float)(sy - 10)}, 2.0f,
                   Color{255, 100, 100, 200});
      }
    }

    // Winrate highlight toggle button — below speaker button.
    {
      wr_x = spk_x;
      wr_y = spk_y + kSpkRadius + kWrRadius + 12;
      int wx = static_cast<int>(wr_x);
      int wy = static_cast<int>(wr_y);

      Vector2 mouse = GetMousePosition();
      float dx = mouse.x - wr_x;
      float dy = mouse.y - wr_y;
      bool hover = (dx * dx + dy * dy <= kWrRadius * kWrRadius);

      Color bg = show_winrate_colors ? Color{50, 50, 70, 230}
                                     : Color{40, 40, 50, 200};
      if (hover) bg = show_winrate_colors ? Color{60, 60, 90, 240}
                                          : Color{60, 60, 70, 230};
      DrawCircle(wx, wy, kWrRadius, bg);

      if (show_winrate_colors) {
        // Show a small red-yellow-green gradient bar as icon.
        DrawRectangle(wx - 8, wy - 3, 6, 6, Color{255, 80, 80, 255});
        DrawRectangle(wx - 2, wy - 3, 6, 6, Color{255, 220, 60, 255});
        DrawRectangle(wx + 4, wy - 3, 6, 6, Color{80, 220, 80, 255});
      } else {
        // Blue bar when winrate colors are off.
        DrawRectangle(wx - 8, wy - 3, 18, 6, Color{80, 140, 255, 255});
      }
    }

    chat.Draw(scr_w, scr_h);
    EndDrawing();
  }

done:
  CloseWindow();
  InitGoBoard(9);
  MoveToSecondMonitor();
  // Reload chat font after returning to puzzle window (new OpenGL context).
  chat.LoadChatFont("fonts/Inter-Regular.ttf");
}
