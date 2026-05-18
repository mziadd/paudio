#pragma once

#include <string>

#include "core/audio_chunk.hpp"
#include "core/config.hpp"

namespace pocket_audio::network {

using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

struct ServerConfig {
  int port = config::kWebSocketPort;
  std::string bind_host = config::kDefaultBindHost;
};

SocketHandle createSocket(const ServerConfig &config = {});
bool sendAudioData(SocketHandle socket, const AudioChunk &chunk);
void closeSocket(SocketHandle socket);

} // namespace pocket_audio::network
