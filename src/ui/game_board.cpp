#include "ui/game_board.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

GameBoard CalcGameBoard(int size, int screen_w, int screen_h) {
  GameBoard gb;
  gb.size = size;
  constexpr int kRightMargin = 100;
  int board_area_h = screen_h - 80;
  int board_area_w = screen_w - kRightMargin;
  int usable = std::min(board_area_w, board_area_h);
  gb.cell = static_cast<float>(usable) / (size + 1);
  gb.margin = gb.cell;
  float grid_px = (size - 1) * gb.cell;
  gb.offset_x = (board_area_w - grid_px) / 2.0f;
  gb.offset_y = (board_area_h - grid_px) / 2.0f;
  gb.piece_r = gb.cell * 0.43f;
  gb.click_r = gb.cell * 0.45f;
  gb.status_y = board_area_h + 10;
  gb.win_w = screen_w;
  gb.win_h = screen_h;
  return gb;
}

Vector2 GameBoardPos(const GameBoard& gb, int pos) {
  int row = pos / gb.size;
  int col = pos % gb.size;
  return {gb.offset_x + col * gb.cell, gb.offset_y + row * gb.cell};
}

// Go column letter: A-T skipping I.
static char GoColumnLetter(int col) {
  char c = 'A' + col;
  if (c >= 'I') c++;
  return c;
}

void DrawGameBoard(const GameBoard& gb, const go::Board& board) {
  // Grid lines.
  for (int i = 0; i < gb.size; i++) {
    Vector2 a = GameBoardPos(gb, i * gb.size);
    Vector2 b = GameBoardPos(gb, i * gb.size + gb.size - 1);
    DrawLineEx(a, b, 1.5f, BLACK);
    Vector2 c = GameBoardPos(gb, i);
    Vector2 d = GameBoardPos(gb, (gb.size - 1) * gb.size + i);
    DrawLineEx(c, d, 1.5f, BLACK);
  }

  // Coordinate labels.
  int label_sz = static_cast<int>(gb.cell * 0.38f);
  if (label_sz < 10) label_sz = 10;
  float pad = gb.cell * 0.55f;
  for (int col = 0; col < gb.size; col++) {
    char buf[2] = {GoColumnLetter(col), '\0'};
    int tw = MeasureText(buf, label_sz);
    Vector2 top = GameBoardPos(gb, col);
    Vector2 bot = GameBoardPos(gb, (gb.size - 1) * gb.size + col);
    DrawText(buf, static_cast<int>(top.x - tw / 2),
             static_cast<int>(top.y - pad - label_sz), label_sz, DARKGRAY);
    DrawText(buf, static_cast<int>(bot.x - tw / 2),
             static_cast<int>(bot.y + pad), label_sz, DARKGRAY);
  }
  for (int row = 0; row < gb.size; row++) {
    int num = gb.size - row;  // Row 0 is top = highest number.
    const char* buf = TextFormat("%d", num);
    int tw = MeasureText(buf, label_sz);
    Vector2 left = GameBoardPos(gb, row * gb.size);
    Vector2 right = GameBoardPos(gb, row * gb.size + gb.size - 1);
    DrawText(buf, static_cast<int>(left.x - pad - tw),
             static_cast<int>(left.y - label_sz / 2), label_sz, DARKGRAY);
    DrawText(buf, static_cast<int>(right.x + pad),
             static_cast<int>(right.y - label_sz / 2), label_sz, DARKGRAY);
  }

  // Star points (9x9).
  if (gb.size == 9) {
    int stars[] = {2 * 9 + 2, 2 * 9 + 6, 6 * 9 + 2, 6 * 9 + 6, 4 * 9 + 4};
    for (int s : stars) {
      Vector2 p = GameBoardPos(gb, s);
      DrawCircle(p.x, p.y, 4, BLACK);
    }
  }

  // Star points (13x13).
  if (gb.size == 13) {
    int stars[] = {3 * 13 + 3, 3 * 13 + 9, 9 * 13 + 3, 9 * 13 + 9,
                   6 * 13 + 6, 3 * 13 + 6, 9 * 13 + 6,
                   6 * 13 + 3, 6 * 13 + 9};
    for (int s : stars) {
      Vector2 p = GameBoardPos(gb, s);
      DrawCircle(p.x, p.y, 4, BLACK);
    }
  }

  // Star points (19x19).
  if (gb.size == 19) {
    int stars[] = {3 * 19 + 3,  3 * 19 + 9,  3 * 19 + 15,
                   9 * 19 + 3,  9 * 19 + 9,  9 * 19 + 15,
                   15 * 19 + 3, 15 * 19 + 9, 15 * 19 + 15};
    for (int s : stars) {
      Vector2 p = GameBoardPos(gb, s);
      DrawCircle(p.x, p.y, 4, BLACK);
    }
  }

  // Stones.
  int n = board.NumPositions();
  for (int i = 0; i < n; i++) {
    if (board.At(i) == go::Stone::kEmpty) continue;
    Vector2 p = GameBoardPos(gb, i);
    if (board.At(i) == go::Stone::kBlack) {
      DrawCircle(p.x, p.y, gb.piece_r, BLACK);
    } else {
      DrawCircle(p.x, p.y, gb.piece_r, WHITE);
      DrawCircleLines(p.x, p.y, gb.piece_r, DARKGRAY);
    }
  }
}

int GameBoardClick(const GameBoard& gb) {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return -1;
  Vector2 mouse = GetMousePosition();
  int n = gb.size * gb.size;
  for (int i = 0; i < n; i++) {
    Vector2 p = GameBoardPos(gb, i);
    float dx = mouse.x - p.x, dy = mouse.y - p.y;
    if (dx * dx + dy * dy < gb.click_r * gb.click_r) return i;
  }
  return -1;
}

void DrawGameStatus(const GameBoard& gb, const char* text) {
  int font_sz = 22;
  int tw = MeasureText(text, font_sz);
  DrawText(text, (gb.win_w - tw) / 2, gb.status_y, font_sz, DARKGRAY);
}

int ComputerMove(const go::Game& game) {
  const auto& board = game.GetBoard();
  go::Stone me = game.CurrentPlayer();
  go::Stone opp = go::Opponent(me);
  int n = board.NumPositions();

  std::vector<int> captures, saves, normal;

  for (int pos = 0; pos < n; pos++) {
    if (board.At(pos) != go::Stone::kEmpty) continue;
    if (go::ValidateMove(board, pos, me, game.PreviousBoard()) !=
        go::MoveResult::kOk)
      continue;

    auto nbrs = board.Neighbors(pos);
    bool all_mine = true;
    for (int nb : nbrs) {
      if (board.At(nb) != me) {
        all_mine = false;
        break;
      }
    }
    if (all_mine && !nbrs.empty()) continue;

    bool is_capture = false;
    for (int nb : nbrs) {
      if (board.At(nb) == opp && go::CountLiberties(board, nb) == 1) {
        is_capture = true;
        break;
      }
    }
    if (is_capture) {
      captures.push_back(pos);
      continue;
    }

    bool is_save = false;
    for (int nb : nbrs) {
      if (board.At(nb) == me && go::CountLiberties(board, nb) == 1) {
        is_save = true;
        break;
      }
    }
    if (is_save) {
      saves.push_back(pos);
      continue;
    }

    normal.push_back(pos);
  }

  auto pick = [](const std::vector<int>& v) -> int {
    if (v.empty()) return -1;
    return v[std::rand() % v.size()];
  };

  int move = pick(captures);
  if (move >= 0) return move;
  move = pick(saves);
  if (move >= 0) return move;
  move = pick(normal);
  if (move >= 0) return move;
  return -1;
}

void MoveToSecondMonitor() {
  int mon_count = GetMonitorCount();
  if (mon_count > 1) {
    SetWindowMonitor(1);
  }
}

std::vector<int> FindBoardMismatches(const go::Board& expected,
                                     const touchstone::DetectionResult& det) {
  std::vector<int> mismatches;
  if (!det.board_found) return mismatches;
  int n = expected.NumPositions();
  for (int i = 0; i < n && i < (int)det.board.size(); i++) {
    go::Stone exp_stone = expected.At(i);
    touchstone::StoneColor det_color = det.board[i];
    bool match = (exp_stone == go::Stone::kEmpty &&
                  det_color == touchstone::StoneColor::kEmpty) ||
                 (exp_stone == go::Stone::kBlack &&
                  det_color == touchstone::StoneColor::kBlack) ||
                 (exp_stone == go::Stone::kWhite &&
                  det_color == touchstone::StoneColor::kWhite);
    if (!match) mismatches.push_back(i);
  }
  return mismatches;
}

void DrawMismatchRing(const GameBoard& gb, int pos) {
  Vector2 p = GameBoardPos(gb, pos);
  DrawRing({p.x, p.y}, gb.piece_r + 1, gb.piece_r + 4, 0, 360, 36, RED);
}

void DrawOffGridRings(const GameBoard& gb,
                      const touchstone::DetectionResult& det) {
  for (auto& og : det.off_grid) {
    float px = gb.offset_x + og.col * gb.cell;
    float py = gb.offset_y + og.row * gb.cell;
    DrawRing({px, py}, gb.piece_r + 1, gb.piece_r + 4, 0, 360, 36, RED);
  }
}

void DrawStone(const GameBoard& gb, int pos, bool black) {
  Vector2 p = GameBoardPos(gb, pos);
  if (black) {
    DrawCircle(p.x, p.y, gb.piece_r, BLACK);
  } else {
    DrawCircle(p.x, p.y, gb.piece_r, WHITE);
    DrawCircleLines(p.x, p.y, gb.piece_r, DARKGRAY);
  }
}

void DrawGameBoardFromDiagram(const GameBoard& gb,
                              const std::string& diagram) {
  // Background.
  static const Color kGridBg = {210, 180, 120, 255};
  ClearBackground(kGridBg);

  // Grid lines.
  for (int i = 0; i < gb.size; i++) {
    Vector2 a = GameBoardPos(gb, i * gb.size);
    Vector2 b = GameBoardPos(gb, i * gb.size + gb.size - 1);
    DrawLineEx(a, b, 1.5f, BLACK);
    Vector2 c = GameBoardPos(gb, i);
    Vector2 d = GameBoardPos(gb, (gb.size - 1) * gb.size + i);
    DrawLineEx(c, d, 1.5f, BLACK);
  }

  // Star points.
  auto draw_stars = [&](const int* pts, int count) {
    for (int i = 0; i < count; i++) {
      Vector2 p = GameBoardPos(gb, pts[i]);
      DrawCircle(p.x, p.y, 4, BLACK);
    }
  };
  if (gb.size == 9) {
    int s[] = {2*9+2, 2*9+6, 6*9+2, 6*9+6, 4*9+4};
    draw_stars(s, 5);
  } else if (gb.size == 13) {
    int s[] = {3*13+3, 3*13+9, 9*13+3, 9*13+9, 6*13+6,
               3*13+6, 9*13+6, 6*13+3, 6*13+9};
    draw_stars(s, 9);
  }

  // Stones from diagram.
  int pos = 0;
  for (char c : diagram) {
    if (c == '.') {
      pos++;
    } else if (c == 'B') {
      DrawStone(gb, pos, true);
      pos++;
    } else if (c == 'W') {
      DrawStone(gb, pos, false);
      pos++;
    }
  }
}
