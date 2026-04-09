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
#include <string>

namespace trade_connector {

/**
 * @enum MarketType
 * @brief Defines the type of trading market
 * 
 * Used as a template parameter to specialize exchange configurations
 * and trading behavior for different market types.
 */
enum class MarketType {
    SPOT,      ///< Spot trading market
    FUTURES,    ///< Futures/derivatives trading market
    GENERIC    ///< Generic market type
};

/**
 * @concept IsFutures
 * @brief Constrains template parameters to futures market type
 * 
 * This concept enables compile-time checks and method overloading
 * based on market type.
 * 
 * @tparam M Market type to validate
 */
template<MarketType M>
concept IsFutures = M == MarketType::FUTURES || M == MarketType::GENERIC;

/**
 * @concept IsSpot
 * @brief Constrains template parameters to spot market type
 * 
 * This concept enables compile-time checks and method overloading
 * based on market type.
 * 
 * @tparam M Market type to validate
 */
template<MarketType M>
concept IsSpot = M == MarketType::SPOT || M == MarketType::GENERIC;

/**
 * @enum ProtocolType
 * @brief Communication protocol types supported by exchanges
 */
enum class ProtocolType {
    JSON,  ///< JSON-based REST/WebSocket protocol
    SBE    ///< Simple Binary Encoding (high-performance binary protocol)
};

class Error {
    // Error codes:
    // 0 = No error
    // 1 = Stream read error
    // 2 = Stream write error
public:
    Error() : code_(0), message_("") {}
    Error(const std::string& message, int value = 0) 
        : code_(value == 0 ? 1 : value), 
          message_(message) {}

    explicit operator bool() const { return code_ != 0; }
    int code() const { return code_; }
    const std::string& message() const { return message_; }
    
private:
    int code_;               ///< Error code
    std::string message_;    ///< Error message
};

inline Error dummy_error; // A dummy error object for default parameters

enum class Venue {
    NONE,
    BINANCE,
    COINBASE,
    KRAKEN,
    BITFINEX,
    FTX,
    BYBIT,
    HUOBI,
    OKEX,
    GATEIO,
    POLONIEX
};

inline std::map<Venue, std::string> venueToString = {
    {Venue::NONE, "NONE"},
    {Venue::BINANCE, "BINANCE"},
    {Venue::COINBASE, "COINBASE"},
    {Venue::KRAKEN, "KRAKEN"},
    {Venue::BITFINEX, "BITFINEX"},
    {Venue::FTX, "FTX"},
    {Venue::BYBIT, "BYBIT"},
    {Venue::HUOBI, "HUOBI"},
    {Venue::OKEX, "OKEX"},
    {Venue::GATEIO, "GATEIO"},
    {Venue::POLONIEX, "POLONIEX"}
};

enum class TokenPair {
    NONE,
    BTCUSD,
    ETHUSD
};

inline std::map<TokenPair, std::string> tokenPairToString = {
    {TokenPair::NONE, "NONE"},
    {TokenPair::BTCUSD, "BTCUSD"},
    {TokenPair::ETHUSD, "ETHUSD"}
};

namespace Binance {
    inline std::map<TokenPair, std::string> tokenPairToString = {
        {TokenPair::BTCUSD, "BTCUSDT"},
        {TokenPair::ETHUSD, "ETHUSDT"}
    };
    inline std::map<std::string, TokenPair, std::less<>> stringToTokenPair = {
        {"BTCUSDT", TokenPair::BTCUSD},
        {"ETHUSDT", TokenPair::ETHUSD}
    };
    inline std::map<std::string, std::string> symbolToCanonical = {
        {"BTCUSDT", "BTCUSD"},
        {"ETHUSDT", "ETHUSD"}
    };
}

namespace Kraken {
    inline std::map<TokenPair, std::string> tokenPairToString = {
        {TokenPair::BTCUSD, "XBT/USD"},
        {TokenPair::ETHUSD, "ETH/USD"}
    };
    inline std::map<std::string, TokenPair> stringToTokenPair = {
        {"XBT/USD", TokenPair::BTCUSD},
        {"ETH/USD", TokenPair::ETHUSD}
    };
    inline std::map<std::string, std::string> symbolToCanonical = {
        {"XBT/USD", "BTCUSD"},
        {"ETH/USD", "ETHUSD"}
    };
}

namespace Coinbase {
    inline std::map<TokenPair, std::string> tokenPairToString = {
        {TokenPair::BTCUSD, "BTC-USD"},
        {TokenPair::ETHUSD, "ETH-USD"}
    };
    inline std::map<std::string, TokenPair> stringToTokenPair = {
        {"BTC-USD", TokenPair::BTCUSD},
        {"ETH-USD", TokenPair::ETHUSD}
    };
    inline std::map<std::string, std::string> symbolToCanonical = {
        {"BTC-USD", "BTCUSD"},
        {"ETH-USD", "ETHUSD"}
    };
}

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
    std::string new_client_order_id = "";  ///< Optional client order ID
    std::string limit_client_order_id = "";
    std::string stop_client_order_id = "";
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
    std::string new_client_order_id = "";  ///< Optional client order ID
    TimeInForce time_in_force = TimeInForce::GTC; ///< Time in force: "GTC", "IOC", "FOK", "GTX", "GTD"
    bool reduce_only = false;        ///< If true, order will only reduce position size
    Side position_side = Side::NONE;  ///< Position side: "BOTH", "LONG", or "SHORT" (hedge mode)
    unsigned long timestamp = 0;     ///< Order timestamp in milliseconds (0 = auto-generate)
};

} // namespace trade_connector