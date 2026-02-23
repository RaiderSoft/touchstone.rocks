#include "audio_io.h"

#include "audio_encoding.h"
#include "raylib.h"

#include <AudioToolbox/AudioToolbox.h>

#include <cstdio>

namespace audio {

// ---------------------------------------------------------------------------
// AudioCapture — PIMPL detail
// ---------------------------------------------------------------------------

static constexpr int kNumBuffers = 3;
static constexpr int kBufferFrames = 4096;

struct AudioCapture::Impl {
  AudioQueueRef queue = nullptr;
  AudioQueueBufferRef buffers[kNumBuffers] = {};
};

AudioCapture::AudioCapture() : impl_(std::make_unique<Impl>()) {}

AudioCapture::~AudioCapture() {
  if (impl_->queue != nullptr) {
    AudioQueueStop(impl_->queue, true);
    AudioQueueDispose(impl_->queue, true);
  }
}

bool AudioCapture::Init() {
  AudioStreamBasicDescription format = {};
  format.mSampleRate = kSampleRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  format.mChannelsPerFrame = kChannels;
  format.mBitsPerChannel = 16;
  format.mBytesPerFrame = kChannels * 2;
  format.mFramesPerPacket = 1;
  format.mBytesPerPacket = format.mBytesPerFrame;

  // AudioQueue input callback — runs on an internal CoreAudio thread.
  // Non-capturing lambda decays to a C function pointer.
  AudioQueueInputCallback callback =
      [](void* user_data, AudioQueueRef queue, AudioQueueBufferRef buffer,
         const AudioTimeStamp* /*start_time*/, UInt32 /*num_descs*/,
         const AudioStreamPacketDescription* /*descs*/) {
        auto* self = static_cast<AudioCapture*>(user_data);

        if (self->is_recording_) {
          auto* data = static_cast<const int16_t*>(buffer->mAudioData);
          size_t count = buffer->mAudioDataByteSize / sizeof(int16_t);

          std::lock_guard<std::mutex> lock(self->buffer_mutex_);
          size_t max_samples =
              static_cast<size_t>(kSampleRate * kMaxSeconds) * kChannels;
          size_t room = (self->samples_.size() < max_samples)
                            ? max_samples - self->samples_.size()
                            : 0;
          size_t to_copy = (count < room) ? count : room;
          if (to_copy > 0) {
            self->samples_.insert(self->samples_.end(), data, data + to_copy);
          }
        }

        AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
      };

  OSStatus status = AudioQueueNewInput(&format, callback, this, nullptr,
                                        nullptr, 0, &impl_->queue);
  if (status != noErr) {
    fprintf(stderr, "AudioCapture: AudioQueueNewInput failed (%d)\n",
            static_cast<int>(status));
    return false;
  }

  UInt32 buf_size = kBufferFrames * format.mBytesPerFrame;
  for (int i = 0; i < kNumBuffers; i++) {
    AudioQueueAllocateBuffer(impl_->queue, buf_size, &impl_->buffers[i]);
    AudioQueueEnqueueBuffer(impl_->queue, impl_->buffers[i], 0, nullptr);
  }

  initialized_ = true;
  return true;
}

bool AudioCapture::Start() {
  if (!initialized_ || impl_->queue == nullptr) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    samples_.clear();
  }

  is_recording_ = true;
  OSStatus status = AudioQueueStart(impl_->queue, nullptr);
  if (status != noErr) {
    is_recording_ = false;
    fprintf(stderr, "AudioCapture: AudioQueueStart failed (%d)\n",
            static_cast<int>(status));
    return false;
  }

  return true;
}

void AudioCapture::Stop() {
  if (!is_recording_) {
    return;
  }
  is_recording_ = false;
  if (impl_->queue != nullptr) {
    AudioQueueStop(impl_->queue, true);
  }
}

std::vector<uint8_t> AudioCapture::GetRecordedWAV() {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  if (samples_.empty()) {
    return {};
  }

  auto wav = audio::EncodeToWav(samples_.data(), samples_.size(), kSampleRate,
                                kChannels);
  samples_.clear();
  return wav;
}

float AudioCapture::RecordingDuration() const {
  // Approximate — reading samples_.size() without lock is fine for UI display.
  size_t n = samples_.size();
  return static_cast<float>(n) / (kSampleRate * kChannels);
}

// ---------------------------------------------------------------------------
// TTSPlayer
// ---------------------------------------------------------------------------

void TTSPlayer::Play(const std::string& base64_pcm_audio, int sample_rate) {
  if (base64_pcm_audio.empty()) {
    return;
  }

  auto pcm_bytes = base64::Decode(base64_pcm_audio);
  if (pcm_bytes.empty()) {
    return;
  }

  // Wrap raw PCM in a WAV container so Raylib can load it.
  size_t sample_count = pcm_bytes.size() / sizeof(int16_t);
  auto wav = audio::EncodeToWav(
      reinterpret_cast<const int16_t*>(pcm_bytes.data()), sample_count,
      sample_rate, 1);

  Wave wave =
      LoadWaveFromMemory(".wav", wav.data(), static_cast<int>(wav.size()));
  if (!IsWaveValid(wave)) {
    fprintf(stderr, "TTSPlayer: LoadWaveFromMemory failed\n");
    return;
  }

  Sound sound = LoadSoundFromWave(wave);
  UnloadWave(wave);

  PlaySound(sound);

  // Block until playback finishes (this runs on a worker thread).
  while (IsSoundPlaying(sound)) {
    struct timespec ts = {0, 10000000};  // 10ms
    nanosleep(&ts, nullptr);
  }

  UnloadSound(sound);
}

void TTSPlayer::StopPlayback() {
  // Future: track active Sound handle and call StopSound().
}

}  // namespace audio
