#include "ui/game_controls.hpp"

#include <cmath>

#include "raylib.h"

void DrawMicButton(float& mic_x, float& mic_y, int scr_w,
                   ChatOverlay& chat) {
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

void DrawSpeakerButton(float& spk_x, float& spk_y, float mic_x, float mic_y,
                       ChatOverlay& chat) {
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
  // Cone (trapezoid via two triangles).
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

void DrawWinrateButton(float& wr_x, float& wr_y, float spk_x, float spk_y,
                       bool show_winrate_colors) {
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
