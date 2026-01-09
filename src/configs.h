/**
 * @file configs.h
 * @brief Exchange configuration templates for trading connectors
 * 
 * This file provides compile-time configuration for different exchanges and market types.
 * Configurations are defined as template specializations, ensuring zero runtime overhead
 * while maintaining type safety and flexibility.
 */

#pragma once

#include <string_view>

namespace trade_connector {

/**
 * @enum MarketType
 * @brief Defines the type of trading market
 * 
 * Used as a template parameter to specialize exchange configurations
 * and trading behavior for different market types.
 */
enum class MarketType {
    SPOT,      ///< Spot trading market (immediate settlement)
    FUTURES    ///< Futures/derivatives trading market (contract-based)
};

/**
 * @concept IsFutures
 * @brief Constrains template parameters to futures market type
 * 
 * This concept enables compile-time checks and method overloading
 * based on market type. Used with C++20 requires clauses.
 * 
 * @tparam M Market type to validate
 */
template<MarketType M>
concept IsFutures = M == MarketType::FUTURES;

/**
 * @concept IsSpot
 * @brief Constrains template parameters to spot market type
 * 
 * This concept enables compile-time checks and method overloading
 * based on market type. Used with C++20 requires clauses.
 * 
 * @tparam M Market type to validate
 */
template<MarketType M>
concept IsSpot = M == MarketType::SPOT;

/**
 * @enum ProtocolType
 * @brief Communication protocol types supported by exchanges
 */
enum class ProtocolType {
    JSON,  ///< JSON-based REST/WebSocket protocol
    SBE    ///< Simple Binary Encoding (high-performance binary protocol)
};

/**
 * @struct BinanceConfig
 * @brief Exchange endpoint configuration (plain data structure)
 * 
 * This template provides compile-time configuration for Binance exchange endpoints.
 * All members are constexpr, ensuring zero runtime overhead. Template specializations
 * exist for different market types (SPOT, FUTURES).
 * 
 * @tparam M Market type (SPOT or FUTURES)
 * 
 * @note No virtual functions - completely zero overhead abstraction
 * @note Users can create custom instances with different endpoints for testing
 */
template<MarketType M>
struct BinanceConfig;

/**
 * @struct BinanceConfig<MarketType::SPOT>
 * @brief Binance Spot market configuration
 * 
 * Contains all REST API endpoints and WebSocket streams for Binance Spot trading.
 * All endpoints are constexpr strings for zero runtime cost.
 */
template<>
struct BinanceConfig<MarketType::SPOT>{
    static constexpr const char* url = "api.binance.com";                    ///< Production REST API host
    static constexpr const char* test_url = "testnet.binance.vision";        ///< Testnet REST API host
    static constexpr const char* account_info = "/api/v3/account";           ///< Account information endpoint
    static constexpr const char* ping = "/api/v3/ping";                      ///< Test connectivity endpoint
    static constexpr const char* server_time = "/api/v3/time";               ///< Server time endpoint
    static constexpr const char* exchange_info = "/api/v3/exchangeInfo";     ///< Exchange trading rules and symbols
    static constexpr const char* depth = "/api/v3/depth";                    ///< Order book depth endpoint
    static constexpr const char* trades = "/api/v3/trades";                  ///< Recent trades endpoint
    static constexpr const char* order = "/api/v3/order";                    ///< Order placement/query/cancel endpoint
    static constexpr const char* listen_key = "/api/v3/userDataStream";      ///< User data stream (WebSocket) endpoint
    static constexpr const ProtocolType default_protocol = ProtocolType::JSON; ///< Default communication protocol
    static constexpr const bool supports_quote_order_qty = true;              ///< Supports quoteOrderQty parameter
};

/**
 * @struct BinanceConfig<MarketType::FUTURES>
 * @brief Binance Futures market configuration
 * 
 * Contains all REST API endpoints and WebSocket streams for Binance Futures trading.
 * Includes futures-specific endpoints for leverage, margin type, and position management.
 */
template<>
struct BinanceConfig<MarketType::FUTURES>{
    static constexpr const char* url = "fapi.binance.com";                       ///< Production Futures API host
    static constexpr const char* test_url = "demo-fapi.binance.com";             ///< Testnet Futures API host
    static constexpr const char* account_info = "/fapi/v2/balance";              ///< Account balance information
    static constexpr const char* open_positions = "/fapi/v3/positionRisk";       ///< Current positions and risk
    //"/fapi/v2/positionRisk", // Gets all symbols' positions
    static constexpr const char* ping = "/fapi/v1/ping";                         ///< Test connectivity endpoint
    static constexpr const char* server_time = "/fapi/v1/time";                  ///< Server time endpoint
    static constexpr const char* exchange_info = "/fapi/v1/exchangeInfo";        ///< Exchange trading rules and symbols
    static constexpr const char* depth = "/fapi/v1/depth";                       ///< Order book depth endpoint
    static constexpr const char* trades = "/fapi/v1/trades";                     ///< Recent trades endpoint
    static constexpr const char* order = "/fapi/v1/order";                       ///< Order placement/query/cancel endpoint
    static constexpr const char* listen_key = "/fapi/v1/listenKey";              ///< User data stream (WebSocket) endpoint
    static constexpr const char* leverage = "/fapi/v1/leverage";                 ///< Leverage adjustment endpoint
    static constexpr const ProtocolType default_protocol = ProtocolType::JSON;    ///< Default communication protocol
    static constexpr const bool supports_quote_order_qty = false;                 ///< Does not support quoteOrderQty parameter
};

// TODO: Add configs for other exchanges (e.g., Coinbase, Kraken, etc.)

} // namespace trade_connector