#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <simdjson.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include "../utils/utils.h"
#include "../types.h"
#include "../configs.h"

namespace trade_connector::rest {

template<MarketType M>
class Client{
    using Config = BinanceConfig<M>;

public:

    /**
     * @brief Create a REST client.
     * 
     * @param host The host URL to connect to
     * @param api_key The API key
     * @param secret_key The secret key
     */
    Client(
        const std::string& host, 
        const std::string& api_key, 
        const std::string& secret_key
    ) : 
      host(host), api_key(api_key), secret_key(secret_key),
      ssl_ctx(boost::asio::ssl::context::sslv23_client), stream(ioc, ssl_ctx) {

        ssl_ctx.set_default_verify_paths();
        connect();
    }

    /**
     * @brief Create a REST client. Uses the default binance REST URL
     * 
     * @param api_key The API key
     * @param secret_key The secret key
     */
    Client(
        const std::string& api_key, 
        const std::string& secret_key
    ) : 
      host(Config::test_url), api_key(api_key), secret_key(secret_key),
      ssl_ctx(boost::asio::ssl::context::sslv23_client), stream(ioc, ssl_ctx) {

        ssl_ctx.set_default_verify_paths();
        connect();
    }

    // Delete copy constructor and assignment
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    // Allow move operations
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    ~Client() {
        // TODO: Check for errors
        boost::beast::error_code ec;
        stream.shutdown(ec);
    }

    std::string createListenKey(){
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };

        auto response = post(Config::listen_key, headers);
        simdjson::ondemand::parser parser;
        simdjson::padded_string json(response);
        auto doc = parser.iterate(json);
        return std::string(doc["listenKey"].get_string().value());
    }

    std::string keepAliveListenKey(const std::string& listen_key){
        std::string target = std::string(Config::listen_key) + "?listenKey=" + listen_key;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        return put(target, headers);
    }

    std::string closeListenKey(const std::string& listen_key){
        std::string target = std::string(Config::listen_key) + "?listenKey=" + listen_key;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        return del(target, headers);
    }

    std::string getAccountInfo(){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                  + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::account_info) + "?" + query + "&signature=" + signature;
        //std::string target = "/api/v3/account?" + query + "&signature=" + signature;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };

        return get(target, headers);
    }

    std::string getOpenPositions() requires(IsFutures<M>){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                  + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::open_positions) + "?" + query + "&signature=" + signature;
        //std::string target = "/fapi/v2/positionRisk?" + query + "&signature=" + signature;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };

        return get(target, headers);
    }

    std::string ping(){
        std::string target = Config::ping;
        return get(target);
    }

    std::string getServerTime(){
        std::string target = Config::server_time;
        return get(target);
    }

    std::string getExchangeInfo(){
        std::string target = Config::exchange_info;
        return get(target);
    }

    std::string getOrderBook(const std::string& symbol, int limit = 100){
        std::string target = std::string(Config::depth) + "?symbol=" + symbol + "&limit=" + std::to_string(limit);
        return get(target);
    }

    std::string getRecentTrades(const std::string& symbol, int limit = 500){
        std::string target = std::string(Config::trades) + "?symbol=" + symbol + "&limit=" + std::to_string(limit);
        return get(target);
    }

    /**
     * @brief Send a new order
     * 
     * @param symbol Trading pair symbol (e.g., "BTCUSDT")
     * @param side Order side (BUY or SELL)
     * @param type Order type, possible values: LIMIT, MARKET, STOP, STOP_MARKET, TAKE_PROFIT, 
     *              TAKE_PROFIT_MARKET, LIMIT_MAKER
     * @param quote_quantity Quote asset quantity to spend/receive
     * @param timeInForce Time in force policy (default: GTC). Possible values: GTC, IOC, FOK, GTX, 
     *                    GTD
     * @return JSON string containing order details
     */

    std::string sendOrder(const OrderParams<M>& params) {
        std::string query = buildQuery(params);
        return sendOrder(query);
    }

    std::string buildQuery(const OrderParams<MarketType::SPOT>& params) {
        std::string query = "symbol=" + params.symbol
            + "&side=" + params.side
            + "&type=" + params.type;
        
        // Add quantity OR quoteOrderQty (not both)
        if (params.quantity > 0.0) {
            query += "&quantity=" + std::to_string(params.quantity);
        } else if (params.quote_quantity > 0.0) {
            query += "&quoteOrderQty=" + std::to_string(params.quote_quantity);
        }
        
        // Add price for LIMIT orders
        if (params.price > 0.0) {
            query += "&price=" + std::to_string(params.price);
        }
        
        // Add timeInForce for LIMIT orders (not for MARKET)
        if (!params.time_in_force.empty() && params.type != "MARKET") {
            query += "&timeInForce=" + params.time_in_force;
        }
        
        if (params.timestamp > 0) {
            query += "&timestamp=" + std::to_string(params.timestamp);
        }
        else {
            query += "&timestamp=" + std::to_string(currentTimestamp());
        }
        return query;
    }

    std::string buildQuery(const OrderParams<MarketType::FUTURES>& params) {
        std::string query = "symbol=" + params.symbol
            + "&side=" + params.side
            + "&type=" + params.type;
        
        // Add quantity
        if (params.quantity > 0.0) {
            query += "&quantity=" + std::to_string(params.quantity);
        }
        
        // Add price for LIMIT orders
        if (params.price > 0.0) {
            query += "&price=" + std::to_string(params.price);
        }
        
        // Add timeInForce for LIMIT orders (not for MARKET)
        if (!params.time_in_force.empty() && params.type != "MARKET") {
            query += "&timeInForce=" + params.time_in_force;
        }

        // Add reduceOnly flag
        if (params.reduce_only) {
            query += "&reduceOnly=true";
        }

        if (params.timestamp > 0) {
            query += "&timestamp=" + std::to_string(params.timestamp);
        }
        return query;
    }

    std::string setLeverage(const std::string& symbol, int leverage) requires(IsFutures<M>) {
        std::string query = "symbol=" + symbol
            + "&leverage=" + std::to_string(leverage)
            + "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = "/fapi/v1/leverage?" + query + "&signature=" + signature;
        
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
    
        return post(target, headers);
    }

    std::string setMarginType(const std::string& symbol, const std::string& margin_type) requires(IsFutures<M>) {
        std::string query = "symbol=" + symbol
            + "&marginType=" + margin_type
            + "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = "/fapi/v1/marginType?" + query + "&signature=" + signature;
        
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
    
        return post(target, headers);
    }

    std::string sendOrder(std::string query) {
        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::order) + "?" + query + "&signature=" + signature;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        
        return post(target, headers);
    }

    std::string readFromStream() {
        boost::beast::error_code ec;
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res, ec);

        if (ec) {
            std::cout << "Error reading from stream: " << ec.message() 
              << " [" << ec.category().name() 
              << ":" << ec.value() << "]" << std::endl;
            // Connection died, try to reconnect
            reconnectStream();

            return "";
        }

        return res.body();
    }

    template<typename RequestType>
    void writeToStream(RequestType& request) {
        boost::beast::error_code ec;
        boost::beast::http::write(stream, request, ec);

        if (ec) {
            std::cout << "Error writing to stream: " << ec.message() 
              << " [" << ec.category().name() 
              << ":" << ec.value() << "]" << std::endl;
            // Connection died, try to reconnect
            reconnectStream();
        }
    }

    void reconnectStream() {
        std::cout << "Reconnecting stream..." << std::endl;
        connect();
    }

    std::string post(const std::string& target,
              const std::vector<std::pair<std::string, std::string>>& headers = {},
              const std::string& body = "") {
        // Build HTTP POST request
        RequestString req{
            boost::beast::http::verb::post, target, 
            11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
        req.body() = body;
        req.prepare_payload();
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        // Send request
        writeToStream<RequestString>(req);
    
        // Receive response
        auto result = readFromStream();
        return result;
    }

    std::string get(const std::string&  target,
                    const std::vector<std::pair<std::string, std::string>>& headers = {}
    ) {
        // Build HTTP GET request
        // HTTP version 1.1 is represented by 11
        RequestEmpty req{boost::beast::http::verb::get, target, 11};
        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        // Send request
        writeToStream<RequestEmpty>(req);
    
        // Receive response
        auto result = readFromStream();
        return result;
    }

    std::string put(const std::string& target,
              const std::vector<std::pair<std::string, std::string>>& headers = {},
              const std::string& body = "") {
        RequestString req{
            boost::beast::http::verb::put, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
        req.body() = body;
        req.prepare_payload();
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        writeToStream<RequestString>(req);
    
        auto result = readFromStream();
        return result;
    }

    std::string del(const std::string& target,
              const std::vector<std::pair<std::string, std::string>>& headers = {}) {
        RequestEmpty req{
            boost::beast::http::verb::delete_, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        writeToStream<RequestEmpty>(req);
    
        auto result = readFromStream();
        return result;
    }

private:

    void connect(){
        // Resolve host into ip addresses
        boost::asio::ip::tcp::resolver resolver(ioc);
        // Port 443 is the standard HTTPS port.
        auto const results = resolver.resolve(host, "443");
    
        // Connect to IP over TCP. TLS layer on top of TCP layer
        // Connect the TCP layer before TLS can start.
        boost::beast::get_lowest_layer(stream).connect(results);
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        {
            boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::beast::system_error{ec};
        }
        // TLS handshake. upgrade TCP connection to a secure HTTPS connection
        stream.handshake(boost::asio::ssl::stream_base::client);
    }

    const std::string host;
    const std::string api_key;
    const std::string secret_key;
    boost::asio::io_context ioc;
    boost::asio::ssl::context ssl_ctx;
    boost::asio::ssl::stream<boost::beast::tcp_stream> stream;
};

} // namespace trade_connector::rest