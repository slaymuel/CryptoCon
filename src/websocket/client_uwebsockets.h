#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <App.h>

namespace trade_connector::websocket {

struct ConnectionData {
    std::string endpoint;
    void (*callback)(std::string_view);
};

class Client{
public:

    Client(){
        app = std::make_unique<uWS::SSLApp>(uWS::SocketContextOptions{
            .key_file_name = nullptr,
            .cert_file_name = nullptr,
            .passphrase = nullptr,
            .dh_params_file_name = nullptr,
            .ca_file_name = nullptr,
            .ssl_ciphers = nullptr,
            .ssl_prefer_low_memory_usage = 0
        });

        // Configure WebSocket behavior - handles ALL connections
        app->ws<ConnectionData>("/*", {
            // No compression
            .compression = uWS::DISABLED,
            // Maximum message size
            .maxPayloadLength = 64 * 1024,
            // Idle timeout in seconds
            .idleTimeout = 120,
            // Maximum backpressure before dropping the connection
            .maxBackpressure = 1 * 1024 * 1024,
            // No upgrade handler (HTTP->WS)
            .upgrade = nullptr,
            // Connection established
            .open = [](auto *ws) {
                auto *data = ws->getUserData();
                std::cout << "Connected to " << data->endpoint << std::endl;
            },
            // Message received - calls the per-connection callback
            .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
                auto *data = ws->getUserData();
                try {
                    if (data->callback) {
                        data->callback(message);
                    }
                } catch (std::exception &e) {
                    std::cerr << "Error in callback: " << e.what() << std::endl;
                }
            },

            .drain = [](auto *ws) {
                // Handle backpressure
            },
            
            .ping = [](auto *ws, std::string_view message) {
                // Auto-handled by uWS
            },
            
            .pong = [](auto *ws, std::string_view message) {
                // Auto-handled by uWS
            },
            
            // Connection closed
            .close = [this](auto *ws, int code, std::string_view message) {
                auto *data = ws->getUserData();
                std::cout << "Connection closed to " << data->endpoint 
                         << " with code " << code << std::endl;
                
                // Remove from active connections
                //connections.erase(data->endpoint);
            }
        });
    }

    ~Client() {

    }

    void connectEndpoint(
        void(*callback)(const std::string_view& msg), 
        const std::string host, 
        const std::string& path
    ) {
        std::string url = "wss://" + host + path;
        
        std::cout << "Connecting to " << url << std::endl;
        
        // Connect to the endpoint
        app->connect(url, [this, url, callback](auto *ws) {
            if (!ws) {
                std::cerr << "Failed to connect to " << url << std::endl;
            } else {
                std::cout << "Connection initiated to " << url << std::endl;
                
                // Set per-connection data
                auto *data = ws->getUserData();
                data->endpoint = url;
                data->callback = callback;
                
                // Track connection
                connections[url] = ws;
            }
        });
    }

    bool isConnected(const std::string& endpoint) {
        return connections.find(endpoint) != connections.end();
    }

    //----------------------------
    void start() {
        try {
            app->run();
        } catch (std::exception &e) {
            std::cout << "<BinaCPP_websocket::start> Error ! " << e.what() << std::endl;
        }
    }

private:
    std::unique_ptr<uWS::SSLApp> app;
    std::map<std::string, uWS::WebSocket<true, true, ConnectionData>> connections;
};

} // namespace trade_connector::websocket