#pragma once

#include <cstddef>
#include <vector>

#include "pocket_audio/core/format.hpp"

namespace pocket_audio {

// One block of interleaved stereo float32 PCM ready for the wire.
class AudioChunk {
public:
    AudioChunk()
        : samples_(static_cast<std::size_t>(kChunkSamplesPerChannel) * kChannelCount, 0.0f) {}

    float*       data()       { return samples_.data(); }
    const float* data() const { return samples_.data(); }

    std::size_t byteSize() const { return samples_.size() * sizeof(float); }
    bool        hasValidSize() const { return byteSize() == kChunkBytes; }

    int samplesPerChannel() const { return kChunkSamplesPerChannel; }
    int channels()          const { return kChannelCount; }

private:
    std::vector<float> samples_;
};

} // namespace pocket_audio
