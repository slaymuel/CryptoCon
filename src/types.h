#pragma once

#include <simdjson.h>
#include "configs.h"

namespace trade_connector {

using Json = simdjson::simdjson_result<simdjson::fallback::ondemand::document>;
using Callback = void(*)(const Json&);

template<MarketType M>
struct OrderParams;

template<>
struct OrderParams<MarketType::SPOT> {
    std::string symbol;
    std::string side;
    std::string type;
    double price = 0.0;
    double quantity = 0.0;
    double quote_quantity = 0.0;
    std::string time_in_force = "";
    unsigned long timestamp = 0;
};

template<>
struct OrderParams<MarketType::FUTURES> {
    std::string symbol;
    std::string side;
    std::string type;
    double price = 0.0;
    double quantity = 0.0;
    std::string time_in_force = "";
    bool reduce_only = false;
    std::string position_side = "";  // BOTH, LONG, SHORT
    unsigned long timestamp = 0;
};

using RequestString = boost::beast::http::request<boost::beast::http::string_body>;
using RequestEmpty = boost::beast::http::request<boost::beast::http::empty_body>;

} // namespace trade_connector