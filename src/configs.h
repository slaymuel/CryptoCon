#pragma once

namespace trade_connector {

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

} // namespace trade_connector