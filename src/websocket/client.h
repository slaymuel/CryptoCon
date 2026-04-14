/// @file client.h
/// @brief Multi-endpoint WebSocket client for real-time exchange data streaming.

#pragma once

#include <string>
#include <memory>
#include <functional>
#include "../utils/utils.h"

namespace trade_connector::websocket {

/// Multi-endpoint WebSocket client. Manages concurrent connections with
/// per-endpoint callbacks and automatic reconnection. Non-copyable/movable.
class Client {
    
public:
    // ixwebsocket already use std::function for callbacks, so 
    // we can just forward those types.
    using MessageCallback = std::function<void(std::string_view)>;
    using OnWSOpen        = std::function<void(const std::string&)>;

    /// Construct with API credentials and optional logger.
    Client(
        const std::string& ws_host,
        const std::string& api_key, 
        const std::string& secret_key,
        std::function<void(const std::string&)> logger = trade_connector::null_logger
    );
    

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    /// Disconnects all endpoints on destruction.
    ~Client();

    #ifdef ENABLE_NATIVE_WS_ACCESS
    ix::WebSocket* getWebSocket(const std::string& endpoint) const {
        auto it = connections.find(endpoint);
        if (it != connections.end() && it->second->ws) {
            return it->second->ws.get();
        }
        return nullptr;
    }
    #endif

    void connectEndpoint(
        MessageCallback callback,
        const std::string& host,
        const std::string& path,
        OnWSOpen on_open = [](const std::string&) {}

    ) {
        connectEndpointImpl(
            callback,
            host,
            path,
            on_open
        );
    }

    /// Send a text message to a connected endpoint. Returns false on failure.
    bool send(const std::string& endpoint, const std::string& message) const;

    /// True if the endpoint has an open connection.
    bool isConnected(const std::string& endpoint) const;

    /// Gracefully close and remove a single endpoint connection.
    void disconnect(const std::string& endpoint);

    void disconnectAll();

    /// Number of active connections (any state).
    size_t connectionCount() const;

    /// Block until all connections close. Cleans up dead connections periodically.
    void wait();

private:
    void addConnection(    
        MessageCallback callback,
        const std::string& host,
        const std::string& path,
        OnWSOpen on_open
    );

    void connectEndpointImpl(
        MessageCallback callback,
        const std::string& host,
        const std::string& path,
        OnWSOpen on_open
    ) {
        addConnection(
            callback,
            host,
            path,
            on_open
        );
    }
    
    const std::string ws_host;
    const std::string api_key;       ///< API key for authenticated endpoints (if needed)
    const std::string secret_key;    ///< Secret key for signing (if needed)
    struct ConnectionData;
    std::unordered_map<std::string, std::unique_ptr<ConnectionData>> connections;
    std::function<void(const std::string&)> logger;
};

} // namespace trade_connector::websocket
