/**
 * @file orderbuilder.h
 * @brief Fluent builder pattern for constructing order parameters
 * 
 * Provides a type-safe, fluent interface for building order parameters
 * with compile-time validation of market-specific requirements using
 * C++20 concepts.
 */

#pragma once

#include "../configs.h"
#include "../types.h"

namespace trade_connector::rest {

/**
 * @class OrderBuilder
 * @brief Fluent builder for constructing type-safe order parameters
 * 
 * This class implements the builder pattern to construct OrderParams objects
 * in a readable, chainable manner. Market-specific methods are enabled/disabled
 * at compile-time using C++20 concepts, preventing invalid parameter combinations.
 * 
 * @tparam M Market type (SPOT or FUTURES)
 * 
 * @example
 * ```cpp
 * auto order = OrderBuilder<MarketType::SPOT>()
 *     .symbol("BTCUSDT")
 *     .side("BUY")
 *     .type("LIMIT")
 *     .price(50000.0)
 *     .quantity(0.001)
 *     .timeInForce("GTC")
 *     .build();
 * ```
 */
template <MarketType M>
class OrderBuilder {
    OrderParams<M> params;
    
public:
    /** @brief Default constructor - initializes with empty parameters */
    OrderBuilder() = default;
    
    /**
     * @brief Reset builder to empty state
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& reset() { params = OrderParams<M>{}; return *this; }
    
    /**
     * @brief Set trading pair symbol
     * @param s Trading pair symbol (e.g., "BTCUSDT", "ETHUSDT")
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& symbol(std::string s) { params.symbol = std::move(s); return *this; }
    
    /**
     * @brief Set order side
     * @param s Order side: "BUY" or "SELL"
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& side(Side s) { params.side = std::move(s); return *this; }
    
    /**
     * @brief Set limit price
     * @param p Price per unit (required for LIMIT orders)
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& price(double p) { params.price = p; return *this; }
    
    OrderBuilder& stopPrice(double sp) { params.stop_price = sp; return *this; }

    OrderBuilder& stopLimitPrice(double slp) { params.stop_limit_price = slp; return *this; }

    /**
     * @brief Set order quantity
     * @param q Quantity in base asset (e.g., BTC amount for BTCUSDT)
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& quantity(double q) { params.quantity = q; return *this; }
    
    /**
     * @brief Set order type
     * @param t Order type: "LIMIT", "MARKET", "STOP_LOSS", "TAKE_PROFIT", etc.
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& type(OrderType t) { params.type = std::move(t); return *this; }
    
    /**
     * @brief Set time in force policy
     * @param tif Time in force: "GTC" (Good Till Cancel), "IOC" (Immediate or Cancel),
     *            "FOK" (Fill or Kill), "GTX" (Good Till Crossing), "GTD" (Good Till Date)
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& timeInForce(TimeInForce tif) { params.time_in_force = std::move(tif); return *this; }
    
    OrderBuilder& stopLimitTimeInForce(TimeInForce sltif) { params.stop_limit_time_in_force = std::move(sltif); return *this; }

    /**
     * @brief Set custom timestamp
     * @param ts Timestamp in milliseconds since epoch (0 = auto-generate)
     * @return Reference to this builder for method chaining
     */
    OrderBuilder& timestamp(unsigned long ts) { params.timestamp = ts; return *this; }

    /**
     * @brief Set quote order quantity (SPOT market only)
     * @param qq Quantity in quote asset (e.g., USDT amount for BTCUSDT)
     * @return Reference to this builder for method chaining
     * @note Only available for spot markets. Mutually exclusive with quantity().
     */
    OrderBuilder& quoteOrderQty(double qq) requires IsSpot<M> { params.quote_quantity = qq; return *this; }

    /**
     * @brief Set reduce-only flag (FUTURES market only)
     * @param ro If true, order will only reduce existing position (cannot increase)
     * @return Reference to this builder for method chaining
     * @note Only available for futures markets
     */
    OrderBuilder& reduceOnly(bool ro) requires IsFutures<M> { params.reduce_only = ro; return *this; }
    
    /**
     * @brief Set position side (FUTURES market only)
     * @param ps Position side: "BOTH" (one-way mode), "LONG", or "SHORT" (hedge mode)
     * @return Reference to this builder for method chaining
     * @note Only available for futures markets. Required in hedge mode.
     */
    OrderBuilder& positionSide(Side ps) requires IsFutures<M> { params.position_side = std::move(ps); return *this; }
    
    /**
     * @brief Set leverage (FUTURES market only)
     * @param lev Leverage multiplier (typically 1-125, depending on symbol)
     * @return Reference to this builder for method chaining
     * @note Only available for futures markets
     */
    OrderBuilder& leverage(int lev) requires IsFutures<M> { params.leverage = lev; return *this; }
    
    /**
     * @brief Build and return the final OrderParams object
     * @return Constructed OrderParams with all configured values
     */
    OrderParams<M> build() const { return params; }
};

} // namespace trade_connector::rest