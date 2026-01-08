#pragma once

#include <string_view>

namespace trade_connector {

enum class MarketType {
    SPOT,
    FUTURES
};

template<MarketType M>
concept IsFutures = M == MarketType::FUTURES;

template<MarketType M>
concept IsSpot = M == MarketType::SPOT;
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
template<MarketType M>
struct BinanceConfig;

template<>
struct BinanceConfig<MarketType::SPOT>{
    static constexpr const char* url = "api.binance.com";
    static constexpr const char* test_url = "testnet.binance.vision";
    static constexpr const char* account_info = "/api/v3/account";
    static constexpr const char* ping = "/api/v3/ping";
    static constexpr const char* server_time = "/api/v3/time";
    static constexpr const char* exchange_info = "/api/v3/exchangeInfo";
    static constexpr const char* depth = "/api/v3/depth";
    static constexpr const char* trades = "/api/v3/trades";
    static constexpr const char* order = "/api/v3/order";
    static constexpr const char* listen_key = "/api/v3/userDataStream";
    static constexpr const ProtocolType default_protocol = ProtocolType::JSON;
    static constexpr const bool supports_quote_order_qty = true;
};

template<>
struct BinanceConfig<MarketType::FUTURES>{
    static constexpr const char* url = "fapi.binance.com";
    static constexpr const char* test_url = "demo-fapi.binance.com";
    static constexpr const char* account_info = "/fapi/v2/balance";
    static constexpr const char* open_positions = "/fapi/v3/positionRisk";
    //"/fapi/v2/positionRisk", // Gets all symbols' positions
    static constexpr const char* ping = "/fapi/v1/ping";
    static constexpr const char* server_time = "/fapi/v1/time";
    static constexpr const char* exchange_info = "/fapi/v1/exchangeInfo";
    static constexpr const char* depth = "/fapi/v1/depth";
    static constexpr const char* trades = "/fapi/v1/trades";
    static constexpr const char* order = "/fapi/v1/order";
    static constexpr const char* listen_key = "/fapi/v1/listenKey";
    static constexpr const char* leverage = "/fapi/v1/leverage";
    static constexpr const ProtocolType default_protocol = ProtocolType::JSON;
    static constexpr const bool supports_quote_order_qty = false;
};

// TODO: Add configs for other exchanges (e.g., Coinbase, Kraken, etc.)

} // namespace trade_connector