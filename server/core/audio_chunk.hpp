#pragma once

#include <cstddef>
#include <vector>

#include "core/format.hpp"

namespace pocket_audio {

// One WS binary frame: 1024 frames × 2 ch × float32 = 8192 bytes.
class AudioChunk {
public:
  AudioChunk()
      : samples_(static_cast<std::size_t>(kChunkSamplesPerChannel) *
                     kChannelCount,
                 0.0f) {}

  float *data() { return samples_.data(); }
  const float *data() const { return samples_.data(); }
  std::size_t byteSize() const { return samples_.size() * sizeof(float); }
  bool hasValidSize() const { return byteSize() == kChunkBytes; }

private:
  std::vector<float> samples_;
};

} // namespace pocket_audio
