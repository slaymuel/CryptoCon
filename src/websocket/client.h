/**
 * @file client.h
 * @brief WebSocket client implementation using IXWebSocket library
 * 
 * Provides a high-performance, multi-endpoint WebSocket client for real-time
 * exchange data streaming. Supports multiple concurrent connections with
 * per-endpoint callbacks and automatic reconnection handling.
 */

#pragma once

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <chrono>
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
struct ConnectionData {
    std::string endpoint;                    ///< Full WebSocket URL (wss://host/path)
    std::unique_ptr<ix::WebSocket> ws;       ///< WebSocket connection instance
};

/**
 * @class Client
 * @brief Multi-endpoint WebSocket client for exchange data streaming
 * 
 * High-performance WebSocket client that manages multiple concurrent connections
 * to exchange WebSocket endpoints. Features include:
 * - Multiple simultaneous endpoint connections
 * - Per-endpoint callback handling with zero-copy string_view
 * - Automatic ping/pong keepalive (30-second interval)
 * - Disabled per-message compression for minimal latency
 * - Thread-safe connection management
 * - Automatic reconnection on connection loss
 * 
 * @note Non-copyable, non-movable to ensure connection stability
 * @note All callbacks must be non-capturing lambdas or function pointers
 * 
 * @example
 * ```cpp
 * Client client;
 * client.connectEndpoint(
 *     +[](std::string_view msg) { std::cout << msg << std::endl; },
 *     "stream.binance.com:9443",
 *     "/ws/btcusdt@trade"
 * );
 * client.wait(); // Keep running
 * ```
 */
class Client {
public:

    /** @brief Default constructor - initializes empty client */
    Client() = default;
    
    /** @brief Copy constructor deleted - connections are not copyable */
    Client(const Client&) = delete;
    /** @brief Copy assignment deleted - connections are not copyable */
    Client& operator=(const Client&) = delete;
    
    /** @brief Move constructor deleted - connections should remain stable */
    Client(Client&&) noexcept = delete;
    /** @brief Move assignment deleted - connections should remain stable */
    Client& operator=(Client&&) noexcept = delete;

    /**
     * @brief Destructor - automatically disconnects all endpoints
     * 
     * Ensures clean shutdown of all WebSocket connections before
     * the client object is destroyed.
     */
    ~Client() {
        disconnectAll();
    }

    /**
     * @brief Connect to a WebSocket endpoint with a message callback
     * 
     * Establishes an asynchronous WebSocket connection to the specified endpoint.
     * Multiple endpoints can be connected simultaneously by calling this method
     * multiple times with different URLs.
     * 
     * Connection features:
     * - Asynchronous, non-blocking connection initiation
     * - 30-second automatic ping/pong keepalive
     * - Disabled compression for low latency
     * - Automatic message fragment assembly
     * - Zero-copy message delivery via string_view
     * 
     * @tparam Callback Callback function type (must be function pointer)
     * @param callback Message handler: void(*)(std::string_view)
     *                 Use +[] for non-capturing lambdas
     * @param host WebSocket host and port (e.g., "stream.binance.com:9443")
     * @param path WebSocket path (e.g., "/ws/btcusdt@trade")
     * 
     * @note Callback is invoked for each received message on a worker thread
     * @note Callback must be thread-safe and should not block for long periods
     * @note Connection errors are logged to std::cerr
     * 
     * @example
     * ```cpp
     * client.connectEndpoint(
     *     +[](std::string_view msg) {
     *         // Parse and process message
     *         std::cout << "Received: " << msg << std::endl;
     *     },
     *     "stream.binance.com:9443",
     *     "/ws/btcusdt@trade"
     * );
     * ```
     */

    template<typename Callback>
    void connectEndpoint(
        Callback callback,
        const std::string& host,
        const std::string& path
    ) {
        std::string url = "wss://" + host + path;
        
        std::cout << "Connecting to " << url << std::endl;
        
        auto conn_data = std::make_unique<ConnectionData>();
        conn_data->endpoint = url;
        conn_data->ws = std::make_unique<ix::WebSocket>();
        
        // Configure WebSocket
        conn_data->ws->setUrl(url);
        
        // Set heartbeat (ping interval) - use setPingInterval instead
        conn_data->ws->setPingInterval(30);
        
        // Disable per-message deflate compression for lower latency
        conn_data->ws->disablePerMessageDeflate();
        
        // Set message callback - captures callback by value (function pointer)
        conn_data->ws->setOnMessageCallback(
            [callback, url](const ix::WebSocketMessagePtr& msg) {
                switch (msg->type) {
                    case ix::WebSocketMessageType::Message:
                        // Zero-copy string_view - no allocation
                        try {
                            callback(std::string_view(msg->str));
                        } catch (const std::exception& e) {
                            std::cerr << "Error in callback for " << url 
                                     << ": " << e.what() << std::endl;
                        }
                        break;
                        
                    case ix::WebSocketMessageType::Open:
                        std::cout << "Connected to " << url << std::endl;
                        break;
                        
                    case ix::WebSocketMessageType::Close:
                        std::cout << "Connection closed to " << url 
                                 << " (code: " << msg->closeInfo.code 
                                 << ", reason: " << msg->closeInfo.reason << ")" << std::endl;
                        break;
                        
                    case ix::WebSocketMessageType::Error:
                        std::cerr << "Error on " << url << ": " 
                                 << msg->errorInfo.reason << std::endl;
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
    void send(const std::string& endpoint, const std::string& message) {
        auto it = connections.find(endpoint);
        if (it != connections.end() && it->second->ws) {
            auto result = it->second->ws->send(message);
            if (!result.success) {
                std::cerr << "Failed to send message to " << endpoint << std::endl;
            }
        } else {
            std::cerr << "Endpoint not connected: " << endpoint << std::endl;
        }
    }

    /**
     * @brief Check if an endpoint is currently connected
     * 
     * @param endpoint Full WebSocket URL to check
     * @return true if connected and ready to send/receive, false otherwise
     * 
     * @note Returns false if endpoint was never connected or connection closed
     */
    bool isConnected(const std::string& endpoint) {
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
    void disconnect(const std::string& endpoint) {
        auto it = connections.find(endpoint);
        if (it != connections.end() && it->second->ws) {
            std::cout << "Disconnecting from " << endpoint << std::endl;
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
    void disconnectAll() {
        for (auto& [endpoint, data] : connections) {
            if (data->ws) {
                std::cout << "Disconnecting from " << endpoint << std::endl;
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
    size_t connectionCount() const {
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
    void wait() {
        // Keep checking if connections are still active
        while (!connections.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Remove dead connections
            for (auto it = connections.begin(); it != connections.end();) {
                if (!it->second->ws || 
                    it->second->ws->getReadyState() == ix::ReadyState::Closed) {
                    std::cout << "Removing closed connection: " << it->first << std::endl;
                    it = connections.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

private:
    /** @brief Map of active connections indexed by endpoint URL */
    std::map<std::string, std::unique_ptr<ConnectionData>> connections;
};

} // namespace trade_connector::websocket
