#include "client.h"
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <functional>
#include <ixwebsocket/IXWebSocket.h>

namespace trade_connector::websocket {

/**
 * @struct ConnectionData
 * @brief Per-connection data structure
 * 
 * Stores connection-specific information including the endpoint URL
 * and the WebSocket instance. Each active connection maintains its own
 * ConnectionData instance.
 */
struct Client::ConnectionData {
    std::string endpoint;                    ///< Full WebSocket URL (wss://host/path)
    std::unique_ptr<ix::WebSocket> ws;       ///< WebSocket connection instance
};

static void null_logger(const std::string&) {}

/** @brief Default constructor - initializes empty client */
Client::Client(
    const std::string& api_key, 
    const std::string& secret_key,
    std::function<void(const std::string&)> logger
) : api_key(api_key), secret_key(secret_key), logger(logger) {};

/**
    * @brief Destructor - automatically disconnects all endpoints
    * 
    * Ensures clean shutdown of all WebSocket connections before
    * the client object is destroyed.
    */
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
        [this, callback, url, ws_ptr, on_open](const ix::WebSocketMessagePtr& msg) {
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

/**
* @brief Send a message to a specific WebSocket endpoint
* 
* Sends a text message to the specified endpoint. The endpoint must be
* connected before sending messages.
* 
* @param endpoint Full WebSocket URL (must match a connected endpoint)
* @param message Message string to send
* 
* @note Silently fails if endpoint is not connected (error logged to std::cerr)
* @note This method is thread-safe
*/
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

/**
    * @brief Check if an endpoint is currently connected
    * 
    * @param endpoint Full WebSocket URL to check
    * @return true if connected and ready to send/receive, false otherwise
    * 
    * @note Returns false if endpoint was never connected or connection closed
    */
bool Client::isConnected(const std::string& endpoint) const {
    auto it = connections.find(endpoint);
    return it != connections.end() && 
            it->second->ws && 
            it->second->ws->getReadyState() == ix::ReadyState::Open;
}

/**
    * @brief Disconnect from a specific WebSocket endpoint
    * 
    * Gracefully closes the WebSocket connection and removes it from
    * the active connections list.
    * 
    * @param endpoint Full WebSocket URL to disconnect
    * 
    * @note Safe to call even if endpoint is not connected
    * @note Connection resources are immediately released
    */
void Client::disconnect(const std::string& endpoint) {
    auto it = connections.find(endpoint);
    if (it != connections.end() && it->second->ws) {
        logger("Disconnecting from " + endpoint);
        it->second->ws->stop();
        connections.erase(it);
    }
}

/**
    * @brief Disconnect from all WebSocket endpoints
    * 
    * Gracefully closes all active WebSocket connections and clears
    * the connections map. Typically called during shutdown.
    * 
    * @note This method is automatically called by the destructor
    */
void Client::disconnectAll() {
    for (auto& [endpoint, data] : connections) {
        if (data->ws) {
            logger("Disconnecting from " + endpoint);
            data->ws->stop();
        }
    }
    connections.clear();
}

/**
    * @brief Get the number of active connections
    * 
    * @return Number of currently connected endpoints
    * 
    * @note Count includes connections in any state (connecting, open, closing)
    */
size_t Client::connectionCount() const {
    return connections.size();
}

/**
    * @brief Wait for all connections to close (blocking)
    * 
    * Blocks the calling thread until all WebSocket connections have closed.
    * This is useful for keeping the main thread alive while WebSocket callbacks
    * handle messages on background threads.
    * 
    * The method periodically checks connection status and automatically removes
    * connections that have closed.
    * 
    * @note This method blocks indefinitely until all connections close
    * @note Dead connections are automatically cleaned up every 100ms
    * @note Use Ctrl+C or external signals to interrupt if needed
    * 
    * @example
    * ```cpp
    * Client client;
    * client.connectEndpoint(callback, host, path);
    * client.wait(); // Keep main thread alive
    * ```
    */
void Client::wait() {
    // Keep checking if connections are still active
    while (!connections.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Remove dead connections
        for (auto it = connections.begin(); it != connections.end();) {
            if (!it->second->ws || 
                it->second->ws->getReadyState() == ix::ReadyState::Closed) {
                logger("Removing closed connection: " + it->first);
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
    }
}

} // namespace trade_connector::websocket
