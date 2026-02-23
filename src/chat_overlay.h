#pragma once

#include "ai_provider.h"
#include "audio_io.h"
#include "raylib.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ChatMessage {
  std::string role;  // "user" or "assistant"
  std::string content;
  double timestamp;  // GetTime() when message was added
};

enum class ChatState { kHidden, kOpen, kThinking };

class ChatOverlay {
 public:
  explicit ChatOverlay(ai::Service& service);
  ~ChatOverlay();

  // Load a TTF font for chat text. Call after InitWindow().
  void LoadChatFont(const std::string& ttf_path);

  // Call every frame BEFORE game input. Returns true if the overlay
  // consumed input this frame (game should skip Enter/Esc/typing).
  bool HandleInput();

  // Call at end of every frame, AFTER all game drawing but before
  // EndFrame()/EndDrawing().
  void Draw(int screen_w, int screen_h);

  void SetSystemPrompt(const std::string& prompt);
  void SetContextProvider(std::function<std::string()> provider);
  bool IsCapturingInput() const;

  // Public voice API — used by external mic button (game board).
  void StartRecording();
  void StopRecordingAndTranscribe();
  bool IsVoiceRecording() const { return voice_recording_; }
  bool IsVoiceTranscribing() const { return voice_transcribing_; }

  // TTS toggle — off by default.
  bool IsTTSEnabled() const { return tts_enabled_; }
  void SetTTSEnabled(bool enabled) { tts_enabled_ = enabled; }

  // Check if TTS is currently speaking (thread-safe).
  bool IsTTSSpeaking() const { return tts_speaking_.load(); }

  // Speak arbitrary text via TTS. Adds a chat bubble and starts audio
  // playback in a worker thread. Non-blocking.
  void SpeakText(const std::string& text);

  // Add a message to the chat log from an external caller.
  void AddMessage(const std::string& role, const std::string& content);

 private:
  ChatState state_ = ChatState::kHidden;
  std::string input_text_;
  int cursor_pos_ = 0;

  std::vector<ChatMessage> messages_;
  std::string system_prompt_;
  std::function<std::string()> context_provider_;

  ai::Service& service_;

  // Threading for async chat API calls.
  std::mutex response_mutex_;
  bool has_pending_response_ = false;
  std::string pending_response_;
  std::string pending_error_;
  std::thread worker_thread_;

  // Voice input.
  bool voice_recording_ = false;
  bool voice_transcribing_ = false;
  audio::AudioCapture audio_capture_;
  std::thread stt_worker_;
  std::mutex stt_mutex_;
  bool has_pending_stt_ = false;
  std::string pending_stt_text_;
  std::string pending_stt_error_;

  // Voice output.
  bool tts_enabled_ = false;
  std::atomic<bool> tts_speaking_{false};
  audio::TTSPlayer tts_player_;
  std::thread tts_worker_;

  // Model switch notification (shown briefly).
  double model_switch_time_ = 0;

  // When the overlay was last closed (for fade timing).
  double close_time_ = 0;

  // Custom font for readable chat text.
  Font font_ = {};
  bool font_loaded_ = false;

  // Scroll offset for message history (pixels scrolled up from bottom).
  int scroll_offset_ = 0;

  // Tuning constants.
  static constexpr double kFadeDelay = 5.0;
  static constexpr double kFadeDuration = 3.0;
  static constexpr int kMaxVisibleMessages = 12;
  static constexpr int kMaxApiMessages = 20;
  static constexpr int kFontSize = 24;
  static constexpr int kLineHeight = 30;
  static constexpr int kPadding = 20;
  static constexpr int kInputHeight = 48;

  void SendMessage();
  void CheckForResponse();
  void CheckForSTT();
  void SpeakResponse(const std::string& text);
  float MessageAlpha(const ChatMessage& msg, double now) const;
  void ChatDrawText(const char* text, int x, int y, int size,
                    Color color) const;
  int ChatMeasureText(const char* text, int size) const;
  std::vector<std::string> WrapText(const std::string& text,
                                    int max_width) const;
  void DrawMessages(int screen_w, int screen_h, int bottom_y);
  void DrawInputBar(int screen_w, int screen_h);
  void DrawModelIndicator(int screen_w, int screen_h);
};
