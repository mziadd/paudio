#pragma once

#include <cstddef>
#include <vector>

#include "core/format.hpp"

namespace pocket_audio {

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
