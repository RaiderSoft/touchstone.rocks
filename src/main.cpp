#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cards.hpp"
#include "ui/widgets.hpp"
#include "chat/chat_overlay.hpp"
#include "persist/dotenv.hpp"
#include "ui/game_board.hpp"
#include "ui/game_mode.hpp"
#include "ui/puzzle_mode.hpp"
#include "engine/katago_engine.hpp"
#include "raylib.h"

#include <curl/curl.h>

#include "persist/game_save.hpp"
#include "vision/vision.hpp"
#include "vision/vision_dev.hpp"

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

  enum class Phase { kMenu, kGameSettings, kLoadGame };
  Phase phase = Phase::kMenu;

  // Game settings state (persists across frames while in kGameSettings).
  GameSettings pending_settings;
  int settings_preset = 0;
  bool settings_setup_mode = false;

  // Load game state (persists across frames while in kLoadGame).
  std::vector<touchstone::SaveInfo> load_saves;
  int load_scroll = 0;

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
  const char* kMenuPrompt =
      "You are a Go coach helping a student learn through puzzles. "
      "Explain concepts clearly, suggest moves, and discuss strategy. "
      "Keep responses to 1-2 short sentences — brevity matters because "
      "your words may be spoken aloud. Use Go terminology. "
      "Coordinates use GTP format (e.g. E5, C3). "
      "Use plain text only — no markdown, no bold, no asterisks, no formatting.";
  chat.SetSystemPrompt(kMenuPrompt);

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
  katago_config.human_model_path = khm ? khm : km;
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
  InitAudioDevice();
  MoveToSecondMonitor();

  // Load a readable font for the chat overlay.
  chat.LoadChatFont("fonts/Inter-Regular.ttf");

  // Menu context provider.
  auto menu_context = []() -> std::string {
    return "The student is on the main menu.";
  };
  chat.SetContextProvider(menu_context);

  // -----------------------------------------------------------------------
  // Main loop
  // -----------------------------------------------------------------------

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

    int scr_w = GetScreenWidth();
    int scr_h = GetScreenHeight();

    // --- Menu phase ---
    if (phase == Phase::kMenu) {
      bool chat_consumed = chat.HandleInput();

      BeginDrawing();
      ClearBackground(Color{35, 30, 25, 255});

      const int TITLE_Y = scr_h / 6;
      const char* title = "touchstone.rocks";
      int tw = MeasureText(title, 56);
      DrawText(title, (scr_w - tw) / 2, TITLE_Y, 56,
               Color{220, 180, 100, 255});

      const char* subtitle = "Select an option";
      int sw = MeasureText(subtitle, 24);
      DrawText(subtitle, (scr_w - sw) / 2, TITLE_Y + 68, 24, GRAY);

      const int BTN_W = 480;
      const int BTN_H = 54;
      const int GAP = 10;
      int by = TITLE_Y + 120;
      int bx = (scr_w - BTN_W) / 2;
      Vector2 mouse = GetMousePosition();

      Button btn_play("Play vs Computer", ui::GreenStyle());
      Button btn_position("Play from Position", ui::BlueStyle());
      Button btn_load("Load Game", ui::PurpleStyle());
      Button btn_puzzles("Puzzles", ui::GoldStyle());
      Button btn_exit("Exit", ui::RedStyle());

      if (btn_play.Draw(bx, by, BTN_W, BTN_H, mouse)) {
        pending_settings = GameSettings{};
        settings_preset = 0;
        settings_setup_mode = false;
        phase = Phase::kGameSettings;
      }

      by += BTN_H + GAP;
      if (btn_position.Draw(bx, by, BTN_W, BTN_H, mouse)) {
        pending_settings = GameSettings{};
        settings_preset = 0;
        settings_setup_mode = true;
        phase = Phase::kGameSettings;
      }

      by += BTN_H + GAP;
      if (btn_load.Draw(bx, by, BTN_W, BTN_H, mouse)) {
        load_saves = touchstone::ListSaves();
        load_scroll = 0;
        if (!load_saves.empty()) {
          phase = Phase::kLoadGame;
        }
      }

      by += BTN_H + GAP;
      if (btn_puzzles.Draw(bx, by, BTN_W, BTN_H, mouse)) {
        RunPuzzleMode(deck, chat, katago_ptr, &vision, vision_active,
                      vision_board_size);
        chat.SetSystemPrompt(kMenuPrompt);
        chat.SetContextProvider(menu_context);
      }

      by += BTN_H + GAP * 4;
      if (btn_exit.Draw(bx, by, BTN_W, BTN_H, mouse)) {
        EndDrawing();
        break;
      }

      chat.Draw(scr_w, scr_h);
      EndDrawing();
      continue;
    }

    // --- Game settings phase ---
    if (phase == Phase::kGameSettings) {
      bool chat_consumed = chat.HandleInput();
      SettingsAction action = DrawGameSettings(
          pending_settings, settings_preset, chat_consumed,
          vision_active ? vision_board_size : 0);
      chat.Draw(scr_w, scr_h);
      EndDrawing();
      if (action == SettingsAction::kStart) {
        pending_settings.setup_mode = settings_setup_mode;
        RunGame(chat, katago_ptr, &vision, pending_settings);
        chat.SetSystemPrompt(kMenuPrompt);
        chat.SetContextProvider(menu_context);
        phase = Phase::kMenu;
      } else if (action == SettingsAction::kCancel) {
        phase = Phase::kMenu;
      }
      continue;
    }

    // --- Load game phase ---
    if (phase == Phase::kLoadGame) {
      bool chat_consumed = chat.HandleInput();

      BeginDrawing();
      ClearBackground(Color{35, 30, 25, 255});

      const char* lt = "LOAD GAME";
      int ltw = MeasureText(lt, 30);
      DrawText(lt, (scr_w - ltw) / 2, 30, 30, Color{220, 180, 100, 255});

      int lx = (scr_w - 480) / 2;
      int ly = 80;
      int max_vis = (scr_h - 140) / 46;
      Vector2 mouse = GetMousePosition();
      std::string chosen_path;

      for (int i = load_scroll;
           i < (int)load_saves.size() && i < load_scroll + max_vis; i++) {
        int ry = ly + (i - load_scroll) * 46;
        Rectangle row = {(float)lx, (float)ry, 480, 42};
        bool rh = CheckCollisionPointRec(mouse, row);
        DrawRectangleRec(row, rh ? Color{55, 55, 65, 255}
                                : Color{40, 40, 48, 255});
        DrawRectangleLinesEx(row, 1, Color{80, 80, 90, 255});
        DrawText(load_saves[i].display_name.c_str(), lx + 8, ry + 4, 16,
                 RAYWHITE);
        DrawText(
            TextFormat("%dx%d  %d moves  %s", load_saves[i].board_size,
                       load_saves[i].board_size, load_saves[i].move_count,
                       load_saves[i].timestamp.substr(0, 10).c_str()),
            lx + 8, ry + 22, 12, Color{140, 140, 140, 255});
        if (rh && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
          chosen_path = load_saves[i].filepath;
        }
      }

      int wheel = (int)GetMouseWheelMove();
      if (wheel != 0) {
        load_scroll -= wheel;
        if (load_scroll < 0) load_scroll = 0;
        int ms = std::max(0, (int)load_saves.size() - max_vis);
        if (load_scroll > ms) load_scroll = ms;
      }

      {
        Button btn_back("< Back", ui::SubtleStyle(), 20);
        int back_w = MeasureText("< Back", 20) + 24;
        if (btn_back.Draw(lx, scr_h - 50, back_w, 36, mouse))
          phase = Phase::kMenu;
      }
      if (!chat_consumed && IsKeyPressed(KEY_ESCAPE)) {
        phase = Phase::kMenu;
      }

      chat.Draw(scr_w, scr_h);
      EndDrawing();

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
          chat.SetSystemPrompt(kMenuPrompt);
          chat.SetContextProvider(menu_context);
          phase = Phase::kMenu;
        }
      }
      continue;
    }
  }

  if (vision_active) vision.Stop();

  katago_engine.Stop();
  CloseAudioDevice();
  CloseWindow();
  curl_global_cleanup();
  return 0;
}
