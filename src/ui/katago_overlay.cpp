#include "ui/katago_overlay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "raylib.h"

void DrawOwnershipOverlay(const GameBoard& gb,
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

void DrawTopMoves(const GameBoard& gb,
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
    DrawText(TextFormat("%d", i + 1), static_cast<int>(p.x) - 4,
             static_cast<int>(p.y) - 6, 12, WHITE);
  }
}

Color QualityColor(float quality) {
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

void DrawKataGoStatsPanel(const GameBoard& gb,
                          const katago::AnalysisResult& analysis,
                          go::Stone current_player,
                          const std::vector<MoveRecord>& move_log,
                          go::Stone human_color) {
  const int PANEL_W = 480;
  const int PANEL_MARGIN = 10;
  int px = gb.win_w - PANEL_W - PANEL_MARGIN;
  int py = 30;
  const int LINE_H = 26;
  const int FONT_SZ = 20;
  const Color BG = {15, 15, 15, 230};
  const Color BORDER = {80, 80, 80, 200};
  const Color LABEL_COL = {160, 160, 160, 255};
  const Color VALUE_COL = {255, 255, 255, 255};

  // Compute total panel height: analysis section + move log.
  int analysis_h = 0;
  if (!analysis.valid) {
    analysis_h = 60;
  } else {
    analysis_h = 50 + 38 + LINE_H * 2 + 28;
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
      const char* banner = black_winning
          ? TextFormat("BLACK  %.0f%%", win_pct)
          : TextFormat("WHITE  %.0f%%", white_pct);
      Color banner_col = black_winning ? Color{80, 80, 80, 255}
                                       : Color{240, 240, 240, 255};
      Color banner_bg = black_winning ? Color{30, 30, 30, 255}
                                      : Color{200, 200, 200, 255};
      Color banner_text = black_winning ? Color{255, 255, 255, 255}
                                        : Color{20, 20, 20, 255};

      DrawRectangle(x, y, content_w, 42, banner_bg);
      DrawRectangleLinesEx(
          Rectangle{(float)x, (float)y, (float)content_w, 42.0f}, 1,
          banner_col);
      int tw = MeasureText(banner, 32);
      DrawText(banner, x + (content_w - tw) / 2, y + 5, 32, banner_text);
      y += 50;
    }

    // Winrate bar.
    {
      int bar_h = 28;
      int black_w = static_cast<int>(analysis.winrate * content_w);
      if (black_w < 1) black_w = 1;
      if (black_w > content_w - 1) black_w = content_w - 1;
      int white_w = content_w - black_w;

      DrawRectangle(x, y, black_w, bar_h, Color{40, 40, 40, 255});
      DrawRectangle(x + black_w, y, white_w, bar_h, Color{220, 220, 220, 255});
      DrawRectangleLinesEx(
          Rectangle{(float)x, (float)y, (float)content_w, (float)bar_h}, 1,
          BORDER);

      const char* b_lbl = TextFormat("%.0f%%", win_pct);
      if (win_pct >= 10)
        DrawText(b_lbl, x + 6, y + 4, 18, Color{200, 200, 200, 255});
      const char* w_lbl = TextFormat("%.0f%%", white_pct);
      if (white_pct >= 10)
        DrawText(w_lbl, x + content_w - MeasureText(w_lbl, 18) - 6, y + 4, 18,
                 Color{60, 60, 60, 255});
      y += bar_h + 10;
    }

    // Score lead.
    {
      const char* score_text =
          (std::abs(analysis.score_lead) < 0.5)
              ? "Score: Even"
          : (analysis.score_lead > 0)
              ? TextFormat("Score: Black +%.1f", analysis.score_lead)
              : TextFormat("Score: White +%.1f", -analysis.score_lead);
      DrawText(score_text, x, y, FONT_SZ, VALUE_COL);
      y += LINE_H;
    }

    // Territory estimate.
    {
      int black_territory = 0, white_territory = 0;
      for (double own : analysis.ownership) {
        if (own > 0.5) black_territory++;
        else if (own < -0.5) white_territory++;
      }
      DrawText(TextFormat("Territory: B~%d  W~%d", black_territory,
                          white_territory),
               x, y, FONT_SZ, LABEL_COL);
      y += LINE_H + 8;
    }
  }

  // --- Move log ---
  if (!move_log.empty()) {
    // Separator line.
    DrawLine(px + 4, y, px + PANEL_W - 4, y, BORDER);
    y += 4;

    // Header row.
    DrawText("#", x, y, 18, LABEL_COL);
    DrawText("Move", x + 36, y, 18, LABEL_COL);
    DrawText("WR", x + 110, y, 18, LABEL_COL);
    DrawText("Delta", x + 170, y, 18, LABEL_COL);
    DrawText("Best was", x + 248, y, 18, LABEL_COL);
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
      DrawText(TextFormat("%d", rec.move_number), x + 4, y, 18, LABEL_COL);

      // Move played (colored by player).
      Color move_col = is_human ? Color{140, 200, 255, 255}
                                : Color{255, 180, 140, 255};
      DrawText(TextFormat("%s%s",
                          rec.color == go::Stone::kBlack ? "B " : "W ",
                          rec.gtp_move.c_str()),
               x + 36, y, 18, move_col);

      if (rec.analysis_complete) {
        // Winrate after.
        DrawText(TextFormat("%.0f%%", rec.winrate_after * 100.0),
                 x + 110, y, 18, VALUE_COL);

        // Delta from mover's perspective.
        double delta;
        if (rec.color == go::Stone::kBlack) {
          delta = rec.winrate_after - rec.winrate_before;
        } else {
          delta = rec.winrate_before - rec.winrate_after;
        }
        DrawText(TextFormat("%+.1f%%", delta * 100.0), x + 170, y, 18, qc);

        // Top 3 KataGo suggestions.
        DrawText(rec.top3_moves.c_str(), x + 248, y, 18, LABEL_COL);
      } else {
        DrawText("...", x + 110, y, 18, LABEL_COL);
      }

      y += LINE_H;
    }
  }
}

void DrawMoveQuality(const GameBoard& gb, int pos, float quality,
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
