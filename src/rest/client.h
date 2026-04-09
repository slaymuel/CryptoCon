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

#include <functional>
#include <memory>
#include <span>
#include <string>
#include "../types.h"

namespace trade_connector::rest {

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
    );

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    /** @brief Could implement these later if needed */
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    ~Client();

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
    std::string readFromStream(Error& error = dummy_error);

    /**
     * @brief Reconnect the SSL/TLS stream
     * 
     * Attempts to re-establish the HTTPS connection after a connection failure.
     * Called automatically by readFromStream() and writeToStream() on errors.
     * 
     * @note Logs reconnection attempt to std::cout
     */
    void reconnectStream();

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
    );

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
    );

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
    );

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
    );

private:

    void connect();


    // pimpl
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace trade_connector::rest