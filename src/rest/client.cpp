#include <string>
#include <simdjson.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include "../types.h"
#include "client.h"

namespace trade_connector::rest {

/// HTTP request with string body (POST/PUT).
using RequestString = boost::beast::http::request<boost::beast::http::string_body>;

/// HTTP request with no body (GET/DELETE).
using RequestEmpty = boost::beast::http::request<boost::beast::http::empty_body>;

struct Client::Impl {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ssl_ctx;
    boost::asio::ssl::stream<boost::beast::tcp_stream> stream;
    std::function<void(const std::string&)> logger;
    const std::string host;

    Impl(std::string host, std::function<void(const std::string&)> logger)
    : ssl_ctx(boost::asio::ssl::context::sslv23_client)
    , stream(ioc, ssl_ctx)
    , host(std::move(host))
    , logger(std::move(logger)){
        ssl_ctx.set_default_verify_paths();
    }

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

    void reconnectStream() {
        logger("Reconnecting stream...");
        connect();
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
};

Client::Client(
    const std::string host,
    std::function<void(const std::string&)> logger
) 
: impl(std::make_unique<Impl>(host, std::move(logger))) {
    impl->connect();
}

Client::~Client() {
    // TODO: Check for errors
    boost::beast::error_code ec;
    impl->stream.shutdown(ec);
    if (ec) {
        impl->logger("Error during SSL shutdown: " + ec.message() 
            + " [" + ec.category().name() 
            + ":" + std::to_string(ec.value()) + "]");
    }
}

std::string Client::readFromStream(
    Error& error
) {
    boost::beast::error_code ec;
    boost::beast::flat_buffer buffer;
    boost::beast::http::response<boost::beast::http::string_body> res;
    boost::beast::http::read(impl->stream, buffer, res, ec);

    if (ec) {
        std::string error_message = "Error reading from stream: " + ec.message();
        error = Error(error_message, 1);
        impl->logger("Error reading from stream: " + ec.message() 
            + " [" + ec.category().name() 
            + ":" + std::to_string(ec.value()) + "]");
        // Connection died, try to reconnect
        reconnectStream();

        return "";
    }

    // RVO does not apply here because res.body() returns a reference.
    return std::move(res.body());
}

void Client::reconnectStream() {
    this->impl->reconnectStream();
}

std::string Client::post(
    const std::string& target,
    Headers headers,
    const std::string& body,
    Error& error
) {
    // Build HTTP POST request
    RequestString req{
        boost::beast::http::verb::post, target, 
        11
    };

    req.set(boost::beast::http::field::host, impl->host);
    req.set(boost::beast::http::field::user_agent, "trade-connector-client");
    req.set(boost::beast::http::field::connection, "keep-alive");
    req.body() = body;
    req.prepare_payload();

    for (auto& h : headers)
        req.set(h.first, h.second);

    impl->logger("POST " + target + " Body: " + body);

    // Send request
    impl->writeToStream<RequestString>(req, error);

    // Receive response
    auto result = readFromStream(error);
    return result;
}

std::string Client::get(
    const std::string& target,
    Headers headers,
    Error& error
) {
    // Build HTTP GET request
    // HTTP version 1.1 is represented by 11
    RequestEmpty req{boost::beast::http::verb::get, target, 11};
    req.set(boost::beast::http::field::host, impl->host);
    req.set(boost::beast::http::field::user_agent, "trade-connector-client");
    req.set(boost::beast::http::field::connection, "keep-alive");

    for (auto& h : headers)
        req.set(h.first, h.second);

    // Send request
    impl->writeToStream<RequestEmpty>(req, error);

    // Receive response
    auto result = readFromStream(error);
    return result;
}

std::string Client::put(
    const std::string& target,
    Headers headers,
    const std::string& body,
    Error& error
) {
    RequestString req{
        boost::beast::http::verb::put, target, 11
    };

    req.set(boost::beast::http::field::host, impl->host);
    req.set(boost::beast::http::field::user_agent, "trade-connector-client");
    req.set(boost::beast::http::field::connection, "keep-alive");
    req.body() = body;
    req.prepare_payload();

    for (auto& h : headers)
        req.set(h.first, h.second);

    impl->writeToStream<RequestString>(req, error);

    auto result = readFromStream(error);
    return result;
}

std::string Client::del(
    const std::string& target,
    Headers headers,
    Error& error
) {
    RequestEmpty req{
        boost::beast::http::verb::delete_, target, 11
    };

    req.set(boost::beast::http::field::host, impl->host);
    req.set(boost::beast::http::field::user_agent, "trade-connector-client");
    req.set(boost::beast::http::field::connection, "keep-alive");

    for (auto& h : headers)
        req.set(h.first, h.second);

    impl->writeToStream<RequestEmpty>(req, error);

    auto result = readFromStream(error);
    return result;
}

} // namespace trade_connector::rest