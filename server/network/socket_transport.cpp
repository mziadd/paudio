// WebSocket server — speaker audio downlink to browsers.

#include "network/socket_transport.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <algorithm>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "core/format.hpp"

namespace pocket_audio::network {

namespace {

using ClientPtr = std::shared_ptr<ix::WebSocket>;

struct Server {
    std::unique_ptr<ix::WebSocketServer> ws;
    std::vector<ClientPtr>               clients;
    std::mutex                           mutex;
};

std::unique_ptr<Server> g_server;

// Caller must hold g_server->mutex.
void printAllSocketStatusesUnlocked(const char* label) {
    std::cerr << "  [" << label << "] " << g_server->clients.size() << " client(s):\n";
    for (std::size_t i = 0; i < g_server->clients.size(); ++i) {
        const auto& c = g_server->clients[i];
        if (!c) {
            std::cerr << "    #" << i << " (null)\n";
            continue;
        }
        std::cerr << "    #" << i << " "
                  << ix::WebSocket::readyStateToString(c->getReadyState()) << "\n";
    }
}

void printAllSocketStatuses(const char* label) {
    if (!g_server) return;
    std::lock_guard<std::mutex> lock(g_server->mutex);
    printAllSocketStatusesUnlocked(label);
}

void addClient(const ClientPtr& ws) {
    if (!g_server || !ws) return;
    std::lock_guard<std::mutex> lock(g_server->mutex);
    for (const auto& c : g_server->clients) {
        if (c.get() == ws.get()) return;
    }
    g_server->clients.push_back(ws);
    std::cerr << "  browser connected (" << g_server->clients.size() << " total)\n";
    printAllSocketStatusesUnlocked("connect");
}

void removeClient(const ClientPtr& ws, const std::string& reason = {}) {
    if (!g_server || !ws) return;
    std::lock_guard<std::mutex> lock(g_server->mutex);
    auto& list = g_server->clients;
    const auto before = list.size();
    list.erase(std::remove(list.begin(), list.end(), ws), list.end());
    if (list.size() == before) return;

    std::cerr << "  browser disconnected (" << list.size() << " remaining)";
    if (!reason.empty()) std::cerr << " — " << reason;
    std::cerr << "\n";
    printAllSocketStatusesUnlocked("disconnect");
}

}  // namespace

SocketHandle createSocket(const ServerConfig& config) {
    g_server     = std::make_unique<Server>();
    g_server->ws = std::make_unique<ix::WebSocketServer>(config.port, config.bind_host);

    g_server->ws->setOnConnectionCallback(
        [](std::weak_ptr<ix::WebSocket> weakWs, std::shared_ptr<ix::ConnectionState>) {
            auto ws = weakWs.lock();
            if (!ws) return;

            addClient(ws);
            ws->disablePerMessageDeflate();

            ws->setOnMessageCallback([ws](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Close) {
                    std::string detail = "code=" + std::to_string(msg->closeInfo.code);
                    if (!msg->closeInfo.reason.empty()) {
                        detail += " reason=\"" + msg->closeInfo.reason + "\"";
                    }
                    removeClient(ws, detail);
                    return;
                }
                if (msg->type == ix::WebSocketMessageType::Open) {
                    std::cerr << "  socket open\n";
                    printAllSocketStatuses("open");
                    return;
                }
                if (msg->type == ix::WebSocketMessageType::Error) {
                    std::cerr << "  socket error";
                    if (!msg->errorInfo.reason.empty()) {
                        std::cerr << ": " << msg->errorInfo.reason;
                    }
                    std::cerr << "\n";
                    printAllSocketStatuses("error");
                }
            });
        });

    if (!g_server->ws->listen().first) {
        std::cerr << "failed to listen on port " << config.port << "\n";
        g_server.reset();
        return kInvalidSocket;
    }

    g_server->ws->start();
    std::cerr << "  WebSocket listening on " << config.bind_host << ":" << config.port << "\n";
    return 1;
}

bool sendAudioData(SocketHandle socket, const AudioChunk& chunk) {
    if (socket == kInvalidSocket || !g_server || !chunk.hasValidSize()) return false;

    thread_local std::vector<ClientPtr> snapshot;
    snapshot.clear();
    {
        std::lock_guard<std::mutex> lock(g_server->mutex);
        if (g_server->clients.empty()) return false;
        snapshot = g_server->clients;
    }

    thread_local std::string payload;
    if (payload.capacity() < kChunkBytes) payload.reserve(kChunkBytes);
    payload.assign(reinterpret_cast<const char*>(chunk.data()), chunk.byteSize());

    bool sent = false;
    for (const auto& client : snapshot) {
        if (client && client->getReadyState() == ix::ReadyState::Open) {
            client->sendBinary(payload);
            sent = true;
        }
    }
    return sent;
}

void closeSocket(SocketHandle socket) {
    (void)socket;
    if (!g_server) return;
    std::cerr << "  shutting down WebSocket server\n";
    printAllSocketStatuses("shutdown");
    g_server->ws->stop();
    g_server.reset();
}

}  // namespace pocket_audio::network
