#include "chat/chat_overlay.hpp"

#include "raylib.h"

#include <algorithm>

ChatOverlay::ChatOverlay(ai::Service& service) : service_(service) {}

ChatOverlay::~ChatOverlay() {
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  if (stt_worker_.joinable()) {
    stt_worker_.join();
  }
  if (tts_worker_.joinable()) {
    tts_worker_.join();
  }
  if (font_loaded_) {
    UnloadFont(font_);
  }
}

void ChatOverlay::LoadChatFont(const std::string& ttf_path) {
  font_ = LoadFontEx(ttf_path.c_str(), kFontSize, nullptr, 0);
  if (font_.texture.id != 0) {
    SetTextureFilter(font_.texture, TEXTURE_FILTER_BILINEAR);
    font_loaded_ = true;
    fprintf(stderr, "Chat: Loaded font '%s' at size %d\n",
            ttf_path.c_str(), kFontSize);
  } else {
    fprintf(stderr, "Chat: Failed to load font '%s', using default\n",
            ttf_path.c_str());
  }
}

void ChatOverlay::SetSystemPrompt(const std::string& prompt) {
  system_prompt_ = prompt;
}

void ChatOverlay::SetContextProvider(std::function<std::string()> provider) {
  context_provider_ = std::move(provider);
}

bool ChatOverlay::IsCapturingInput() const {
  return state_ != ChatState::kHidden;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

bool ChatOverlay::HandleInput() {
  CheckForResponse();
  CheckForSTT();

  switch (state_) {
    case ChatState::kHidden: {
      if (IsKeyPressed(KEY_ENTER)) {
        state_ = waiting_for_response_ ? ChatState::kThinking
                                       : ChatState::kOpen;
        input_text_.clear();
        cursor_pos_ = 0;
        scroll_offset_ = 0;
        return true;
      }
      return false;
    }

    case ChatState::kOpen: {
      if (IsKeyPressed(KEY_ESCAPE)) {
        state_ = ChatState::kHidden;
        close_time_ = GetTime();
        return true;
      }

      // Tab cycles through available models.
      if (IsKeyPressed(KEY_TAB)) {
        std::string next = service_.NextModel();
        if (!next.empty()) {
          model_switch_time_ = GetTime();
        }
        return true;
      }

      if (IsKeyPressed(KEY_ENTER)) {
        if (!input_text_.empty()) {
          SendMessage();
        }
        return true;
      }

      // Backspace.
      if (IsKeyPressed(KEY_BACKSPACE)) {
        if (cursor_pos_ > 0) {
          input_text_.erase(cursor_pos_ - 1, 1);
          cursor_pos_--;
        }
      }

      // Character input.
      int ch;
      while ((ch = GetCharPressed()) != 0) {
        if (ch >= 32 && ch < 127) {
          input_text_.insert(cursor_pos_, 1, static_cast<char>(ch));
          cursor_pos_++;
        }
      }

      // Mouse wheel scrolls message history.
      {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
          scroll_offset_ += static_cast<int>(wheel * kLineHeight * 2);
          if (scroll_offset_ < 0) scroll_offset_ = 0;
        }
      }

      return true;  // Always consume input when chat is open.
    }

    case ChatState::kThinking: {
      if (IsKeyPressed(KEY_ESCAPE)) {
        state_ = ChatState::kHidden;
        close_time_ = GetTime();
        return true;
      }
      // Drain char queue so nothing leaks to game.
      while (GetCharPressed() != 0) {
      }
      // Mouse wheel scrolls message history.
      {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
          scroll_offset_ += static_cast<int>(wheel * kLineHeight * 2);
          if (scroll_offset_ < 0) scroll_offset_ = 0;
        }
      }
      return true;
    }

  }
  return false;
}

// ---------------------------------------------------------------------------
// Async API
// ---------------------------------------------------------------------------

void ChatOverlay::SendMessage() {
  scroll_offset_ = 0;  // Snap to bottom on new message.
  if (!service_.Available()) {
    messages_.push_back(
        {"assistant",
         "No AI provider configured. Set CLAUDE_API_KEY or GEMINI_API_KEY.",
         GetTime()});
    return;
  }

  messages_.push_back({"user", input_text_, GetTime()});

  // Build system prompt with dynamic context.
  std::string full_prompt = system_prompt_;
  if (context_provider_) {
    std::string context = context_provider_();
    if (!context.empty()) {
      full_prompt = context + "\n\n" + full_prompt;
    }
  }

  // Collect messages for API (last N only).
  std::vector<ai::Message> api_messages;
  int start =
      std::max(0, static_cast<int>(messages_.size()) - kMaxApiMessages);
  for (int i = start; i < static_cast<int>(messages_.size()); i++) {
    const auto& msg = messages_[i];
    if (msg.role == "user" || msg.role == "assistant") {
      api_messages.push_back({msg.role, msg.content});
    }
  }

  ai::Request request;
  request.system_prompt = full_prompt;
  request.messages = api_messages;
  request.max_tokens = 32768;

  input_text_.clear();
  cursor_pos_ = 0;
  waiting_for_response_ = true;
  // Only show "thinking" state if the overlay is already open.
  // Voice-initiated messages from the game board keep the overlay hidden.
  if (state_ == ChatState::kOpen) {
    state_ = ChatState::kThinking;
  }

  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  worker_thread_ = std::thread([this, request]() {
    auto response = service_.Complete(request);

    std::lock_guard<std::mutex> lock(response_mutex_);
    has_pending_response_ = true;
    if (response.success) {
      pending_response_ = response.content;
      pending_error_.clear();
    } else {
      pending_response_.clear();
      pending_error_ = response.error;
    }
  });
}

void ChatOverlay::CheckForResponse() {
  std::lock_guard<std::mutex> lock(response_mutex_);
  if (!has_pending_response_) return;

  has_pending_response_ = false;
  waiting_for_response_ = false;

  if (!pending_error_.empty()) {
    messages_.push_back({"assistant", "Error: " + pending_error_, GetTime()});
  } else {
    messages_.push_back({"assistant", pending_response_, GetTime()});
    if (tts_enabled_) {
      SpeakResponse(pending_response_);
    }
  }

  scroll_offset_ = 0;  // Snap to bottom on new response.

  if (state_ == ChatState::kThinking) {
    state_ = ChatState::kOpen;
  }
}

// ---------------------------------------------------------------------------
// Voice input/output
// ---------------------------------------------------------------------------

void ChatOverlay::StartRecording() {
  if (!audio_capture_.IsInitialized()) {
    if (!audio_capture_.Init()) {
      messages_.push_back(
          {"assistant", "Microphone not available.", GetTime()});
      return;
    }
  }

  if (audio_capture_.Start()) {
    voice_recording_ = true;
  } else {
    messages_.push_back(
        {"assistant", "Failed to start recording.", GetTime()});
  }
}

void ChatOverlay::StopRecordingAndTranscribe() {
  audio_capture_.Stop();
  voice_recording_ = false;
  auto wav_data = audio_capture_.GetRecordedWAV();

  if (wav_data.empty()) {
    messages_.push_back({"assistant", "No audio recorded.", GetTime()});
    return;
  }

  voice_transcribing_ = true;

  if (stt_worker_.joinable()) {
    stt_worker_.join();
  }

  stt_worker_ = std::thread([this, wav = std::move(wav_data)]() {
    ai::TranscribeRequest req;
    req.audio_wav = wav;
    auto response = service_.Transcribe(req);

    std::lock_guard<std::mutex> lock(stt_mutex_);
    has_pending_stt_ = true;
    if (response.success) {
      pending_stt_text_ = response.text;
      pending_stt_error_.clear();
    } else {
      pending_stt_text_.clear();
      pending_stt_error_ = response.error;
    }
  });
}

void ChatOverlay::CheckForSTT() {
  std::lock_guard<std::mutex> lock(stt_mutex_);
  if (!has_pending_stt_) return;

  has_pending_stt_ = false;

  voice_transcribing_ = false;

  if (!pending_stt_error_.empty()) {
    messages_.push_back(
        {"assistant", "Transcription error: " + pending_stt_error_, GetTime()});
  } else if (!pending_stt_text_.empty()) {
    // Auto-send the transcribed text.
    input_text_ = pending_stt_text_;
    cursor_pos_ = static_cast<int>(input_text_.size());
    SendMessage();
  }
}

void ChatOverlay::SpeakResponse(const std::string& text) {
  if (text.empty()) return;

  if (tts_worker_.joinable()) {
    tts_worker_.join();
  }

  tts_speaking_.store(true);
  tts_worker_ = std::thread([this, text]() {
    ai::SpeakRequest req;
    req.text = text;
    auto response = service_.Speak(req);
    if (response.success) {
      tts_player_.Play(response.audio_base64, 24000);
    }
    tts_speaking_.store(false);
  });
}

void ChatOverlay::AddMessage(const std::string& role,
                             const std::string& content) {
  messages_.push_back({role, content, GetTime()});
  scroll_offset_ = 0;
}

void ChatOverlay::SpeakText(const std::string& text) {
  if (text.empty()) return;

  AddMessage("assistant", text);

  if (!tts_enabled_) return;

  if (tts_worker_.joinable()) {
    tts_worker_.join();
  }

  tts_speaking_.store(true);
  tts_worker_ = std::thread([this, text]() {
    ai::SpeakRequest req;
    req.text = text;
    auto response = service_.Speak(req);
    if (response.success) {
      tts_player_.Play(response.audio_base64, 24000);
    }
    tts_speaking_.store(false);
  });
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void ChatOverlay::Draw(int screen_w, int screen_h) {
  bool active = (state_ != ChatState::kHidden);

  if (active) {
    // Large translucent panel covering most of the screen.
    int panel_margin = 40;
    int panel_x = panel_margin;
    int panel_y = panel_margin;
    int panel_w = screen_w - panel_margin * 2;
    int panel_h = screen_h - panel_margin * 2;
    DrawRectangle(panel_x, panel_y, panel_w, panel_h, Color{0, 0, 0, 190});
    DrawRectangleLinesEx(
        Rectangle{(float)panel_x, (float)panel_y, (float)panel_w, (float)panel_h},
        1, Color{80, 80, 80, 200});

    // Header.
    const char* header = "[ENTER] send   [TAB] model   [ESC] close";
    ChatDrawText(header, panel_x + kPadding, panel_y + 12, 16,
                 Color{120, 120, 120, 200});

    DrawInputBar(screen_w, screen_h);
    DrawModelIndicator(screen_w, screen_h);
  }

  int bottom_y = active ? (screen_h - 40 - kInputHeight - kPadding) : screen_h;
  DrawMessages(screen_w, screen_h, bottom_y);
}

float ChatOverlay::MessageAlpha(const ChatMessage& msg, double now) const {
  if (state_ != ChatState::kHidden) {
    return 1.0f;
  }
  return 0.0f;  // Hidden = invisible immediately.
}

void ChatOverlay::DrawMessages(int screen_w, int screen_h, int bottom_y) {
  double now = GetTime();
  bool active = (state_ != ChatState::kHidden);
  int panel_margin = 40;
  int top_limit = active ? (panel_margin + 36) : 0;  // Below panel header.

  // Bubble layout constants.
  constexpr int kBubblePadX = 12;   // Horizontal padding inside bubble.
  constexpr int kBubblePadY = 8;    // Vertical padding inside bubble.
  constexpr int kBubbleGap = 14;    // Vertical gap between bubbles.
  constexpr int kBubbleRadius = 8;  // Corner rounding radius.
  constexpr int kRoleFontSize = 16; // Small label above bubble.
  constexpr int kRoleLabelGap = 4;  // Gap between role label and bubble.

  int bubble_area_left = active ? (panel_margin + kPadding) : kPadding;
  int bubble_area_right = active ? (screen_w - panel_margin - kPadding)
                                 : (screen_w * 2 / 3 + kPadding);
  int max_bubble_w = bubble_area_right - bubble_area_left;
  int max_text_w = max_bubble_w - kBubblePadX * 2;
  int max_msgs = active ? 200 : kMaxVisibleMessages;
  int y = bottom_y - kPadding + scroll_offset_;
  int count = 0;

  // Clip messages to panel area when active.
  if (active) {
    BeginScissorMode(panel_margin, top_limit,
                     screen_w - panel_margin * 2,
                     bottom_y - top_limit + kPadding);
  }

  // Status indicator bubble (thinking, recording, transcribing).
  {
    std::string status_text;
    Color status_color = Color{150, 150, 150, 255};

    if (waiting_for_response_) {
      int dots = (static_cast<int>(now * 3.0)) % 4;
      status_text = "Thinking";
      for (int i = 0; i < dots; i++) {
        status_text += ".";
      }
    } else if (voice_recording_) {
      int dots = (static_cast<int>(now * 2.0)) % 4;
      status_text = "Recording";
      for (int i = 0; i < dots; i++) {
        status_text += ".";
      }
      status_color = Color{255, 80, 80, 255};
    } else if (voice_transcribing_) {
      int dots = (static_cast<int>(now * 3.0)) % 4;
      status_text = "Transcribing";
      for (int i = 0; i < dots; i++) {
        status_text += ".";
      }
      status_color = Color{255, 200, 80, 255};
    }

    if (!status_text.empty()) {
      int tw = ChatMeasureText(status_text.c_str(), kFontSize);
      int bubble_h = kLineHeight + kBubblePadY * 2;
      y -= bubble_h;
      DrawRectangleRounded(
          Rectangle{(float)bubble_area_left, (float)y,
                    (float)(tw + kBubblePadX * 2), (float)bubble_h},
          0.3f, kBubbleRadius, Color{40, 40, 40, 200});
      ChatDrawText(status_text.c_str(), bubble_area_left + kBubblePadX,
                   y + kBubblePadY, kFontSize, status_color);
      y -= kBubbleGap;
    }
  }

  // Track total content height for scroll clamping.
  int content_start_y = y;

  // Walk messages newest first.
  for (int i = static_cast<int>(messages_.size()) - 1;
       i >= 0 && count < max_msgs; i--) {
    const auto& msg = messages_[i];
    float alpha = MessageAlpha(msg, now);
    if (alpha <= 0.01f) continue;

    bool is_user = (msg.role == "user");
    auto lines = WrapText(msg.content, max_text_w);
    int text_h = static_cast<int>(lines.size()) * kLineHeight;
    int bubble_h = text_h + kBubblePadY * 2;

    // Total height: role label + gap + bubble.
    int total_h = active ? (kRoleFontSize + kRoleLabelGap + bubble_h) : bubble_h;
    y -= total_h;

    // Skip drawing if entirely below visible area (scrolled past).
    if (y > bottom_y) {
      y -= kBubbleGap;
      count++;
      continue;
    }

    // Stop if we're way above the visible area.
    if (y + total_h < top_limit - 200) break;

    auto a = static_cast<unsigned char>(alpha * 255);

    // Compute bubble width based on widest line.
    int widest = 0;
    for (const auto& line : lines) {
      int lw = ChatMeasureText(line.c_str(), kFontSize);
      if (lw > widest) widest = lw;
    }
    int bubble_w = widest + kBubblePadX * 2;
    if (bubble_w > max_bubble_w) bubble_w = max_bubble_w;

    // Position: user bubbles right-aligned, AI bubbles left-aligned.
    int bubble_x;
    if (active && is_user) {
      bubble_x = bubble_area_right - bubble_w;
    } else {
      bubble_x = bubble_area_left;
    }

    int bubble_y = active ? (y + kRoleFontSize + kRoleLabelGap) : y;

    // Draw role label above bubble (only when panel is open).
    if (active) {
      const char* label = is_user ? "You" : "AI";
      int label_x = is_user ? (bubble_x + bubble_w -
                                ChatMeasureText(label, kRoleFontSize))
                            : bubble_x;
      ChatDrawText(label, label_x, y, kRoleFontSize,
                   Color{120, 120, 120, a});
    }

    // Draw bubble background.
    Color bg_color;
    if (is_user) {
      bg_color = Color{30, 60, 90, static_cast<unsigned char>(active ? 210 : (alpha * 180))};
    } else {
      bg_color = Color{45, 45, 50, static_cast<unsigned char>(active ? 210 : (alpha * 180))};
    }
    DrawRectangleRounded(
        Rectangle{(float)bubble_x, (float)bubble_y,
                  (float)bubble_w, (float)bubble_h},
        0.3f, kBubbleRadius, bg_color);

    // Draw text inside bubble.
    Color text_color;
    if (is_user) {
      text_color = Color{170, 215, 255, a};
    } else {
      text_color = Color{240, 240, 240, a};
    }

    int line_y = bubble_y + kBubblePadY;
    for (const auto& line : lines) {
      ChatDrawText(line.c_str(), bubble_x + kBubblePadX, line_y, kFontSize,
                   text_color);
      line_y += kLineHeight;
    }

    y -= kBubbleGap;
    count++;
  }

  if (active) {
    EndScissorMode();

    // Clamp scroll offset so we don't scroll past all content.
    int visible_h = bottom_y - top_limit;
    int total_content_h = content_start_y - y;
    int max_scroll = total_content_h - visible_h;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_offset_ > max_scroll) scroll_offset_ = max_scroll;
  }
}

void ChatOverlay::DrawInputBar(int screen_w, int screen_h) {
  int panel_margin = 40;
  int bar_x = panel_margin + kPadding;
  int bar_y = screen_h - panel_margin - kInputHeight - kPadding;
  int bar_w = screen_w - panel_margin * 2 - kPadding * 2;
  int text_y = bar_y + (kInputHeight - kFontSize) / 2;

  DrawRectangle(bar_x, bar_y, bar_w, kInputHeight, Color{20, 20, 20, 230});
  DrawRectangleLinesEx(
      Rectangle{(float)bar_x, (float)bar_y, (float)bar_w, (float)kInputHeight},
      1, Color{100, 100, 100, 220});

  // Prompt indicator.
  const char* prompt = "> ";
  int prompt_w = ChatMeasureText(prompt, kFontSize);
  ChatDrawText(prompt, bar_x + 10, text_y, kFontSize,
               Color{180, 180, 180, 255});

  if (state_ == ChatState::kThinking) {
    ChatDrawText("Waiting for response...", bar_x + 10 + prompt_w, text_y,
                 kFontSize, Color{150, 150, 150, 255});
  } else if (voice_recording_) {
    ChatDrawText("Recording...", bar_x + 10 + prompt_w, text_y, kFontSize,
                 Color{255, 100, 100, 255});
  } else if (voice_transcribing_) {
    ChatDrawText("Transcribing...", bar_x + 10 + prompt_w, text_y, kFontSize,
                 Color{255, 200, 80, 255});
  } else {
    BeginScissorMode(bar_x, bar_y, bar_w, kInputHeight);
    ChatDrawText(input_text_.c_str(), bar_x + 10 + prompt_w, text_y, kFontSize,
                 Color{255, 255, 255, 255});

    // Blinking cursor.
    if ((static_cast<int>(GetTime() * 2.0)) % 2 == 0) {
      std::string before = input_text_.substr(0, cursor_pos_);
      int cursor_x =
          bar_x + 10 + prompt_w + ChatMeasureText(before.c_str(), kFontSize);
      DrawRectangle(cursor_x, bar_y + 8, 2, kInputHeight - 16,
                    Color{255, 255, 255, 220});
    }
    EndScissorMode();
  }
}

void ChatOverlay::DrawModelIndicator(int screen_w, int screen_h) {
  const std::string& model = service_.CurrentModel();
  if (model.empty()) return;

  // Show current model name above the input bar, right-aligned.
  int panel_margin = 40;
  std::string label = "[TAB] " + model;
  int label_size = 18;
  int tw = ChatMeasureText(label.c_str(), label_size);
  int x = screen_w - panel_margin - kPadding - tw - 8;
  int y = screen_h - panel_margin - kInputHeight - kPadding - label_size - 8;

  // Brief highlight after switching.
  double age = GetTime() - model_switch_time_;
  Color color;
  if (age < 2.0) {
    color = Color{100, 255, 100, 220};
  } else {
    color = Color{150, 150, 150, 150};
  }

  DrawRectangle(x - 6, y - 4, tw + 12, label_size + 8, Color{0, 0, 0, 120});
  ChatDrawText(label.c_str(), x, y, label_size, color);
}

// ---------------------------------------------------------------------------
// Text utilities
// ---------------------------------------------------------------------------

void ChatOverlay::ChatDrawText(const char* text, int x, int y, int size,
                               Color color) const {
  if (font_loaded_) {
    DrawTextEx(font_, text, Vector2{(float)x, (float)y}, (float)size, 1.0f,
               color);
  } else {
    DrawText(text, x, y, size, color);
  }
}

int ChatOverlay::ChatMeasureText(const char* text, int size) const {
  if (font_loaded_) {
    Vector2 v = MeasureTextEx(font_, text, (float)size, 1.0f);
    return static_cast<int>(v.x);
  }
  return MeasureText(text, size);
}

std::vector<std::string> ChatOverlay::WrapText(const std::string& text,
                                               int max_width) const {
  std::vector<std::string> lines;
  if (text.empty()) {
    lines.push_back("");
    return lines;
  }

  std::string remaining = text;
  while (!remaining.empty()) {
    int fit = static_cast<int>(remaining.size());
    while (fit > 0 &&
           ChatMeasureText(remaining.substr(0, fit).c_str(), kFontSize) >
               max_width) {
      int space = static_cast<int>(remaining.rfind(' ', fit - 1));
      if (space <= 0) {
        fit--;
      } else {
        fit = space;
      }
    }
    if (fit <= 0) fit = 1;

    lines.push_back(remaining.substr(0, fit));
    if (fit < static_cast<int>(remaining.size()) && remaining[fit] == ' ') {
      fit++;
    }
    remaining = remaining.substr(fit);
  }

  if (lines.empty()) lines.push_back("");
  return lines;
}
