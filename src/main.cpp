#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>

#include "rest/client.h"
#include "websocket/client.h"
#include "configs.h"

int main() {
    std::string spot_api_key = "xFE3InwAv8mPtpQKZq6dorsahvb91gxpEvJSKuB1dbDL6X4Yy5GHOB5IMhdUtwhN";
    std::string spot_secret_key = "Sg9OzEuDdVpmb8QQNrMrBtj5FUNTLZC0SwlVsyH4lsKRxOnPAIg1bSIvrj1MTn3e";
    std::string future_api_key = "W249L82DXdkccN72iFAEVPuHWxhfPhJPVAEEdwNCsMfFF00k4pWJg5yQ4EqkjJxk";
    std::string future_secret_key = "x5ZeVuj4uCc2DHnGhBBpVQQv3Kfq8cYhlUlRNfncfPXeaxF9WPRWqrq2C9okR7CE";
    //auto config = trade_connector::BinanceFutures();
    auto config = trade_connector::BinanceSpot();
    //trade_connector::rest::Client client("demo-fapi.binance.com", future_api_key, future_secret_key, config);
    trade_connector::rest::Client rest_client("testnet.binance.vision", spot_api_key, spot_secret_key, config);
    trade_connector::websocket::Client ws_client;
    //trade_connector::websocket::Client ws_client("wss://fstream.binancefuture.com", spot_api_key, spot_secret_key, config);
    std::string listen_key = rest_client.createListenKey();
    std::cout << "Listen Key: " << listen_key << std::endl;
    //std::string response = client.get("/api/v3/time");
    //std::cout << "Response: " << response << std::endl;

    auto response = rest_client.getAccountInfo();
    std::cout << "Account Info: " << response << std::endl;
    //simdjson::ondemand::parser parser;
    //simdjson::padded_string json(response);
    //auto doc = parser.iterate(response);
    //for (simdjson::ondemand::value val : doc.get_array()) {
    //    simdjson::ondemand::object obj = val.get_object();
//
    //    std::string_view asset = obj["asset"].get_string();
    //    std::string_view balance = obj["balance"].get_string();
//
    //    std::cout << "Asset: " << asset
    //            << " Balance: " << balance << "\n";
    //}

    std::cout << rest_client.getServerTime() << std::endl;
    std::cout << std::endl;
    std::cout << rest_client.ping() << std::endl;
    std::cout << std::endl;
    //std::cout << rest_client.getExchangeInfo() << std::endl;
    //std::cout << std::endl;
    std::cout << rest_client.getOrderBook("BTCUSDT", 5) << std::endl;
    std::cout << std::endl;
    std::cout << rest_client.getRecentTrades("BTCUSDT", 5) << std::endl;
    std::cout << std::endl;
    std::cout << rest_client.sendOrder("BTCUSDT", "BUY", "MARKET", 0.0, 0.002, 10.0, "GTC") << std::endl;

    ws_client.connectEndpoint(
        +[](std::string_view msg) {
            try {
                std::cout << "Received message: " << msg << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error processing message: " << e.what() << std::endl;
            }
        },
        "stream.testnet.binance.vision",  // host
        "/ws/btcusdt@trade"                // path
        //"/ws/btcusdt@depth"                // path
    );
    ws_client.connectEndpoint(
        +[](std::string_view msg) {
            std::cout << "[USER DATA] " << msg << std::endl;
            // This will receive:
            // - executionReport: Order updates (filled, cancelled, etc.)
            // - outboundAccountPosition: Balance updates
            // - balanceUpdate: Balance changes
        },
        "stream.testnet.binance.vision",
        "/ws/" + listen_key  // Use the listen key as the path
    );
    // Wait a moment for connections to establish
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Active connections: " << ws_client.connectionCount() << std::endl;
    
    // Keep listening for messages
    std::cout << "Listening for messages... Press Ctrl+C to exit" << std::endl;
    ws_client.wait();  // Blocks until all connections close

    return 0;
}
