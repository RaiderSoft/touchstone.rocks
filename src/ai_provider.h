#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ai {

struct Message {
  std::string role;  // "user" or "assistant"
  std::string content;
};

struct Request {
  std::string model;
  std::string system_prompt;
  std::vector<Message> messages;
  int max_tokens = 512;
};

struct Response {
  bool success = false;
  std::string content;
  std::string error;
};

// STT: audio WAV bytes -> transcribed text.
struct TranscribeRequest {
  std::vector<uint8_t> audio_wav;
};

struct TranscribeResponse {
  bool success = false;
  std::string text;
  std::string error;
};

// TTS: text -> base64-encoded PCM audio.
struct SpeakRequest {
  std::string text;
  std::string voice = "Kore";
};

struct SpeakResponse {
  bool success = false;
  std::string audio_base64;  // Base64 raw PCM (24kHz, 16-bit, mono).
  std::string error;
};

// ---------------------------------------------------------------------------
// Provider interface
// ---------------------------------------------------------------------------

class Provider {
 public:
  virtual ~Provider() = default;
  virtual std::string name() const = 0;
  virtual std::vector<std::string> models() const = 0;

  // Blocking call. Must be called from a background thread.
  virtual Response Complete(const Request& request) = 0;

  // STT/TTS — only implemented by Gemini provider.
  virtual TranscribeResponse Transcribe(const TranscribeRequest&) {
    return {false, "", "Not supported by this provider"};
  }
  virtual SpeakResponse Speak(const SpeakRequest&) {
    return {false, "", "Not supported by this provider"};
  }
};

// ---------------------------------------------------------------------------
// Built-in providers (nullptr if env var not set)
// ---------------------------------------------------------------------------

// Reads CLAUDE_API_KEY.
std::unique_ptr<Provider> CreateAnthropicProvider();

// Reads GEMINI_API_KEY.
std::unique_ptr<Provider> CreateGeminiProvider();

// ---------------------------------------------------------------------------
// Service - manages providers, routes requests by model name
// ---------------------------------------------------------------------------

class Service {
 public:
  void AddProvider(std::unique_ptr<Provider> provider);

  // All models across all registered providers.
  std::vector<std::string> AvailableModels() const;

  // Blocking. Routes to the provider that owns `request.model`.
  // If model is empty, uses the default (first registered model).
  Response Complete(const Request& request);

  // STT/TTS — routes to the first provider that supports them (Gemini).
  TranscribeResponse Transcribe(const TranscribeRequest& request);
  SpeakResponse Speak(const SpeakRequest& request);

  const std::string& CurrentModel() const { return current_model_; }
  void SetModel(const std::string& model) { current_model_ = model; }

  // Cycle to the next available model. Returns the new model name.
  std::string NextModel();

  bool Available() const { return !providers_.empty(); }

 private:
  std::vector<std::unique_ptr<Provider>> providers_;
  std::string current_model_;

  Provider* ProviderForModel(const std::string& model) const;
};

}  // namespace ai
