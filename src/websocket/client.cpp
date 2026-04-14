#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>
#include "client.h"

namespace trade_connector::websocket {

/// Per-connection state: endpoint URL + WebSocket instance.
struct Client::ConnectionData {
    std::string endpoint;                    ///< Full WebSocket URL (wss://host/path)
    std::unique_ptr<ix::WebSocket> ws;       ///< WebSocket connection instance
    bool was_open = false;                   ///< True once the connection has been established
};

Client::Client(
    const std::string& ws_host,
    const std::string& api_key, 
    const std::string& secret_key,
    std::function<void(const std::string&)> logger
) : ws_host(ws_host), api_key(api_key), secret_key(secret_key), logger(logger) {};

Client::~Client() {
    disconnectAll();
}

void Client::addConnection(    
    MessageCallback callback,
    const std::string& host,
    const std::string& path,
    OnWSOpen on_open
){
    std::string url = "wss://" + host + path;
    logger("Connecting to " + url);

    auto conn_data = std::make_unique<ConnectionData>();
    conn_data->endpoint = url;
    conn_data->ws = std::make_unique<ix::WebSocket>();
    ix::WebSocket* ws_ptr = conn_data->ws.get();

    // Configure WebSocket
    conn_data->ws->setUrl(url);

    // Set heartbeat (ping interval) - use setPingInterval instead
    conn_data->ws->setPingInterval(30);

    // Disable per-message deflate compression for lower latency
    conn_data->ws->disablePerMessageDeflate();

    // Set message callback - captures callback by value (function pointer)
    conn_data->ws->setOnMessageCallback(
        [this, callback, url, ws_ptr, on_open, conn = conn_data.get()](const ix::WebSocketMessagePtr& msg) {
            switch (msg->type) {
                case ix::WebSocketMessageType::Message:
                    // Zero-copy string_view - no allocation
                    try {
                        callback(std::string_view(msg->str));
                    } catch (const std::exception& e) {
                        logger("Error in callback for " + url + ": " + e.what());
                    }
                    break;

                case ix::WebSocketMessageType::Open:
                    conn->was_open = true;
                    logger("Websocket connection to " + url + " established.");
                    try {
                        on_open(url);
                    } catch (const std::exception& e) {
                        logger("Error in on-open handler for " + url + ": " + e.what());
                    }
                    break;

                case ix::WebSocketMessageType::Close:
                    logger("Connection closed to " + url + " (code: " + std::to_string(msg->closeInfo.code) + ", reason: " + msg->closeInfo.reason + ")");
                    break;

                case ix::WebSocketMessageType::Error:
                    logger("WebSocket error on " + url + ": " + msg->errorInfo.reason);
                    break;

                case ix::WebSocketMessageType::Ping:
                    // Auto-handled by ixwebsocket
                    break;

                case ix::WebSocketMessageType::Pong:
                    // Auto-handled by ixwebsocket
                    break;

                case ix::WebSocketMessageType::Fragment:
                    // Fragments are automatically assembled
                    break;
            }
        }
    );

    // Start connection (async, non-blocking)
    conn_data->ws->start();

    connections[url] = std::move(conn_data);
}

bool Client::send(const std::string& endpoint, const std::string& message) const {
    auto it = connections.find(endpoint);
    if (it != connections.end() && it->second->ws) {
        auto result = it->second->ws->send(message);
        if (result.success) {
            return true;
        }
    } else {
        logger("Endpoint not connected: " + endpoint);
    }
    return false;
}

bool Client::isConnected(const std::string& endpoint) const {
    auto it = connections.find(endpoint);
    return it != connections.end() && 
            it->second->ws && 
            it->second->ws->getReadyState() == ix::ReadyState::Open;
}

void Client::disconnect(const std::string& endpoint) {
    auto it = connections.find(endpoint);
    if (it != connections.end() && it->second->ws) {
        logger("Disconnecting from " + endpoint);
        it->second->ws->stop();
        connections.erase(it);
    }
}

void Client::disconnectAll() {
    for (auto& [endpoint, data] : connections) {
        if (data->ws) {
            logger("Disconnecting from " + endpoint);
            data->ws->stop();
        }
    }
    connections.clear();
}

size_t Client::connectionCount() const {
    return connections.size();
}

void Client::wait() {
    // Keep checking if connections are still active
    while (!connections.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Remove dead connections (only those that were previously open)
        for (auto it = connections.begin(); it != connections.end();) {
            if (!it->second->ws || 
                (it->second->was_open && it->second->ws->getReadyState() == ix::ReadyState::Closed)) {
                logger("Removing closed connection: " + it->first);
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
    }
}

} // namespace trade_connector::websocket
