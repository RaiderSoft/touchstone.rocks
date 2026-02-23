#include "audio_encoding.h"

#include <cstring>
#include <stdexcept>

namespace audio {

// Write a little-endian value to a buffer.
static void WriteLE16(uint8_t* buf, uint16_t val) {
  buf[0] = static_cast<uint8_t>(val & 0xFF);
  buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

static void WriteLE32(uint8_t* buf, uint32_t val) {
  buf[0] = static_cast<uint8_t>(val & 0xFF);
  buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
  buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
  buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

std::vector<uint8_t> EncodeToWav(const int16_t* samples, size_t sample_count,
                                 int sample_rate, int channels) {
  const int bits_per_sample = 16;
  const int block_align = channels * (bits_per_sample / 8);
  const uint32_t data_size =
      static_cast<uint32_t>(sample_count) * (bits_per_sample / 8);
  const uint32_t file_size = 36 + data_size;  // Total - 8 for RIFF header.

  // 44-byte WAV header + PCM data.
  std::vector<uint8_t> wav(44 + data_size);
  uint8_t* h = wav.data();

  // RIFF header.
  std::memcpy(h + 0, "RIFF", 4);
  WriteLE32(h + 4, file_size);
  std::memcpy(h + 8, "WAVE", 4);

  // fmt sub-chunk.
  std::memcpy(h + 12, "fmt ", 4);
  WriteLE32(h + 16, 16);  // Sub-chunk size (PCM = 16).
  WriteLE16(h + 20, 1);   // Audio format (1 = PCM).
  WriteLE16(h + 22, static_cast<uint16_t>(channels));
  WriteLE32(h + 24, static_cast<uint32_t>(sample_rate));
  WriteLE32(h + 28,
            static_cast<uint32_t>(sample_rate * block_align));  // Byte rate.
  WriteLE16(h + 32, static_cast<uint16_t>(block_align));
  WriteLE16(h + 34, static_cast<uint16_t>(bits_per_sample));

  // data sub-chunk.
  std::memcpy(h + 36, "data", 4);
  WriteLE32(h + 40, data_size);

  // PCM data (int16_t is already little-endian on x86/ARM).
  std::memcpy(wav.data() + 44, samples, data_size);

  return wav;
}

}  // namespace audio

// ---------------------------------------------------------------------------
// Base64
// ---------------------------------------------------------------------------

namespace base64 {

static const char kTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(((len + 2) / 3) * 4);

  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
    if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

    out.push_back(kTable[(n >> 18) & 0x3F]);
    out.push_back(kTable[(n >> 12) & 0x3F]);
    out.push_back((i + 1 < len) ? kTable[(n >> 6) & 0x3F] : '=');
    out.push_back((i + 2 < len) ? kTable[n & 0x3F] : '=');
  }

  return out;
}

std::string Encode(const std::vector<uint8_t>& data) {
  return Encode(data.data(), data.size());
}

static int DecodeChar(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;  // Padding or invalid.
}

std::vector<uint8_t> Decode(const std::string& encoded) {
  std::vector<uint8_t> out;
  out.reserve((encoded.size() / 4) * 3);

  uint32_t buf = 0;
  int bits = 0;

  for (char c : encoded) {
    if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
    int val = DecodeChar(c);
    if (val < 0) continue;

    buf = (buf << 6) | static_cast<uint32_t>(val);
    bits += 6;

    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
    }
  }

  return out;
}

}  // namespace base64
