/// @brief Synchronous HTTPS REST client with persistent keep-alive connection.
///
/// Uses Boost.Beast over OpenSSL. Supports GET, POST, PUT, DELETE.
/// Connection auto-reconnects on failure. Thread-unsafe — one client per thread.

#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include "../types.h"
#include "../utils/utils.h"

namespace trade_connector::rest {

class Client{
    using Headers = std::span<const std::pair<std::string, std::string>>;

public:
    /// @param host Exchange hostname (e.g. "api.binance.com")
    /// @throws boost::beast::system_error on connection/TLS failure
    Client(
        const std::string host,
        std::function<void(const std::string&)> logger = trade_connector::null_logger
    );

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    /** @brief Could implement these later if needed */
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    ~Client();

    /// Read HTTP response body from the persistent connection.
    std::string readFromStream(Error& error = dummy_error);

    /// Re-establish the HTTPS connection after failure.
    void reconnectStream();

    /// Send HTTP POST request. Returns response body.
    std::string post(
        const std::string& target,
        Headers headers = {},
        const std::string& body = "",
        Error& error = dummy_error
    );

    /// Send HTTP GET request. Returns response body.
    std::string get(
        const std::string& target,
        Headers headers = {},
        Error& error = dummy_error
    );

    /// Send HTTP PUT request. Returns response body.
    std::string put(
        const std::string& target,
        Headers headers = {},
        const std::string& body = "",
        Error& error = dummy_error
    );

    /// Send HTTP DELETE request. Returns response body.
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