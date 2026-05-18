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
  std::vector<ClientPtr> clients;
  std::mutex mutex;
};

std::unique_ptr<Server> g_server;

void addClient(const ClientPtr &ws) {
  if (!g_server || !ws)
    return;
  std::lock_guard<std::mutex> lock(g_server->mutex);
  for (const auto &c : g_server->clients) {
    if (c.get() == ws.get())
      return;
  }
  g_server->clients.push_back(ws);
  std::cerr << "  browser connected (" << g_server->clients.size() << ")\n";
}

void removeClient(const ClientPtr &ws) {
  if (!g_server || !ws)
    return;
  std::lock_guard<std::mutex> lock(g_server->mutex);
  auto &list = g_server->clients;
  list.erase(std::remove(list.begin(), list.end(), ws), list.end());
  std::cerr << "  browser disconnected (" << list.size() << " left)\n";
}

} // namespace

SocketHandle createSocket(const ServerConfig &config) {
  g_server = std::make_unique<Server>();
  g_server->ws =
      std::make_unique<ix::WebSocketServer>(config.port, config.bind_host);

  g_server->ws->setOnConnectionCallback(
      [](std::weak_ptr<ix::WebSocket> weakWs,
         std::shared_ptr<ix::ConnectionState>) {
        auto ws = weakWs.lock();
        if (!ws)
          return;
        addClient(ws);
        ws->disablePerMessageDeflate();
        ws->setOnMessageCallback([ws](const ix::WebSocketMessagePtr &msg) {
          if (msg->type == ix::WebSocketMessageType::Close) {
            removeClient(ws);
          }
        });
      });

  if (!g_server->ws->listen().first) {
    std::cerr << "failed to listen on port " << config.port << "\n";
    g_server.reset();
    return kInvalidSocket;
  }

  g_server->ws->start();
  std::cerr << "  listening on " << config.bind_host << ":" << config.port
            << "\n";
  return 1;
}

bool sendAudioData(SocketHandle socket, const AudioChunk &chunk) {
  if (socket == kInvalidSocket || !g_server || !chunk.hasValidSize())
    return false;

  thread_local std::vector<ClientPtr> snapshot;
  thread_local std::string payload;
  {
    std::lock_guard<std::mutex> lock(g_server->mutex);
    if (g_server->clients.empty())
      return false;
    snapshot = g_server->clients;
  }

  if (payload.capacity() < kChunkBytes)
    payload.reserve(kChunkBytes);
  payload.assign(reinterpret_cast<const char *>(chunk.data()),
                 chunk.byteSize());

  bool sent = false;
  for (const auto &client : snapshot) {
    if (client && client->getReadyState() == ix::ReadyState::Open) {
      client->sendBinary(payload);
      sent = true;
    }
  }
  return sent;
}

void closeSocket(SocketHandle socket) {
  (void)socket;
  if (!g_server)
    return;
  g_server->ws->stop();
  g_server.reset();
}

} // namespace pocket_audio::network
