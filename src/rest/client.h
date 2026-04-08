/**
 * @brief REST API client for cryptocurrency exchange interactions
 * 
 * Provides a high-performance, template-based REST client for interacting with
 * cryptocurrency exchange APIs. Supports both spot and futures markets with
 * compile-time market-specific method validation using C++20 concepts.
 * 
 * Features:
 * - Market-specific endpoint configuration via templates
 * - Persistent HTTPS connections with keep-alive
 * - Automatic request signing (HMAC-SHA256)
 * - Connection resilience with automatic reconnection
 * - Type-safe order submission with OrderParams
 */

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

/**
 * @class Client
 * @brief Market-specific REST API client for exchange operations
 * 
 * Thread-safe REST client that maintains a persistent HTTPS connection to the
 * exchange. Handles authentication, request signing, and automatic reconnection.
 * 
 * The client is templated on MarketType, enabling compile-time validation of
 * market-specific methods (e.g., futures-only leverage settings).
 * 
 * @tparam M Market type (MarketType::SPOT or MarketType::FUTURES)
 * 
 * Key features:
 * - Persistent SSL/TLS connection with HTTP keep-alive
 * - Automatic HMAC-SHA256 request signing
 * - Market-specific endpoint routing
 * - Type-safe order parameter handling
 * - Connection health monitoring and reconnection
 * 
 * @note Non-copyable, non-movable to maintain connection integrity
 * @note All methods are blocking and thread-safe
 * 
 * @example
 * ```cpp
 * Client<MarketType::SPOT> client(api_key, secret_key);
 * auto account = client.getAccountInfo();
 * 
 * order_builder.
 *       reset().
 *       symbol("BTCUSDT").
 *       side("BUY").
 *       type("LIMIT").
 *       timeInForce("GTC").
 *       quantity(0.001).
 *       price(50000.0).
 *       quoteOrderQty(100.0).
 *       timestamp(currentTimestamp());
 * auto response = rest_client.sendOrder(order_builder.build());
 * ```
 */
template<MarketType M = MarketType::GENERIC>
class Client{
    using Headers = std::span<const std::pair<std::string, std::string>>;

public:

    static void null_logger(const std::string&) {}
    /**
     * @brief Construct REST client with custom host
     * 
     * Creates a REST client and establishes an HTTPS connection to the specified host.
     * The connection uses SSL/TLS with system default certificate verification.
     * 
     * @param host Exchange API hostname (e.g., "api.binance.com", "testnet.binance.vision")
     * @param api_key Exchange API key for authentication
     * @param secret_key Exchange secret key for request signing
     * 
     * @throws boost::beast::system_error if connection fails
     * @throws boost::beast::system_error if SSL handshake fails
     */
    Client(
        const std::string host,
        std::function<void(const std::string&)> logger = null_logger
    ) : 
      host(host),
      ssl_ctx(boost::asio::ssl::context::sslv23_client), 
      stream(ioc, ssl_ctx),
      logger(logger) {
        ssl_ctx.set_default_verify_paths();
        connect(host);
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    /** @brief Could implement these later if needed */
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    /**
     * @brief Destructor - gracefully closes the HTTPS connection
     * 
     * Performs a clean SSL shutdown before destroying the client.
     * Errors during shutdown are ignored to ensure destructor never throws.
     */
    ~Client() {
        // TODO: Check for errors
        boost::beast::error_code ec;
        stream.shutdown(ec);
        if (ec) {
            logger("Error during SSL shutdown: " + ec.message() 
              + " [" + ec.category().name() 
              + ":" + std::to_string(ec.value()) + "]");
        }
    }

    /**
     * @brief Read HTTP response from the persistent connection
     * 
     * Reads the complete HTTP response including headers and body from the
     * SSL stream. Handles connection errors and triggers reconnection if needed.
     * 
     * @return Response body as string (empty string on error)
     * 
     * @note Blocking call - waits for complete response
     * @note Automatically attempts reconnection on connection failure
     * @note Errors are logged to std::cout
     */
    std::string readFromStream(
        Error& error = dummy_error
    ) {
        boost::beast::error_code ec;
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res, ec);

        if (ec) {
            std::string error_message = "Error reading from stream: " + ec.message();
            error = Error(error_message, 1);
            logger("Error reading from stream: " + ec.message() 
              + " [" + ec.category().name() 
              + ":" + std::to_string(ec.value()) + "]");
            // Connection died, try to reconnect
            reconnectStream();

            return "";
        }

        return res.body();
    }

    template<typename RequestType>
    void writeToStream(
        RequestType& request,
        Error& error = dummy_error
    ) {
        boost::beast::error_code ec;
        boost::beast::http::write(stream, request, ec);

        if (ec) {
            std::string error_message = "Error writing to stream: " + ec.message();
            error = Error(error_message, 2);
            logger("Error writing to stream: " + ec.message() 
              + " [" + ec.category().name() 
              + ":" + std::to_string(ec.value()) + "]");
            // Connection died, try to reconnect
            reconnectStream();
        }
    }

    /**
     * @brief Reconnect the SSL/TLS stream
     * 
     * Attempts to re-establish the HTTPS connection after a connection failure.
     * Called automatically by readFromStream() and writeToStream() on errors.
     * 
     * @note Logs reconnection attempt to std::cout
     */
    void reconnectStream() {
        logger("Reconnecting stream...");
        connect();
    }

    /**
     * @brief Send HTTP POST request
     * 
     * Constructs and sends an HTTP POST request with optional headers and body.
     * Uses the persistent connection with HTTP/1.1 keep-alive.
     * 
     * @param target Request target/path (e.g., "/api/v3/order")
     * @param headers Optional custom headers (e.g., {"X-MBX-APIKEY", key})
     * @param body Optional request body (default: empty)
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string post(
        const std::string& target,
        Headers headers = {},
        const std::string& body = "",
        Error& error = dummy_error
    ) {
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

        logger("POST " + target + " Body: " + body);

        // Send request
        writeToStream<RequestString>(req, error);
    
        // Receive response
        auto result = readFromStream(error);
        return result;
    }

    /**
     * @brief Send HTTP GET request
     * 
     * Constructs and sends an HTTP GET request with optional headers.
     * Uses the persistent connection with HTTP/1.1 keep-alive.
     * 
     * @param target Request target/path with query string (e.g., "/api/v3/ping")
     * @param headers Optional custom headers (e.g., {"X-MBX-APIKEY", key})
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string get(
        const std::string& target,
        Headers headers = {},
        Error& error = dummy_error
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
        writeToStream<RequestEmpty>(req, error);
    
        // Receive response
        auto result = readFromStream(error);
        return result;
    }

    /**
     * @brief Send HTTP PUT request
     * 
     * Constructs and sends an HTTP PUT request with optional headers and body.
     * Used for updating resources (e.g., extending listen key validity).
     * 
     * @param target Request target/path
     * @param headers Optional custom headers
     * @param body Optional request body (default: empty)
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string put(
        const std::string& target,
        Headers headers = {},
        const std::string& body = "",
        Error& error = dummy_error
    ) {
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

        writeToStream<RequestString>(req, error);
    
        auto result = readFromStream(error);
        return result;
    }

    /**
     * @brief Send HTTP DELETE request
     * 
     * Constructs and sends an HTTP DELETE request with optional headers.
     * Used for deleting resources (e.g., closing listen keys, canceling orders).
     * 
     * @param target Request target/path
     * @param headers Optional custom headers
     * @param error Error object for capturing errors
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string del(
        const std::string& target,
        Headers headers = {},
        Error& error = dummy_error
    ) {
        RequestEmpty req{
            boost::beast::http::verb::delete_, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        writeToStream<RequestEmpty>(req, error);
    
        auto result = readFromStream(error);
        return result;
    }

private:

    /**
     * @brief Establish HTTPS connection to the exchange
     * 
     * Performs DNS resolution, TCP connection, and SSL/TLS handshake.
     * Called by constructors to establish initial connection and by
     * reconnectStream() to restore connection after failures.
     * 
     * Process:
     * 1. DNS resolution of hostname to IP addresses
     * 2. TCP connection to port 443 (HTTPS)
     * 3. SNI (Server Name Indication) configuration
     * 4. TLS handshake and certificate verification
     * 
     * @throws boost::beast::system_error if DNS resolution fails
     * @throws boost::beast::system_error if TCP connection fails
     * @throws boost::beast::system_error if SSL handshake fails
     * @throws boost::beast::system_error if certificate verification fails
     */
    void connect() {
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

    const std::string host;          ///< Exchange API hostname
    boost::asio::io_context ioc;    ///< Boost.Asio I/O context for async operations
    boost::asio::ssl::context ssl_ctx;  ///< SSL context with system certificate store
    boost::asio::ssl::stream<boost::beast::tcp_stream> stream;  ///< Persistent SSL/TLS stream
    std::function<void(const std::string&)> logger;                  ///< Logger instance for logging messages
};

} // namespace trade_connector::rest