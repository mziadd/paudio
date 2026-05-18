#pragma once

#include <functional>
#include <memory>
#include <string>

#include "core/audio_chunk.hpp"

namespace pocket_audio::capture {

using ChunkCallback = std::function<void(const AudioChunk &)>;

class SystemCapture {
public:
  static std::unique_ptr<SystemCapture> create();

  virtual ~SystemCapture() = default;
  virtual bool start(ChunkCallback on_chunk) = 0;
  virtual void stop() = 0;
  virtual bool isRunning() const = 0;
  virtual const std::string &lastError() const = 0;
};

} // namespace pocket_audio::capture
