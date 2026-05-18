#pragma once

#include <string>

#include "pocket_audio/core/audio_chunk.hpp"
#include "pocket_audio/config/network_config.hpp"

namespace pocket_audio::network {

using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

struct ServerConfig {
    int port = config::kWebSocketPort;
    std::string bind_host = config::kDefaultBindHost;
};

SocketHandle createSocket(const ServerConfig& config = {});

// Push one PCM chunk to all connected browsers.
bool sendAudioData(SocketHandle socket, const AudioChunk& chunk);

void closeSocket(SocketHandle socket);

}  // namespace pocket_audio::network
