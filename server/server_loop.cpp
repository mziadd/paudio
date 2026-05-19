#include "server_loop.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "core/audio_chunk.hpp"
#include "capture/system_capture.hpp"
#include "network/socket_transport.hpp"

using pocket_audio::AudioChunk;

namespace pocket_audio {

namespace {

std::atomic<bool> g_run{true};
std::atomic<int> g_chunksSent{0};

} // namespace

void requestServerStop() { g_run = false; }

int runServer() {
  g_run = true;
  std::signal(SIGINT, [](int) { g_run = false; });
#if !defined(_WIN32)
  std::signal(SIGTERM, [](int) { g_run = false; });
#endif

  auto capture = capture::SystemCapture::create();
  if (!capture) {
    std::cerr << "capture not supported on this platform\n";
    return 1;
  }

  const auto socket = network::createSocket();
  if (socket == network::kInvalidSocket) {
    std::cerr << "failed to start WebSocket server\n";
    return 1;
  }

  if (!capture->start([socket](const AudioChunk &chunk) {
        if (network::sendAudioData(socket, chunk))
          ++g_chunksSent;
      })) {
    std::cerr << "capture failed: " << capture->lastError() << "\n";
    network::closeSocket(socket);
    return 1;
  }

  std::cout << "WebSocket on port 9000 — open http://YOUR_IP:8080 and tap Listen\n";

  auto last_report = std::chrono::steady_clock::now();
  while (g_run && capture->isRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto now = std::chrono::steady_clock::now();
    if (now - last_report >= std::chrono::seconds(3)) {
      std::cout << "  to browser: " << (g_chunksSent.exchange(0) / 3)
                << " chunks/s\n";
      last_report = now;
    }
  }

  capture->stop();
  network::closeSocket(socket);
  return 0;
}

} // namespace pocket_audio
