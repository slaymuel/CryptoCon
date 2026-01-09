/**
 * @file types.h
 * @brief Core type definitions and order parameter structures
 * 
 * This file defines fundamental types used throughout the trading connector,
 * including JSON parsing types, callback signatures, and market-specific
 * order parameter structures.
 */

#pragma once

#include <simdjson.h>
#include "configs.h"

namespace trade_connector {

/**
 * @typedef Json
 * @brief Alias for simdjson document result type
 * 
 * Represents the result of parsing a JSON document using the simdjson library.
 * This type is used for high-performance, zero-copy JSON parsing.
 */
using Json = simdjson::simdjson_result<simdjson::fallback::ondemand::document>;

/**
 * @typedef Callback
 * @brief Function pointer type for WebSocket message callbacks
 * 
 * Defines the signature for callback functions that process incoming
 * JSON messages from WebSocket connections. Must be a non-capturing lambda
 * or regular function pointer.
 * 
 * @param json Parsed JSON document from the WebSocket message
 */
using Callback = void(*)(const Json&);

/**
 * @struct OrderParams
 * @brief Template for market-specific order parameters
 * 
 * This template is specialized for each market type to provide appropriate
 * order parameters. Each specialization contains the exact fields required
 * for that market's order API.
 * 
 * @tparam M Market type (SPOT or FUTURES)
 */
template<MarketType M>
struct OrderParams;

/**
 * @struct OrderParams<MarketType::SPOT>
 * @brief Order parameters for spot market trading
 * 
 * Contains all parameters needed to place orders on spot markets.
 * Supports both base quantity and quote quantity order types.
 */
template<>
struct OrderParams<MarketType::SPOT> {
    std::string symbol;              ///< Trading pair symbol (e.g., "BTCUSDT")
    std::string side;                ///< Order side: "BUY" or "SELL"
    std::string type;                ///< Order type: "LIMIT", "MARKET", "STOP_LOSS", etc.
    double price = 0.0;              ///< Limit price (required for LIMIT orders)
    double quantity = 0.0;           ///< Base asset quantity to buy/sell
    double quote_quantity = 0.0;     ///< Quote asset quantity (alternative to quantity)
    std::string time_in_force = ""; ///< Time in force: "GTC", "IOC", "FOK", "GTX", "GTD"
    unsigned long timestamp = 0;     ///< Order timestamp in milliseconds (0 = auto-generate)
};

/**
 * @struct OrderParams<MarketType::FUTURES>
 * @brief Order parameters for futures market trading
 * 
 * Contains all parameters needed to place orders on futures markets.
 * Includes futures-specific fields like position side and reduce-only flag.
 */
template<>
struct OrderParams<MarketType::FUTURES> {
    std::string symbol;              ///< Trading pair symbol (e.g., "BTCUSDT")
    std::string side;                ///< Order side: "BUY" or "SELL"
    std::string type;                ///< Order type: "LIMIT", "MARKET", "STOP", "TAKE_PROFIT", etc.
    double price = 0.0;              ///< Limit price (required for LIMIT orders)
    double quantity = 0.0;           ///< Contract quantity to buy/sell
    std::string time_in_force = ""; ///< Time in force: "GTC", "IOC", "FOK", "GTX", "GTD"
    bool reduce_only = false;        ///< If true, order will only reduce position size
    std::string position_side = "";  ///< Position side: "BOTH", "LONG", or "SHORT" (hedge mode)
    unsigned long timestamp = 0;     ///< Order timestamp in milliseconds (0 = auto-generate)
};

/**
 * @typedef RequestString
 * @brief HTTP request type with string body
 * 
 * Used for POST/PUT requests that need to send data in the request body.
 */
using RequestString = boost::beast::http::request<boost::beast::http::string_body>;

/**
 * @typedef RequestEmpty
 * @brief HTTP request type with no body
 * 
 * Used for GET/DELETE requests that don't send data in the request body.
 */
using RequestEmpty = boost::beast::http::request<boost::beast::http::empty_body>;

} // namespace trade_connector