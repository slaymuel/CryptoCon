#pragma once

#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <chrono>
#include <ixwebsocket/IXWebSocket.h>

namespace trade_connector::websocket {

struct ConnectionData {
    std::string endpoint;
    std::unique_ptr<ix::WebSocket> ws;
};

class Client {
public:

    Client() = default;
    // Delete copy constructor and assignment
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    // Allow move operations
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    ~Client() {
        disconnectAll();
    }

    /**
     * @brief Connect to WebSocket endpoint. Can be called multiple times to connect multiple endpoints.
     * 
     * @param callback Function pointer (use +[] for non-capturing lambdas)
     * @param host WebSocket host (e.g., "stream.binance.com:9443")
     * @param path WebSocket path (e.g., "/ws/btcusdt@trade")
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
     * @brief Send message to a specific endpoint
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
     * @brief Check if endpoint is connected
     */
    bool isConnected(const std::string& endpoint) {
        auto it = connections.find(endpoint);
        return it != connections.end() && 
               it->second->ws && 
               it->second->ws->getReadyState() == ix::ReadyState::Open;
    }

    /**
     * @brief Disconnect from specific endpoint
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
     * @brief Disconnect all endpoints
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
     * @brief Get number of active connections
     */
    size_t connectionCount() const {
        return connections.size();
    }

    /**
     * @brief Wait for all connections to close (blocking)
     * Useful for keeping main thread alive
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
    std::map<std::string, std::unique_ptr<ConnectionData>> connections;
};

} // namespace trade_connector::websocket
