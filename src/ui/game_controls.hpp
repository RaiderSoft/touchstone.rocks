#pragma once

#include "chat/chat_overlay.hpp"

// Button radii (shared with click detection in game loop).
constexpr float kMicRadius = 24.0f;
constexpr float kSpkRadius = 20.0f;
constexpr float kWrRadius = 20.0f;

// Draw the mic button and return its center position via mic_x/mic_y.
void DrawMicButton(float& mic_x, float& mic_y, int scr_w,
                   ChatOverlay& chat);

// Draw the speaker/TTS toggle button below the mic.
void DrawSpeakerButton(float& spk_x, float& spk_y, float mic_x, float mic_y,
                       ChatOverlay& chat);

// Draw the winrate highlight toggle button below the speaker.
void DrawWinrateButton(float& wr_x, float& wr_y, float spk_x, float spk_y,
                       bool show_winrate_colors);
