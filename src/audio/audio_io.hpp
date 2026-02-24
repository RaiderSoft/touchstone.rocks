#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace audio {

// ---------------------------------------------------------------------------
// AudioCapture — microphone recording (AudioQueue on macOS, ALSA on Linux)
// ---------------------------------------------------------------------------

class AudioCapture {
 public:
  AudioCapture();
  ~AudioCapture();

  // Not copyable or movable (owns OS resources).
  AudioCapture(const AudioCapture&) = delete;
  AudioCapture& operator=(const AudioCapture&) = delete;

  // Initialize the microphone. Returns false if mic not available.
  bool Init();

  // Start/stop recording. Non-blocking.
  bool Start();
  void Stop();

  // Get all recorded audio as WAV bytes (clears internal buffer).
  std::vector<uint8_t> GetRecordedWAV();

  bool IsRecording() const { return is_recording_; }
  bool IsInitialized() const { return initialized_; }

  // Approximate recording duration so far (seconds).
  float RecordingDuration() const;

  static constexpr int kSampleRate = 16000;
  static constexpr int kChannels = 1;
  static constexpr float kMaxSeconds = 30.0f;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  bool initialized_ = false;
  std::atomic<bool> is_recording_{false};

  std::mutex buffer_mutex_;
  std::vector<int16_t> samples_;
};

// ---------------------------------------------------------------------------
// TTSPlayer — play base64-encoded PCM audio via Raylib
// ---------------------------------------------------------------------------

class TTSPlayer {
 public:
  // Decode base64 PCM and play as audio. Blocking — call from worker thread.
  // sample_rate: expected sample rate of the decoded audio (e.g. 24000).
  void Play(const std::string& base64_pcm_audio, int sample_rate);

  // Stop any currently playing TTS audio.
  void StopPlayback();
};

}  // namespace audio
