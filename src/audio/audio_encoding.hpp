#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace audio {

// Encode PCM samples as a WAV file in memory.
// 16-bit signed PCM, little-endian.
std::vector<uint8_t> EncodeToWav(const int16_t* samples, size_t sample_count,
                                 int sample_rate, int channels);

}  // namespace audio

namespace base64 {

std::string Encode(const uint8_t* data, size_t len);
std::string Encode(const std::vector<uint8_t>& data);

std::vector<uint8_t> Decode(const std::string& encoded);

}  // namespace base64
