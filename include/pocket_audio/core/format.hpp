#pragma once

#include <cstddef>

// Shared PCM format for server and web client.
// We use float32 at 48 kHz — the native rate of macOS and Windows 11 mixers.
// No conversion happens anywhere in the pipeline, so quality is preserved.
namespace pocket_audio {

constexpr int kSampleRateHz             = 48000;
constexpr int kChannelCount             = 2;
// 1024 frames @ 48 kHz = ~21 ms per packet, ~47 packets/sec.
// This is the sweet spot for Wi-Fi: small enough for low latency,
// large enough to ride out typical Wi-Fi jitter without stuttering.
// (Smaller chunks like 256 caused glitches over wireless.)
constexpr int kChunkSamplesPerChannel   = 1024;
constexpr int kBytesPerSample           = 4;      // float32

// 1024 frames * 2 channels * 4 bytes = 8192 bytes per chunk.
constexpr std::size_t kChunkBytes =
    static_cast<std::size_t>(kChunkSamplesPerChannel) * kChannelCount * kBytesPerSample;

} // namespace pocket_audio
