#include "ui/puzzle_mode.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "cards.hpp"
#include "go/board.hpp"
#include "go/move.hpp"
#include "raylib.h"
#include "ui/game_board.hpp"
#include "ui/vision_move.hpp"
#include "ui/widgets.hpp"

void RunPuzzleMode(const std::vector<touchstone::Card>& deck,
                   ChatOverlay& chat, katago::Engine* katago,
                   touchstone::VisionSystem* vision,
                   bool vision_active, int vision_board_size) {
  enum class Phase {
    kPuzzleList,
    kSetup,
    kShowPuzzle,
    kCorrect,
    kIncorrect,
    kHint
  };

  int qi = -1;
  int last_clicked = -1;
  Phase phase = Phase::kPuzzleList;
  int puzzle_scroll = 0;
  int setup_confirm = 0;
  const int SETUP_CONFIRM_NEEDED = 10;
  int removal_confirm = 0;
  const int REMOVAL_CONFIRM_NEEDED = 5;

  VisionMoveState vision_state;
  float auto_confirm_secs = 0.0f;
  if (vision_active && vision) {
    auto_confirm_secs = vision->GetCalibration().auto_confirm_seconds;
  }

  GameBoard puzzle_gb =
      CalcGameBoard(9, GetScreenWidth(), GetScreenHeight());

  // Puzzle context provider for chat.
  auto puzzle_context = [&phase, &qi, &deck]() -> std::string {
    if (phase == Phase::kPuzzleList || qi < 0 ||
        qi >= static_cast<int>(deck.size())) {
      return "The student is browsing the puzzle list.";
    }
    const auto& card = deck[qi];
    std::string ctx = "Current puzzle: " + card.id + " (" +
                      touchstone::CardTypeName(card.type) + ")\n";
    ctx += "Board (9x9, '.'=empty, 'B'=black, 'W'=white):\n";
    for (int r = 0; r < card.board_size; r++) {
      for (int c = 0; c < card.board_size; c++) {
        int idx = r * card.board_size + c;
        if (idx < static_cast<int>(card.diagram.size())) {
          ctx += card.diagram[idx];
          if (c < card.board_size - 1) ctx += ' ';
        }
      }
      ctx += '\n';
    }
    ctx += (card.player_to_move == go::Stone::kBlack) ? "Black to play.\n"
                                                      : "White to play.\n";
    if (!card.hint.empty()) ctx += "Hint: " + card.hint + "\n";
    return ctx;
  };
  chat.SetContextProvider(puzzle_context);

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
    if (IsWindowResized())
      puzzle_gb = CalcGameBoard(9, GetScreenWidth(), GetScreenHeight());

    int scr_w = GetScreenWidth();
    int scr_h = GetScreenHeight();

    // --- Puzzle list ---
    if (phase == Phase::kPuzzleList) {
      bool chat_consumed = chat.HandleInput();

      BeginDrawing();
      ClearBackground(Color{35, 30, 25, 255});

      const char* title = "PUZZLES";
      int title_w = MeasureText(title, 30);
      DrawText(title, (scr_w - title_w) / 2, 30, 30,
               Color{220, 180, 100, 255});

      const int BTN_W = 480;
      const int BTN_H = 54;
      const int GAP = 10;
      int lx = (scr_w - BTN_W) / 2;
      int ly = 80;
      int max_vis = (scr_h - 140) / (BTN_H + GAP);
      if (max_vis < 1) max_vis = 1;
      Vector2 mouse = GetMousePosition();

      for (int i = puzzle_scroll;
           i < (int)deck.size() && i < puzzle_scroll + max_vis; i++) {
        int py = ly + (i - puzzle_scroll) * (BTN_H + GAP);

        Button btn(TextFormat("%d. [%s] %s", i + 1,
                              touchstone::CardTypeName(deck[i].type),
                              deck[i].id.c_str()),
                   ui::GoldStyle(), 22);
        if (btn.Draw(lx, py, BTN_W, BTN_H, mouse)) {
          qi = i;
          last_clicked = -1;
          vision_state.Reset();
          setup_confirm = 0;
          phase = vision_active ? Phase::kSetup : Phase::kShowPuzzle;
        }
      }

      int wheel = (int)GetMouseWheelMove();
      if (wheel != 0) {
        puzzle_scroll -= wheel;
        if (puzzle_scroll < 0) puzzle_scroll = 0;
        int ms = std::max(0, (int)deck.size() - max_vis);
        if (puzzle_scroll > ms) puzzle_scroll = ms;
      }

      {
        Button btn_back("< Back", ui::SubtleStyle(), 20);
        int back_w = MeasureText("< Back", 20) + 24;
        if (btn_back.Draw(lx, scr_h - 50, back_w, 36, mouse)) {
          chat.Draw(scr_w, scr_h);
          EndDrawing();
          return;  // Back to main menu.
        }
      }
      if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
        chat.Draw(scr_w, scr_h);
        EndDrawing();
        return;  // Back to main menu.
      }

      chat.Draw(scr_w, scr_h);
      EndDrawing();
      continue;
    }

    // --- Puzzle phases (setup, play, correct, incorrect, hint) ---
    bool chat_consumed = chat.HandleInput();
    BeginDrawing();

    {
      const auto& card = deck[qi];
      DrawGameBoardFromDiagram(puzzle_gb, card.diagram);

      switch (phase) {
        case Phase::kSetup: {
          if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
            phase = Phase::kPuzzleList;
            break;
          }
          auto det = vision->GetLatestDetection();
          auto expected = DiagramToVisionColors(card.diagram);
          int stones_expected = 0, stones_matched = 0;
          for (int i = 0; i < (int)expected.size(); i++) {
            if (expected[i] == touchstone::StoneColor::kEmpty) continue;
            stones_expected++;
            if (i < (int)det.board.size() && det.board[i] == expected[i]) {
              stones_matched++;
            }
          }

          int errors = 0;
          if (det.board_found) {
            for (int i = 0; i < (int)expected.size() &&
                            i < (int)det.board.size();
                 i++) {
              if (expected[i] != det.board[i]) {
                DrawMismatchRing(puzzle_gb, i);
                errors++;
              }
            }
          }

          if (stones_matched == stones_expected && errors == 0 &&
              det.board_found) {
            setup_confirm++;
            if (setup_confirm >= SETUP_CONFIRM_NEEDED) {
              phase = Phase::kShowPuzzle;
              vision_state.Reset();
              setup_confirm = 0;
              break;
            }
            DrawGameStatus(
                puzzle_gb,
                TextFormat("[%d/%d] Board ready! Confirming...", qi + 1,
                           (int)deck.size()));
          } else {
            setup_confirm = 0;
            DrawGameStatus(
                puzzle_gb,
                TextFormat("[%d/%d] Set up the board: %d/%d stones placed",
                           qi + 1, (int)deck.size(), stones_matched,
                           stones_expected));
          }
          break;
        }

        case Phase::kShowPuzzle: {
          if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
            phase = Phase::kPuzzleList;
            break;
          }

          if (IsKeyPressed(KEY_H) && !card.hint.empty()) {
            phase = Phase::kHint;
            break;
          }

          int clicked = GameBoardClick(puzzle_gb);
          bool showing_vision = false;

          if (clicked < 0 && vision_active && vision) {
            auto det = vision->GetLatestDetection();
            auto expected = DiagramToVisionColors(card.diagram);
            touchstone::StoneColor move_color =
                (card.player_to_move == go::Stone::kBlack)
                    ? touchstone::StoneColor::kBlack
                    : touchstone::StoneColor::kWhite;

            auto result = UpdateVisionMove(vision_state, expected, move_color,
                                           det, auto_confirm_secs,
                                           GetFrameTime());

            if (!result.mismatches.empty()) {
              for (int pos : result.mismatches) {
                DrawMismatchRing(puzzle_gb, pos);
              }
              DrawGameStatus(
                  puzzle_gb,
                  "Board mismatch! Fix the board before moving.  [ESC=back]");
              showing_vision = true;
            } else if (result.pending_pos >= 0) {
              bool is_black =
                  (card.player_to_move == go::Stone::kBlack);
              DrawPendingMove(puzzle_gb, result.pending_pos, is_black,
                              result.pending_progress, auto_confirm_secs);
              if (auto_confirm_secs > 0) {
                float remaining =
                    auto_confirm_secs -
                    static_cast<float>(vision_state.pending_timer);
                if (remaining < 0) remaining = 0;
                DrawGameStatus(
                    puzzle_gb,
                    TextFormat("Move detected. Auto-confirm in %.1fs  "
                               "[SPACE=now]  [ESC=back]",
                               remaining));
              } else {
                DrawGameStatus(
                    puzzle_gb,
                    "Move detected. Press SPACE to confirm.  [ESC=back]");
              }
              showing_vision = true;
            }

            if (result.confirmed_pos >= 0) {
              clicked = result.confirmed_pos;
            }

            DrawOffGridRings(puzzle_gb, det);
          }

          if (!showing_vision) {
            std::string player =
                (card.player_to_move == go::Stone::kBlack) ? "Black"
                                                           : "White";
            DrawGameStatus(
                puzzle_gb,
                TextFormat("%s to play.%s  [ESC=back]", player.c_str(),
                           card.hint.empty() ? "" : " [H=hint]"));
          }

          if (clicked >= 0) {
            auto cells = touchstone::ParseDiagram(card.diagram);
            if (clicked < (int)cells.size() && cells[clicked] != 0) {
              break;  // Clicked on an existing stone.
            }

            if (touchstone::IsCorrectMove(card, clicked)) {
              phase = Phase::kCorrect;
              last_clicked = clicked;
            } else {
              go::Board board(card.board_size, card.diagram);
              go::Board prev(card.board_size);
              auto result = go::ValidateMove(board, clicked,
                                             card.player_to_move, prev);
              if (result == go::MoveResult::kOk) {
                phase = Phase::kIncorrect;
                last_clicked = clicked;
              }
            }
          }
          break;
        }

        case Phase::kCorrect: {
          bool is_black = (card.player_to_move == go::Stone::kBlack);
          DrawStone(puzzle_gb, last_clicked, is_black);
          {
            Vector2 hp = GameBoardPos(puzzle_gb, last_clicked);
            DrawRing({hp.x, hp.y}, puzzle_gb.piece_r + 1,
                     puzzle_gb.piece_r + 4, 0, 360, 36, GREEN);
          }
          DrawGameStatus(
              puzzle_gb,
              ("Correct! " + card.explanation + "  [SPACE]").c_str());

          if (IsKeyPressed(KEY_SPACE)) {
            last_clicked = -1;
            phase = Phase::kPuzzleList;
          }
          break;
        }

        case Phase::kIncorrect: {
          bool is_black = (card.player_to_move == go::Stone::kBlack);
          DrawStone(puzzle_gb, last_clicked, is_black);

          if (vision_active && vision) {
            DrawGameStatus(puzzle_gb,
                           "Incorrect. Remove the stone to try again.");
            auto det = vision->GetLatestDetection();
            if (det.board_found && last_clicked >= 0 &&
                last_clicked < (int)det.board.size() &&
                det.board[last_clicked] ==
                    touchstone::StoneColor::kEmpty) {
              removal_confirm++;
              if (removal_confirm >= REMOVAL_CONFIRM_NEEDED) {
                removal_confirm = 0;
                last_clicked = -1;
                vision_state.Reset();
                phase = Phase::kShowPuzzle;
              }
            } else {
              removal_confirm = 0;
            }
          } else {
            DrawGameStatus(puzzle_gb, "Incorrect. Try again!  [SPACE]");
            if (IsKeyPressed(KEY_SPACE)) {
              last_clicked = -1;
              phase = Phase::kShowPuzzle;
            }
          }
          break;
        }

        case Phase::kHint: {
          DrawGameStatus(puzzle_gb,
                         ("Hint: " + card.hint + "  [SPACE]").c_str());
          if (IsKeyPressed(KEY_SPACE)) {
            phase = Phase::kShowPuzzle;
          }
          break;
        }

        case Phase::kPuzzleList:
          break;
      }
    }

    chat.Draw(scr_w, scr_h);
    EndDrawing();
  }
}
