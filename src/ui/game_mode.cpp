#include "ui/game_mode.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "board.hpp"
#include "ui/board_overlay.hpp"
#include "ui/game_board.hpp"
#include "ui/game_context.hpp"
#include "ui/game_controls.hpp"
#include "ui/katago_overlay.hpp"
#include "persist/game_save.hpp"
#include "go/game.hpp"
#include "go/move.hpp"
#include "go/scoring.hpp"
#include "raylib.h"




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

  std::srand(static_cast<unsigned>(std::time(nullptr)));

  const int BOARD_SZ = settings.board_size;
  int scr_w = GetScreenWidth();
  int scr_h = GetScreenHeight();
  GameBoard gb = CalcGameBoard(BOARD_SZ, scr_w, scr_h);

  // Tell KataGo the board size and komi for this game.
  if (katago) {
    katago->SetBoardSize(BOARD_SZ);
    katago->SetKomi(settings.komi);
  }

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
  if (vision_active && vision->GetBoardSize() != BOARD_SZ) {
    fprintf(stderr,
            "Vision calibrated for %dx%d but game is %dx%d — vision disabled.\n",
            vision->GetBoardSize(), vision->GetBoardSize(), BOARD_SZ, BOARD_SZ);
    vision_active = false;
  }
  if (vision_active) {
    vision->ResetDetection();
  }
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
    sd.komi = settings.komi;
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
      katago->SetKomi(sd.komi);
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

  // Auto-load a save if requested from the main menu.
  if (!settings.load_save_path.empty()) {
    touchstone::SaveData sd;
    std::string err = touchstone::LoadGame(settings.load_save_path, sd);
    if (err.empty()) {
      err = RestoreFromSave(sd);
    }
    if (!err.empty()) {
      fprintf(stderr, "Load failed: %s\n", err.c_str());
    }
  }

  // Mic + speaker + winrate button positions (top-right of screen).
  float mic_x = 0, mic_y = 0;
  float spk_x = 0, spk_y = 0;
  float wr_x = 0, wr_y = 0;

  while (!WindowShouldClose()) {
    scr_w = GetScreenWidth();
    scr_h = GetScreenHeight();
    gb = CalcGameBoard(BOARD_SZ, scr_w, scr_h);

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

    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

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
          DrawText(TextFormat("%dx%d  %d moves  %s",
                              si.board_size, si.board_size, si.move_count,
                              si.timestamp.substr(0, 10).c_str()),
                   px + 28, row_y + 22, 12,
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
            DrawText(TextFormat("%dx%d  %d moves  %s",
                                si.board_size, si.board_size, si.move_count,
                                si.timestamp.substr(0, 10).c_str()),
                     px + 28, row_y + 22, 12,
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
          DrawGameStatus(gb, TextFormat(
              "Setup [%s]  B/W/E=color  SPACE=play  ESC=cancel",
              tool_name));
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
          DrawGameStatus(gb, TextFormat(
              "%s%s to play. Move %d. Captures: B=%d W=%d  [H=help]",
              computer_passed ? "Bot passed! " : "",
              hc, game.MoveNumber() + 1,
              game.CapturedBy(go::Stone::kBlack),
              game.CapturedBy(go::Stone::kWhite)));

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
            DrawGameStatus(gb, TextFormat(
                "Computer plays (%d,%d). Update board to match.",
                row + 1, col + 1));
          } else {
            DrawGameStatus(gb, TextFormat(
                "Computer plays at (%d,%d). Place %s stone.",
                row + 1, col + 1, comp_color_name));
          }
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
        DrawGameStatus(gb, TextFormat(
            "Game Over! Black: %.1f  White: %.1f  %s wins!",
            score.black_score, score.white_score, winner_str));
        break;
      }
    }

    DrawMicButton(mic_x, mic_y, scr_w, chat);
    DrawSpeakerButton(spk_x, spk_y, mic_x, mic_y, chat);
    DrawWinrateButton(wr_x, wr_y, spk_x, spk_y, show_winrate_colors);

    chat.Draw(scr_w, scr_h);
    EndDrawing();
  }

done:
  if (IsWindowFullscreen()) ToggleFullscreen();
  CloseWindow();
  InitGoBoard(9);
  MoveToSecondMonitor();
  // Reload chat font after returning to puzzle window (new OpenGL context).
  chat.LoadChatFont("fonts/Inter-Regular.ttf");
}
