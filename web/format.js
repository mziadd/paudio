// Mirrors include/pocket_audio/core/format.hpp.
export const kSampleRateHz           = 48000;
export const kChannelCount           = 2;
export const kBytesPerSample         = 4;        // float32
export const kChunkSamplesPerChannel = 1024;    // ~21 ms at 48 kHz — robust over Wi-Fi
export const kChunkBytes             = kChunkSamplesPerChannel * kChannelCount * kBytesPerSample; // 8192
