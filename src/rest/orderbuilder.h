#pragma once

#include "../configs.h"
#include "../types.h"

namespace trade_connector::rest {

template <MarketType M>
class OrderBuilder {
    OrderParams<M> params;
    
public:
    OrderBuilder() = default;
    OrderBuilder& reset() { params = OrderParams<M>{}; return *this; }
    OrderBuilder& symbol(std::string s) { params.symbol = std::move(s); return *this; }
    OrderBuilder& side(std::string s) { params.side = std::move(s); return *this; }
    OrderBuilder& price(double p) { params.price = p; return *this; }
    OrderBuilder& quantity(double q) { params.quantity = q; return *this; }
    OrderBuilder& type(std::string t) { params.type = std::move(t); return *this; }
    OrderBuilder& timeInForce(std::string tif) { params.time_in_force = std::move(tif); return *this; }
    OrderBuilder& timestamp(unsigned long ts) { params.timestamp = ts; return *this; }
    // Only for SPOT
    OrderBuilder& quoteQuantity(double qq) requires IsSpot<M> { params.quote_quantity = qq; return *this; }
    // Only for FUTURES
    OrderBuilder& reduceOnly(bool ro) requires IsFutures<M> { params.reduce_only = ro; return *this; }
    OrderBuilder& positionSide(std::string ps) requires IsFutures<M> { params.position_side = std::move(ps); return *this; }
    OrderBuilder& leverage(int lev) requires IsFutures<M> { params.leverage = lev; return *this; }
    OrderParams<M> build() const { return params; }
};

} // namespace trade_connector::rest