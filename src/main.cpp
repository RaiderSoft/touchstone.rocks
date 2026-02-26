#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "boardgame/board.hpp"
#include "cards.hpp"
#include "chat/chat_overlay.hpp"
#include "persist/dotenv.hpp"
#include "ui/game_board.hpp"
#include "ui/game_mode.hpp"
#include "engine/katago_engine.hpp"
#include "go/board.hpp"
#include "go/game.hpp"
#include "go/move.hpp"
#include "go/scoring.hpp"
#include "raylib.h"

#include <curl/curl.h>

#include "persist/game_save.hpp"
#include "vision/vision.hpp"
#include "vision/vision_dev.hpp"

// ---------------------------------------------------------------------------
// Puzzle helpers
// ---------------------------------------------------------------------------

static std::vector<int> ParseDiagram(const std::string& diagram) {
  std::vector<int> cells;
  for (char c : diagram) {
    if (c == '.') cells.push_back(kEmpty);
    else if (c == 'B') cells.push_back(kPlayer1);
    else if (c == 'W') cells.push_back(kPlayer2);
  }
  return cells;
}

static void DrawBoardPieces(const touchstone::Card& card) {
  auto cells = ParseDiagram(card.diagram);
  for (int i = 0; i < static_cast<int>(cells.size()); i++) {
    if (cells[i] != kEmpty) {
      DrawPiece(i, cells[i]);
    }
  }
}

static const char* CardTypeName(touchstone::CardType t) {
  switch (t) {
    case touchstone::CardType::kCapture:      return "Capture";
    case touchstone::CardType::kDefend:       return "Defend";
    case touchstone::CardType::kLifeAndDeath: return "Life & Death";
    case touchstone::CardType::kTesuji:       return "Tesuji";
  }
  return "?";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  // CLI subcommands.
  if (argc >= 2) {
    if (std::strcmp(argv[1], "vision") == 0) {
      RunVisionDevMode();
      return 0;
    }
  }

  const auto& deck = touchstone::AllCards();
  if (deck.empty()) {
    printf("No puzzles loaded.\n");
    return 1;
  }

  auto vision_cal = touchstone::LoadCalibrationData();
  int vision_board_size = vision_cal.valid ? vision_cal.board_size : 0;
  touchstone::VisionSystem vision(vision_board_size > 0 ? vision_board_size : 9);
  bool vision_active = false;
  if (vision_board_size > 0) {
    vision.LoadCalibration();
    if (vision.IsCalibrated()) {
      vision_active = vision.Start(0);
    }
  }
  touchstone::DetectionResult baseline;
  bool baseline_captured = false;

  enum class Phase {
    kMenu, kPuzzleList, kSetup, kShowPuzzle, kCorrect, kIncorrect, kHint
  };

  int qi = -1;
  int last_clicked = -1;
  Phase phase = Phase::kMenu;
  int puzzle_scroll = 0;
  int setup_confirm = 0;
  const int SETUP_CONFIRM_NEEDED = 10;
  int removal_confirm = 0;
  const int REMOVAL_CONFIRM_NEEDED = 5;

  std::vector<int> puzzle_status(deck.size(), 0);

  curl_global_init(CURL_GLOBAL_DEFAULT);
  dotenv::Load();

  // AI chat service.
  ai::Service ai_service;
  if (auto p = ai::CreateAnthropicProvider()) {
    ai_service.AddProvider(std::move(p));
  }
  if (auto p = ai::CreateGeminiProvider()) {
    ai_service.AddProvider(std::move(p));
  }

  ChatOverlay chat(ai_service);
  const char* kCoachPrompt =
      "You are a Go coach helping a student learn through puzzles. "
      "Explain concepts clearly, suggest moves, and discuss strategy. "
      "Keep responses to 1-2 short sentences — brevity matters because "
      "your words may be spoken aloud. Use Go terminology. "
      "Coordinates use GTP format (e.g. E5, C3). "
      "Use plain text only — no markdown, no bold, no asterisks, no formatting.";
  chat.SetSystemPrompt(kCoachPrompt);

  // KataGo engine — REQUIRED.
  katago::Config katago_config;
  const char* kp = std::getenv("KATAGO_PATH");
  katago_config.katago_path = kp ? kp : "/opt/homebrew/bin/katago";
  const char* km = std::getenv("KATAGO_MODEL");
  if (!km || km[0] == '\0') {
    fprintf(stderr, "FATAL: KATAGO_MODEL not set. Set it in .env.\n");
    curl_global_cleanup();
    return 1;
  }
  katago_config.model_path = km;
  const char* khm = std::getenv("KATAGO_HUMAN_MODEL");
  katago_config.human_model_path = khm ? khm : km;  // Default: same as model.
  const char* kc = std::getenv("KATAGO_CONFIG");
  katago_config.config_path = (kc && kc[0] != '\0') ? kc : "config/analysis_example.cfg";
  const char* kv = std::getenv("KATAGO_ANALYSIS_VISITS");
  katago_config.default_max_visits = (kv && kv[0] != '\0') ? std::atoi(kv) : 200;

  katago::Engine katago_engine(katago_config);
  if (!katago_engine.Start()) {
    fprintf(stderr, "FATAL: KataGo failed to start. Check KATAGO_PATH, KATAGO_MODEL, KATAGO_CONFIG.\n");
    curl_global_cleanup();
    return 1;
  }
  fprintf(stderr, "KataGo engine ready.\n");
  katago::Engine* katago_ptr = &katago_engine;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(1280, 720, "Touchstone");
  MaximizeWindow();
  SetTargetFPS(60);
  SetExitKey(0);
  SetupGoBoard(9);
  InitAudioDevice();
  MoveToSecondMonitor();

  // Load a readable font for the chat overlay.
  chat.LoadChatFont("fonts/Inter-Regular.ttf");

  // Puzzle/menu context provider.
  auto puzzle_context = [&phase, &qi, &deck, &puzzle_status]() -> std::string {
    if (phase == Phase::kMenu || qi < 0 ||
        qi >= static_cast<int>(deck.size())) {
      return "The student is on the main menu, browsing puzzles.";
    }
    const auto& card = deck[qi];
    std::string ctx = "Current puzzle: " + card.id + " (" +
                      CardTypeName(card.type) + ")\n";
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
    if (puzzle_status[qi] == 1)
      ctx += "Student solved this puzzle.\n";
    else if (puzzle_status[qi] == -1)
      ctx += "Student made an incorrect attempt.\n";
    return ctx;
  };
  chat.SetContextProvider(puzzle_context);

  // -----------------------------------------------------------------------
  // Main loop
  // -----------------------------------------------------------------------

  while (!ShouldClose()) {
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
    if (IsWindowResized()) SetupGoBoard(9);

    // --- Menu phase ---
    if (phase == Phase::kMenu) {
      bool chat_consumed = chat.HandleInput();

      BeginDrawing();
      ClearBackground(Color{35, 30, 25, 255});

      int scr_h = GetScreenHeight();
      const int TITLE_Y = scr_h / 6;
      const char* title = "touchstone.rocks";
      int tw = MeasureText(title, 56);
      DrawText(title, (GetScreenWidth() - tw) / 2, TITLE_Y, 56,
               Color{220, 180, 100, 255});

      const char* subtitle = "Select an option";
      int sw = MeasureText(subtitle, 24);
      DrawText(subtitle, (GetScreenWidth() - sw) / 2, TITLE_Y + 68, 24,
               GRAY);

      const int BTN_W = 480;
      const int BTN_H = 54;
      const int GAP = 10;
      int start_y = TITLE_Y + 120;
      int bx = (GetScreenWidth() - BTN_W) / 2;
      Vector2 mouse = GetMousePosition();

      // "Play vs Computer" button.
      {
        int by = start_y;
        Rectangle btn = {(float)bx, (float)by, (float)BTN_W, (float)BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        Color bg = hover ? Color{55, 70, 55, 255} : Color{40, 55, 40, 255};
        DrawRectangleRec(btn, bg);
        DrawRectangle(bx, by, 4, BTN_H, Color{100, 200, 100, 255});
        DrawText("Play vs Computer", bx + 18, by + 14, 24,
                 Color{180, 255, 180, 255});
        if (hover)
          DrawRectangleLinesEx(btn, 1, Color{100, 200, 100, 255});
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          EndDrawing();
          GameSettings gs = RunGameSettings(chat);
          if (!gs.cancelled) {
            RunGame(chat, katago_ptr, &vision, gs);
          }
          chat.SetSystemPrompt(kCoachPrompt);
          chat.SetContextProvider(puzzle_context);
          continue;
        }
      }

      // "Play from Position" button.
      start_y += BTN_H + GAP;
      {
        int by = start_y;
        Rectangle btn = {(float)bx, (float)by, (float)BTN_W, (float)BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        Color bg = hover ? Color{55, 55, 70, 255} : Color{40, 40, 55, 255};
        DrawRectangleRec(btn, bg);
        DrawRectangle(bx, by, 4, BTN_H, Color{100, 140, 220, 255});
        DrawText("Play from Position", bx + 18, by + 14, 24,
                 Color{160, 190, 255, 255});
        if (hover)
          DrawRectangleLinesEx(btn, 1, Color{100, 140, 220, 255});
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          EndDrawing();
          GameSettings gs = RunGameSettings(chat);
          if (!gs.cancelled) {
            gs.setup_mode = true;
            RunGame(chat, katago_ptr, &vision, gs);
          }
          chat.SetSystemPrompt(kCoachPrompt);
          chat.SetContextProvider(puzzle_context);
          continue;
        }
      }

      // "Load Game" button.
      start_y += BTN_H + GAP;
      {
        int by = start_y;
        Rectangle btn = {(float)bx, (float)by, (float)BTN_W, (float)BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        Color bg = hover ? Color{60, 55, 70, 255} : Color{45, 40, 55, 255};
        DrawRectangleRec(btn, bg);
        DrawRectangle(bx, by, 4, BTN_H, Color{180, 140, 220, 255});
        DrawText("Load Game", bx + 18, by + 14, 24,
                 Color{200, 180, 255, 255});
        if (hover)
          DrawRectangleLinesEx(btn, 1, Color{180, 140, 220, 255});
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          EndDrawing();
          auto saves = touchstone::ListSaves();
          if (!saves.empty()) {
            // Save picker loop.
            std::string chosen_path;
            int scroll = 0;
            bool picking = true;
            while (picking && !ShouldClose()) {
              BeginDrawing();
              ClearBackground(Color{35, 30, 25, 255});
              int sw = GetScreenWidth();
              int sh = GetScreenHeight();
              const char* lt = "LOAD GAME";
              int ltw = MeasureText(lt, 30);
              DrawText(lt, (sw - ltw) / 2, 30, 30,
                       Color{220, 180, 100, 255});

              int lx = (sw - 480) / 2;
              int ly = 80;
              int max_vis = (sh - 140) / 46;
              for (int i = scroll;
                   i < (int)saves.size() && i < scroll + max_vis; i++) {
                int ry = ly + (i - scroll) * 46;
                Rectangle row = {(float)lx, (float)ry, 480, 42};
                Vector2 mp = GetMousePosition();
                bool rh = CheckCollisionPointRec(mp, row);
                DrawRectangleRec(row, rh ? Color{55, 55, 65, 255}
                                        : Color{40, 40, 48, 255});
                DrawRectangleLinesEx(row, 1, Color{80, 80, 90, 255});
                DrawText(saves[i].display_name.c_str(), lx + 8, ry + 4, 16,
                         RAYWHITE);
                DrawText(
                    TextFormat("%dx%d  %d moves  %s", saves[i].board_size,
                               saves[i].board_size, saves[i].move_count,
                               saves[i].timestamp.substr(0, 10).c_str()),
                    lx + 8, ry + 22, 12, Color{140, 140, 140, 255});
                if (rh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                  chosen_path = saves[i].filepath;
                  picking = false;
                }
              }
              int wheel = (int)GetMouseWheelMove();
              if (wheel != 0) {
                scroll -= wheel;
                if (scroll < 0) scroll = 0;
                int ms = std::max(0, (int)saves.size() - max_vis);
                if (scroll > ms) scroll = ms;
              }
              DrawText("ESC to cancel", lx, sh - 30, 14, GRAY);
              if (IsKeyPressed(KEY_ESCAPE)) picking = false;
              EndDrawing();
            }
            if (!chosen_path.empty()) {
              touchstone::SaveData sd;
              std::string err = touchstone::LoadGame(chosen_path, sd);
              if (err.empty()) {
                GameSettings gs;
                gs.board_size = sd.board_size;
                gs.human_color = static_cast<go::Stone>(sd.human_color);
                gs.human_sl_profile = sd.human_sl_profile;
                gs.komi = sd.komi;
                gs.load_save_path = chosen_path;
                RunGame(chat, katago_ptr, &vision, gs);
              }
            }
          }
          chat.SetSystemPrompt(kCoachPrompt);
          chat.SetContextProvider(puzzle_context);
          continue;
        }
      }

      // "Puzzles" button.
      start_y += BTN_H + GAP;
      {
        int by = start_y;
        Rectangle btn = {(float)bx, (float)by, (float)BTN_W, (float)BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        Color bg = hover ? Color{70, 65, 55, 255} : Color{50, 45, 38, 255};
        DrawRectangleRec(btn, bg);
        DrawRectangle(bx, by, 4, BTN_H, Color{220, 180, 100, 255});
        DrawText("Puzzles", bx + 18, by + 14, 24,
                 Color{255, 220, 140, 255});
        if (hover)
          DrawRectangleLinesEx(btn, 1, Color{220, 180, 100, 255});
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          puzzle_scroll = 0;
          phase = Phase::kPuzzleList;
        }
      }

      // "Exit" button.
      start_y += BTN_H + GAP * 4;
      {
        int by = start_y;
        Rectangle btn = {(float)bx, (float)by, (float)BTN_W, (float)BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);
        Color bg = hover ? Color{70, 45, 45, 255} : Color{50, 38, 38, 255};
        DrawRectangleRec(btn, bg);
        DrawRectangle(bx, by, 4, BTN_H, Color{180, 80, 80, 255});
        DrawText("Exit", bx + 18, by + 14, 24,
                 Color{255, 140, 140, 255});
        if (hover)
          DrawRectangleLinesEx(btn, 1, Color{180, 80, 80, 255});
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          EndDrawing();
          break;
        }
      }

      chat.Draw(GetScreenWidth(), GetScreenHeight());
      EndDrawing();
      continue;
    }

    // --- Puzzle browser ---
    if (phase == Phase::kPuzzleList) {
      bool chat_consumed = chat.HandleInput();

      BeginDrawing();
      ClearBackground(Color{35, 30, 25, 255});

      int scr_w = GetScreenWidth();
      int scr_h = GetScreenHeight();

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
        int by = ly + (i - puzzle_scroll) * (BTN_H + GAP);
        Rectangle btn = {(float)lx, (float)by, (float)BTN_W, (float)BTN_H};
        bool hover = CheckCollisionPointRec(mouse, btn);

        Color bg = hover ? Color{70, 65, 55, 255} : Color{50, 45, 38, 255};
        DrawRectangleRec(btn, bg);

        Color indicator = GRAY;
        const char* status_icon = " ";
        if (puzzle_status[i] == 1) {
          indicator = GREEN;
          status_icon = "OK";
        } else if (puzzle_status[i] == -1) {
          indicator = RED;
          status_icon = "X";
        }
        DrawRectangle(lx, by, 4, BTN_H, indicator);

        DrawText(TextFormat("%d. [%s] %s", i + 1,
                            CardTypeName(deck[i].type), deck[i].id.c_str()),
                 lx + 18, by + 14, 22, RAYWHITE);
        DrawText(status_icon, lx + BTN_W - 40, by + 16, 18, indicator);

        if (hover)
          DrawRectangleLinesEx(btn, 1, Color{220, 180, 100, 255});

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          qi = i;
          last_clicked = -1;
          if (vision_active) {
            baseline_captured = false;
            setup_confirm = 0;
          }
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

      DrawText("ESC to go back", lx, scr_h - 30, 14, GRAY);
      if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
        phase = Phase::kMenu;
      }

      chat.Draw(scr_w, scr_h);
      EndDrawing();
      continue;
    }

    // --- Puzzle phases ---
    bool chat_consumed = chat.HandleInput();
    BeginFrame();

    {
      const auto& card = deck[qi];
      DrawBoardPieces(card);

      switch (phase) {
        case Phase::kSetup: {
          if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
            phase = Phase::kMenu;
            break;
          }
          auto det = vision.GetLatestDetection();
          auto expected = ParseDiagram(card.diagram);
          int stones_expected = 0, stones_matched = 0;
          for (int i = 0; i < (int)expected.size(); i++) {
            if (expected[i] == kEmpty) continue;
            stones_expected++;
            if (i < (int)det.board.size()) {
              bool match =
                  (expected[i] == kPlayer1 &&
                   det.board[i] == touchstone::StoneColor::kBlack) ||
                  (expected[i] == kPlayer2 &&
                   det.board[i] == touchstone::StoneColor::kWhite);
              if (match) stones_matched++;
            }
          }

          int errors = 0;
          if (det.board_found) {
            for (int i = 0; i < (int)expected.size() &&
                            i < (int)det.board.size();
                 i++) {
              bool det_black =
                  det.board[i] == touchstone::StoneColor::kBlack;
              bool det_white =
                  det.board[i] == touchstone::StoneColor::kWhite;
              bool det_empty =
                  det.board[i] == touchstone::StoneColor::kEmpty;
              bool exp_black = expected[i] == kPlayer1;
              bool exp_white = expected[i] == kPlayer2;
              bool exp_empty = expected[i] == kEmpty;

              bool wrong = false;
              if (exp_empty && !det_empty) wrong = true;
              if (exp_black && !det_black) wrong = true;
              if (exp_white && !det_white) wrong = true;
              if (wrong) {
                DrawHighlight(i);
                errors++;
              }
            }
          }

          if (stones_matched == stones_expected && errors == 0 &&
              det.board_found) {
            setup_confirm++;
            if (setup_confirm >= SETUP_CONFIRM_NEEDED) {
              phase = Phase::kShowPuzzle;
              baseline_captured = false;
              setup_confirm = 0;
              break;
            }
            DrawStatus(TextFormat("[%d/%d] Board ready! Confirming...",
                                  qi + 1, (int)deck.size()));
          } else {
            setup_confirm = 0;
            DrawStatus(TextFormat(
                "[%d/%d] Set up the board: %d/%d stones placed",
                qi + 1, (int)deck.size(), stones_matched,
                stones_expected));
          }
          break;
        }

        case Phase::kShowPuzzle: {
          if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
            phase = Phase::kMenu;
            break;
          }

          if (IsKeyPressed(KEY_H) && !card.hint.empty()) {
            phase = Phase::kHint;
            break;
          }

          int clicked = GetClickedPosition();
          bool board_mismatch = false;

          if (clicked < 0 && vision_active) {
            auto det = vision.GetLatestDetection();
            if (det.board_found) {
              auto expected_cells = ParseDiagram(card.diagram);
              for (int i = 0; i < (int)expected_cells.size() &&
                              i < (int)det.board.size();
                   i++) {
                if (expected_cells[i] == kEmpty) continue;
                bool match =
                    (expected_cells[i] == kPlayer1 &&
                     det.board[i] == touchstone::StoneColor::kBlack) ||
                    (expected_cells[i] == kPlayer2 &&
                     det.board[i] == touchstone::StoneColor::kWhite);
                if (!match) {
                  board_mismatch = true;
                  DrawHighlight(i);
                }
              }
            }
            if (!board_mismatch) {
              clicked = DetectVisionMove(vision, card, baseline,
                                         baseline_captured);
            }
          }

          if (board_mismatch) {
            DrawStatus(
                "Board mismatch! Fix the board before moving.  [ESC=menu]");
          } else {
            std::string player =
                (card.player_to_move == go::Stone::kBlack) ? "Black"
                                                           : "White";
            DrawStatus(TextFormat("%s to play.%s  [ESC=menu]",
                                  player.c_str(),
                                  card.hint.empty() ? "" : " [H=hint]"));
          }

          if (clicked >= 0) {
            auto cells = ParseDiagram(card.diagram);
            if (clicked < (int)cells.size() && cells[clicked] != kEmpty) {
              break;
            }

            if (touchstone::IsCorrectMove(card, clicked)) {
              phase = Phase::kCorrect;
              last_clicked = clicked;
              puzzle_status[qi] = 1;
            } else {
              go::Board board(card.board_size, card.diagram);
              go::Board prev(card.board_size);
              auto result = go::ValidateMove(board, clicked,
                                             card.player_to_move, prev);
              if (result == go::MoveResult::kOk) {
                phase = Phase::kIncorrect;
                last_clicked = clicked;
                if (puzzle_status[qi] == 0) puzzle_status[qi] = -1;
              }
            }
          }
          break;
        }

        case Phase::kCorrect: {
          int player = (card.player_to_move == go::Stone::kBlack) ? kPlayer1
                                                                  : kPlayer2;
          DrawPiece(last_clicked, player);
          DrawHighlight(last_clicked);
          DrawStatus(
              ("Correct! " + card.explanation + "  [SPACE]").c_str());

          if (IsKeyPressed(KEY_SPACE)) {
            last_clicked = -1;
            phase = Phase::kMenu;
          }
          break;
        }

        case Phase::kIncorrect: {
          int player = (card.player_to_move == go::Stone::kBlack) ? kPlayer1
                                                                  : kPlayer2;
          DrawPiece(last_clicked, player);

          if (vision_active) {
            DrawStatus("Incorrect. Remove the stone to try again.");
            auto det = vision.GetLatestDetection();
            if (det.board_found && last_clicked >= 0 &&
                last_clicked < (int)det.board.size() &&
                det.board[last_clicked] ==
                    touchstone::StoneColor::kEmpty) {
              removal_confirm++;
              if (removal_confirm >= REMOVAL_CONFIRM_NEEDED) {
                removal_confirm = 0;
                last_clicked = -1;
                baseline_captured = false;
                phase = Phase::kShowPuzzle;
              }
            } else {
              removal_confirm = 0;
            }
          } else {
            DrawStatus("Incorrect. Try again!  [SPACE]");
            if (IsKeyPressed(KEY_SPACE)) {
              last_clicked = -1;
              phase = Phase::kShowPuzzle;
            }
          }
          break;
        }

        case Phase::kHint: {
          DrawStatus(("Hint: " + card.hint + "  [SPACE]").c_str());
          if (IsKeyPressed(KEY_SPACE)) {
            phase = Phase::kShowPuzzle;
          }
          break;
        }

        case Phase::kMenu:
          break;
      }
    }

    chat.Draw(GetScreenWidth(), GetScreenHeight());
    EndFrame();
  }

  if (vision_active) vision.Stop();

  katago_engine.Stop();
  CloseAudioDevice();
  CloseBoard();
  curl_global_cleanup();
  return 0;
}
