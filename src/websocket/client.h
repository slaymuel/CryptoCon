/**
 * @file client.h
 * @brief WebSocket client implementation using IXWebSocket library
 * 
 * Provides a high-performance, multi-endpoint WebSocket client for real-time
 * exchange data streaming. Supports multiple concurrent connections with
 * per-endpoint callbacks and automatic reconnection handling.
 */

#pragma once

#include <string>
#include <memory>
#include <functional>

namespace trade_connector::websocket {

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
    using MessageCallback = void(*)(std::string_view);
    using OnWSOpen        = std::function<void(const std::string&)>;

    static void null_logger(const std::string&) {}

    /** @brief Default constructor - initializes empty client */
    Client(
        const std::string& api_key, 
        const std::string& secret_key,
        std::function<void(const std::string&)> logger = null_logger
    );
    

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    /** @brief Could implement these later if needed */
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    /**
     * @brief Destructor - automatically disconnects all endpoints
     * 
     * Ensures clean shutdown of all WebSocket connections before
     * the client object is destroyed.
     */
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
    bool send(const std::string& endpoint, const std::string& message) const;

    /**
     * @brief Check if an endpoint is currently connected
     * 
     * @param endpoint Full WebSocket URL to check
     * @return true if connected and ready to send/receive, false otherwise
     * 
     * @note Returns false if endpoint was never connected or connection closed
     */
    bool isConnected(const std::string& endpoint) const;

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
    void disconnect(const std::string& endpoint);

    void disconnectAll();

    /**
     * @brief Get the number of active connections
     * 
     * @return Number of currently connected endpoints
     * 
     * @note Count includes connections in any state (connecting, open, closing)
     */
    size_t connectionCount() const;

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

    const std::string api_key;       ///< API key for authenticated endpoints (if needed)
    const std::string secret_key;    ///< Secret key for signing (if needed)
    /** @brief Map of active connections indexed by endpoint URL */
    struct ConnectionData; // Forward declaration of per-connection data structure
    std::unordered_map<std::string, std::unique_ptr<ConnectionData>> connections;
    std::function<void(const std::string&)> logger;
};

} // namespace trade_connector::websocket
