#pragma once

#include <cstddef>

namespace pocket_audio {

constexpr int kSampleRateHz = 48000;
constexpr int kChannelCount = 2;
constexpr int kChunkSamplesPerChannel = 1024;
constexpr int kBytesPerSample = 4;

constexpr std::size_t kChunkBytes =
    static_cast<std::size_t>(kChunkSamplesPerChannel) * kChannelCount *
    kBytesPerSample;

} // namespace pocket_audio
