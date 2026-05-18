// pocket-audio-server: system audio -> browser over WebSocket.

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#include <ixwebsocket/IXNetSystem.h>
#endif

#include "capture/system_capture.hpp"
#include "network/socket_transport.hpp"

namespace {

std::atomic<bool> g_run{true};
std::atomic<int>  g_chunksSent{0};

void onSignal(int) { g_run = false; }

}  // namespace

using pocket_audio::AudioChunk;
using pocket_audio::capture::SystemCapture;
using pocket_audio::network::closeSocket;
using pocket_audio::network::createSocket;
using pocket_audio::network::kInvalidSocket;
using pocket_audio::network::sendAudioData;

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "-h" || std::string_view(argv[i]) == "--help") {
            std::cout << "pocket-audio-server — stream PC speakers to a browser\n";
            return 0;
        }
    }

#if defined(_WIN32)
    ix::initNetSystem();
#endif
    std::signal(SIGINT, onSignal);

#if defined(__APPLE__)
    auto capture = SystemCapture::create(pocket_audio::capture::MacBackend::ScreenCapture);
#else
    auto capture = SystemCapture::create();
#endif
    if (!capture) {
        std::cerr << "capture not supported on this platform\n";
        return 1;
    }

    const auto socket = createSocket();
    if (socket == kInvalidSocket) {
        std::cerr << "failed to start WebSocket server\n";
        return 1;
    }

    const auto on_chunk = [socket](const AudioChunk& chunk) {
        if (sendAudioData(socket, chunk)) ++g_chunksSent;
    };

    if (!capture->start(on_chunk)) {
        std::cerr << "capture failed: " << capture->lastError() << "\n";
        closeSocket(socket);
        return 1;
    }

    std::cout << "WebSocket on port 9000 — tap Listen in the browser (Ctrl+C to stop)\n";

    auto last_report = std::chrono::steady_clock::now();
    while (g_run && capture->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const auto now = std::chrono::steady_clock::now();
        if (now - last_report >= std::chrono::seconds(3)) {
            const int sent = g_chunksSent.exchange(0);
            std::cout << "  to browser: " << (sent / 3) << " chunks/s\n";
            last_report = now;
        }
    }

    g_run = false;
    capture->stop();
    closeSocket(socket);
#if defined(_WIN32)
    ix::uninitNetSystem();
#endif
    return 0;
}
