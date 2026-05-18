#pragma once

#include <string>

namespace pocket_audio::config {

// Default WebSocket endpoint (implement in socket_transport / socket_client next).
constexpr int kWebSocketPort = 9000;
constexpr const char* kDefaultBindHost = "0.0.0.0";
constexpr const char* kDefaultServerUrl = "ws://127.0.0.1:9000";

inline std::string defaultServerUrl() {
    return std::string("ws://127.0.0.1:") + std::to_string(kWebSocketPort);
}

}  // namespace pocket_audio::config
