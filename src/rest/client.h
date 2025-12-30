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

namespace trade_connector::rest {

enum class ProtocolType {
    JSON,
    SBE
};

/**
 * @brief Exchange endpoint configuration (plain data structure)
 * 
 * No virtual functions - zero overhead.
 * Users can create instances with their own endpoints.
 */
struct ExchangeConfig {
    std::string account_info;
    std::string ping;
    std::string server_time;
    std::string exchange_info;
    std::string depth;
    std::string trades;
    std::string order;
    std::string listen_key;
    ProtocolType default_protocol;
    bool supports_quote_order_qty;
};

// Factory functions for built-in exchanges
inline ExchangeConfig BinanceSpot() {
    return {
        "/api/v3/account",
        "/api/v3/ping",
        "/api/v3/time",
        "/api/v3/exchangeInfo",
        "/api/v3/depth",
        "/api/v3/trades",
        "/api/v3/order",
        "/api/v3/userDataStream",
        ProtocolType::JSON,
        true
    };
}

inline ExchangeConfig BinanceFutures() {
    return {
        "/fapi/v2/balance",
        "/fapi/v1/ping",
        "/fapi/v1/time",
        "/fapi/v1/exchangeInfo",
        "/fapi/v1/depth",
        "/fapi/v1/trades",
        "/fapi/v1/order",
        "/fapi/v1/listenKey",
        ProtocolType::JSON,
        false
    };
}

class Client{

public:

    Client(
        const std::string& host, 
        const std::string& api_key, 
        const std::string& secret_key, 
        const ExchangeConfig& config = BinanceFutures()
    ) : 
      host(host), api_key(api_key), secret_key(secret_key), config(config),
      ssl_ctx(boost::asio::ssl::context::sslv23_client), stream(ioc, ssl_ctx) {

        ssl_ctx.set_default_verify_paths();
        connect();
    }

    ~Client() {
        boost::beast::error_code ec;
        stream.shutdown(ec);
    }

    std::string createListenKey(){
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        return post(config.listen_key, headers);
    }

    std::string keepAliveListenKey(const std::string& listen_key){
        std::string target = config.listen_key + "?listenKey=" + listen_key;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        return put(target, headers);
    }

    std::string closeListenKey(const std::string& listen_key){
        std::string target = config.listen_key + "?listenKey=" + listen_key;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        return del(target, headers);
    }

    std::string getAccountInfo(){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                  + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = config.account_info + "?" + query + "&signature=" + signature;
        //std::string target = "/api/v3/account?" + query + "&signature=" + signature;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };

        return get(target, headers);
    }

    std::string ping(){
        std::string target = config.ping;
        return get(target);
    }

    std::string getServerTime(){
        std::string target = config.server_time;
        return get(target);
    }

    std::string getExchangeInfo(){
        std::string target = config.exchange_info;
        return get(target);
    }

    std::string getOrderBook(const std::string& symbol, int limit = 100){
        std::string target = config.depth + "?symbol=" + symbol + "&limit=" + std::to_string(limit);
        return get(target);
    }

    std::string getRecentTrades(const std::string& symbol, int limit = 500){
        std::string target = config.trades + "?symbol=" + symbol + "&limit=" + std::to_string(limit);
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
    std::string sendOrder(
        std::string symbol, 
        std::string side, 
        std::string type,
        double price = 0.0,
        double quantity = 0.0,
        double quote_quantity = 0.0,
        std::string time_in_force = ""
    ){
        std::string query = "symbol=" + symbol
            + "&side=" + side
            + "&type=" + type;
        
        // Add quantity OR quoteOrderQty (not both)
        if (quantity > 0.0) {
            query += "&quantity=" + std::to_string(quantity);
        } else if (quote_quantity > 0.0) {
            query += "&quoteOrderQty=" + std::to_string(quote_quantity);
        }
        
        // Add price for LIMIT orders
        if (price > 0.0) {
            query += "&price=" + std::to_string(price);
        }
        
        // Add timeInForce for LIMIT orders (not for MARKET)
        if (!time_in_force.empty() && type != "MARKET") {
            query += "&timeInForce=" + time_in_force;
        }
        
        query += "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = config.order + "?" + query + "&signature=" + signature;
        //std::string target = "/api/v3/order?" + query + "&signature=" + signature;
        std::vector<std::pair<std::string, std::string>> headers = {
            {"X-MBX-APIKEY", api_key}
        };
        return post(target, headers);
    }

    std::string post(const std::string& target,
              const std::vector<std::pair<std::string, std::string>>& headers = {},
              const std::string& body = "") {
        // Build HTTP POST request
        boost::beast::http::request<boost::beast::http::string_body> req{
            boost::beast::http::verb::post, target, 
            11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.body() = body;
        req.prepare_payload();
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        // Send request
        boost::beast::http::write(stream, req);
    
        // Receive response
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res);

        return res.body();
    }

        std::string get(const std::string&  target,
                    const std::vector<std::pair<std::string, std::string>>& headers = {}) {
        // Build HTTP GET request
        // HTTP version 1.1 is represented by 11
        boost::beast::http::request<boost::beast::http::empty_body> req{boost::beast::http::verb::get, target, 11};
        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        // Send request
        boost::beast::http::write(stream, req);
    
        // Receive response
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res);
        
        // Probably RVO but just to be sure
        return res.body();
    }

    std::string put(const std::string& target,
              const std::vector<std::pair<std::string, std::string>>& headers = {},
              const std::string& body = "") {
        boost::beast::http::request<boost::beast::http::string_body> req{
            boost::beast::http::verb::put, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.body() = body;
        req.prepare_payload();
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        boost::beast::http::write(stream, req);
    
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res);

        return res.body();
    }

    std::string del(const std::string& target,
              const std::vector<std::pair<std::string, std::string>>& headers = {}) {
        boost::beast::http::request<boost::beast::http::empty_body> req{
            boost::beast::http::verb::delete_, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        boost::beast::http::write(stream, req);
    
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res);

        return res.body();
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
    const ExchangeConfig& config;
    boost::asio::io_context ioc;
    boost::asio::ssl::context ssl_ctx;
    boost::asio::ssl::stream<boost::beast::tcp_stream> stream;
};

} // namespace trade_connector::rest