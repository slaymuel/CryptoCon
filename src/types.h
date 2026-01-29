/**
 * @file types.h
 * @brief Core type definitions and order parameter structures
 * 
 * This file defines fundamental types used throughout the trading connector,
 * including JSON parsing types, callback signatures, and market-specific
 * order parameter structures.
 */

#pragma once

#include <map>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include "configs.h"

namespace trade_connector {

enum class Side{
    BUY,
    SELL,
    LONG,
    SHORT,
    BOTH,
    FLAT,
    NONE
};

inline std::map<Side, std::string> sideToString = {
    {Side::BUY, "BUY"},
    {Side::SELL, "SELL"},
    {Side::LONG, "LONG"},
    {Side::SHORT, "SHORT"},
    {Side::BOTH, "BOTH"},
    {Side::FLAT, "FLAT"},
    {Side::NONE, "NONE"}
};

enum class TimeInForce{
    GTC, // Good Till Canceled
    IOC, // Immediate Or Cancel
    FOK // Fill Or Kill
};

inline std::map<TimeInForce, std::string> timeInForceToString = {
    {TimeInForce::GTC, "GTC"},
    {TimeInForce::IOC, "IOC"},
    {TimeInForce::FOK, "FOK"}
};

enum class OrderType{
    MARKET,
    LIMIT,
    STOP_LOSS_LIMIT,
    STOP_LOSS,
    TAKE_PROFIT_LIMIT,
    TAKE_PROFIT,
    LIMIT_MAKER,
    CANCEL,
    OCO,
    UNKNOWN
};
inline std::map<OrderType, std::string> orderTypeToString = {
    {OrderType::MARKET, "MARKET"},
    {OrderType::LIMIT, "LIMIT"},
    {OrderType::STOP_LOSS_LIMIT, "STOP_LOSS_LIMIT"},
    {OrderType::STOP_LOSS, "STOP_LOSS"},
    {OrderType::TAKE_PROFIT_LIMIT, "TAKE_PROFIT_LIMIT"},
    {OrderType::TAKE_PROFIT, "TAKE_PROFIT"},
    {OrderType::LIMIT_MAKER, "LIMIT_MAKER"},
    {OrderType::CANCEL, "CANCEL"},
    {OrderType::OCO, "OCO"},
    {OrderType::UNKNOWN, "UNKNOWN"}
};

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
 // Create OrderParams for simple, stop loss, take profit, oco etc in addition to STOP, FUTURES
template<>
struct OrderParams<MarketType::SPOT> {
    std::string symbol;              ///< Trading pair symbol (e.g., "BTCUSDT")
    Side side;                       ///< Order side: "BUY" or "SELL"
    OrderType type;                 ///< Order type: MARKET, LIMIT, STOP_LOSS, etc.
    double price = 0.0;              ///< Limit price (required for LIMIT orders)
    double stop_price = 0.0;         ///< Stop price (for STOP_LOSS, TAKE_PROFIT orders)
    double stop_limit_price = 0.0;   ///< Stop limit price (for STOP_LOSS_LIMIT, TAKE_PROFIT_LIMIT)
    std::string limitClientOrderId = "";
    std::string stopClientOrderId = "";
    double quantity = 0.0;           ///< Base asset quantity to buy/sell
    double quote_quantity = 0.0;     ///< Quote asset quantity (alternative to quantity)
    TimeInForce time_in_force = TimeInForce::GTC; ///< Time in force: "GTC", "IOC", "FOK", "GTX", "GTD"
    TimeInForce stop_limit_time_in_force = TimeInForce::GTC; ///< Time in force for stop limit orders
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
    Side side;                ///< Order side: "BUY" or "SELL"
    OrderType type;                 ///< Order type: MARKET, LIMIT, STOP_LOSS, etc.
    double price = 0.0;              ///< Limit price (required for LIMIT orders)
    double quantity = 0.0;           ///< Contract quantity to buy/sell
    TimeInForce time_in_force = TimeInForce::GTC; ///< Time in force: "GTC", "IOC", "FOK", "GTX", "GTD"
    bool reduce_only = false;        ///< If true, order will only reduce position size
    Side position_side = Side::NONE;  ///< Position side: "BOTH", "LONG", or "SHORT" (hedge mode)
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