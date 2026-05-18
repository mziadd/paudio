#pragma once

#include <cstring>
#include <vector>

#include "core/audio_chunk.hpp"
#include "core/format.hpp"

namespace pocket_audio {

// Emit fixed 8192-byte stereo float chunks from interleaved L,R pending buffer.
template <typename Callback>
inline void flushStereoChunks(std::vector<float> &pending, std::size_t &head,
                              const Callback &cb) {
  const std::size_t chunkLen =
      static_cast<std::size_t>(kChunkSamplesPerChannel * kChannelCount);

  while (pending.size() - head >= chunkLen) {
    AudioChunk chunk;
    std::memcpy(chunk.data(), pending.data() + head, chunkLen * sizeof(float));
    head += chunkLen;
    if (cb)
      cb(chunk);
  }

  constexpr std::size_t kCompactThreshold = 8192;
  if (head >= kCompactThreshold) {
    const std::size_t left = pending.size() - head;
    if (left > 0) {
      std::memmove(pending.data(), pending.data() + head, left * sizeof(float));
    }
    pending.resize(left);
    head = 0;
  }
}

} // namespace pocket_audio
