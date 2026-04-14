#pragma once

#include <functional>
#include <simdjson.h>
#include <format>
#include "../utils/signing.h"
#include "../utils/utils.h"
#include "../types.h"

namespace trade_connector {

/// @brief Binance exchange policy implementing REST and WebSocket operations.
/// @tparam M Market type (SPOT, FUTURES, or GENERIC)
template< MarketType M = MarketType::GENERIC>
class BinancePolicy {
    using Headers = std::span<const std::pair<std::string, std::string>>;

public:
    static constexpr MarketType market_type = M;
    static constexpr const char* BINANCE_WS_USER_HOST = "ws-api.testnet.binance.vision";
    static constexpr const char* BINANCE_WS_HOST = "stream.testnet.binance.vision";
    static constexpr const char* BINANCE_REST_HOST = "testnet.binance.vision";

    BinancePolicy(
        std::function<void(const std::string&)> logger = null_logger
    ) { }

    BinancePolicy(const BinancePolicy&) = delete;
    BinancePolicy& operator=(const BinancePolicy&) = delete;
    
    /** @brief Could implement these later if needed */
    BinancePolicy(BinancePolicy&&) noexcept = delete;
    BinancePolicy& operator=(BinancePolicy&&) noexcept = delete;

    ~BinancePolicy() = default;

    const char* restHost() {
        return BINANCE_REST_HOST;
    }

    const char* wsHost() {
        return BINANCE_WS_HOST;
    }

    /// Create a listen key for user data streams (valid 60 min).
    std::string createListenKey(auto& host) {
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };

        auto response = host.post("/api/v3/userDataStream", headers);
        simdjson::ondemand::parser parser;
        simdjson::padded_string json(response);
        std::string listen_key;
        try{
            auto doc = parser.iterate(json);
            listen_key = std::string(doc["listenKey"].get_string().value());
        }
        catch(const simdjson::simdjson_error& e){
            host.logger("Failed to fetch listen key");
            host.logger("Full response: " + response);
            throw e;
        }
        return listen_key;
    }

    /// Extend listen key validity by another 60 minutes.
    std::string keepAliveListenKey(auto& host, const std::string& listen_key){
        std::string target = std::format("/api/v3/userDataStream?listenKey={}", listen_key);
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };
        return host.restClient().put(target, headers);
    }

    /// Close and invalidate a listen key.
    std::string closeListenKey(auto& host, const std::string& listen_key){
        std::string target = std::format("/api/v3/userDataStream?listenKey={}", listen_key);
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };
        return host.restClient().del(target, headers);
    }

    /// Retrieve account info (balances, permissions, commission rates).
    std::string getAccountInfo(auto& host){
        std::string query = "timestamp=" + std::to_string(currentTimeMillis())
                + "&recvWindow=5000";

        std::string signature = signHMAC(host.secretKey(), query);
        std::string target = std::format("/api/v3/account?{}&signature={}", query, signature);
        //std::string target = "/api/v3/account?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };

        return host.restClient().get(target, headers);
    }

    /// Get open positions — futures only.
    std::string getOpenPositions(auto& host) requires(IsFutures<M>){
        std::string query = "timestamp=" + std::to_string(currentTimeMillis())
                + "&recvWindow=5000";

        std::string signature = signHMAC(host.secretKey(), query);
        std::string target = std::format("{}?{}&signature={}", "/api/v3/openOrders", query, signature);
        //std::string target = "/fapi/v2/positionRisk?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };

        return host.restClient().get(target, headers);
    }

    /// Ping the exchange (connectivity test, no auth required).
    std::string ping(auto& host){
        std::string target = "/api/v3/ping";
        return host.restClient().get(target);
    }

    /// Get server time (milliseconds since epoch). Useful for clock sync.
    std::string getServerTime(auto& host){
        return host.restClient().get("/api/v3/time");
    }

    /// Get exchange info (trading rules, symbol filters, rate limits).
    std::string getExchangeInfo(auto& host){
        return host.restClient().get("/api/v3/exchangeInfo");
    }

    /// Get all open orders — spot only.
    std::string getOpenOrders(auto& host) requires(IsSpot<M>){
        std::string query = "timestamp=" + std::to_string(currentTimeMillis())
                + "&recvWindow=5000";

        std::string signature = signHMAC(host.secretKey(), query);
        std::string target = std::format("{}?{}&signature={}", "/api/v3/openOrders", query, signature);
        //std::string target = "/api/v3/openOrders?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };

        return host.restClient().get(target, headers);
    }

    /// Cancel all open orders by fetching and iterating them — spot only.
    void cancelAllOpenOrders(auto& host) requires(IsSpot<M>){
        auto open_orders = getOpenOrders(host);

        simdjson::ondemand::parser parser;
        simdjson::padded_string json_orders(open_orders);
        try{
            auto orders_doc = parser.iterate(json_orders);
            auto orders = orders_doc.get_array().value();
            for (auto order : orders) {
                // Avoids allocation since find() on Binance::stringToTokenPair is transparent
                auto it = Binance::stringToTokenPair.find(order["symbol"].get_string().value());
                auto symbol = it->second;
                uint64_t order_id = order["orderId"].get_uint64().value();
                host.cancelOrder(symbol, order_id);
            }
        }
        catch(const simdjson::simdjson_error& e){
            host.logger("Failed to cancel orders");
            throw e;
        }
    }

    /// Get order book depth for a symbol. @param limit Valid: 5, 10, 20, 50, 100, 500, 1000, 5000.
    std::string getOrderBook(auto& host, const TokenPair& symbol, int limit = 100){
        std::string target = std::format("{}?symbol={}&limit={}", "/api/v3/depth", Binance::tokenPairToString[symbol], limit);
        return host.restClient().get(target);
    }

    /// Get recent trades for a symbol. @param limit Max 1000.
    std::string getRecentTrades(auto& host, const TokenPair& symbol, int limit = 500){
        std::string target = std::format("{}?symbol={}&limit={}", "/api/v3/trades", Binance::tokenPairToString[symbol], limit);
        return host.restClient().get(target);
    }

    /// Place an order. Supports LIMIT, MARKET, STOP_LOSS, TAKE_PROFIT, OCO, etc.
    template<typename T>
    std::string sendOrder(auto& host, const T& params) {
        // buildquery also templated since now params is different for oco, market, limit etc
        std::string query = buildQuery(params);
        std::string signature = signHMAC(host.secretKey(), query);
        auto endpoint = (params.type == OrderType::OCO) ? "/api/v3/order/oco" : "/api/v3/order";
        std::string target = std::format("{}?{}&signature={}", endpoint, query, signature);
        return sendOrder(host, target);
    }

    /// Send order with market-type-specific params and error reporting.
    std::string sendOrder(auto& host, const OrderParams<M>& params, Error& error = dummy_error) {
        std::string query = buildQuery(params);
        std::string signature = signHMAC(host.secretKey(), query);
        auto endpoint = (params.type == OrderType::OCO) ? "/api/v3/order/oco" : "/api/v3/order";
        std::string target = std::format("{}?{}&signature={}", endpoint, query, signature);
        return sendOrder(host, target, error);
    }

    /// Low-level: POST a pre-built signed target URL as an order.
    std::string sendOrder(auto& host, std::string target, Error& error = dummy_error) {
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };
        
        return host.restClient().post(target, headers, "", error);
    }

    /// Build URL-encoded query string from spot order params (auto-timestamps).
    std::string buildQuery(const OrderParams<MarketType::SPOT>& params) {
        std::string query;
        query.reserve(256);

        query.append("symbol=").append(params.symbol)
            .append("&side=").append(sideToString[params.side]);
        if (!params.new_client_order_id.empty()) {
            query.append("&newClientOrderId=").append(params.new_client_order_id);
        }
        switch (params.type) {
            case OrderType::LIMIT:
                query.append("&type=").append(orderTypeToString[params.type]);
                query.append("&timeInForce=").append(timeInForceToString[params.time_in_force]);

                query.append("&price=");
                appendNumber(query, params.price);

                query.append("&quantity=");
                appendNumber(query, params.quantity);
                break;
                
            case OrderType::MARKET:
                query.append("&type=").append(orderTypeToString[params.type]);
                if (params.quantity > 0.0) {
                    query.append("&quantity=");
                    appendNumber(query, params.quantity);
                } else if (params.quote_quantity > 0.0) {
                    query.append("&quoteOrderQty=");
                    appendNumber(query, params.quote_quantity);
                }
                break;

            case OrderType::STOP_LOSS:
                query.append("&type=").append(orderTypeToString[params.type]);
                query.append("&stopPrice=");
                appendNumber(query, params.stop_price);
                if (params.quantity > 0.0) {
                    query.append("&quantity=");
                    appendNumber(query, params.quantity);
                }
                break;

            case OrderType::STOP_LOSS_LIMIT:
                query.append("&type=").append(orderTypeToString[params.type]);
                query.append("&stopPrice=");
                appendNumber(query, params.stop_price);
                query.append("&price=");
                appendNumber(query, params.price);
                query.append("&timeInForce=").append(timeInForceToString[params.time_in_force]);
                if (params.quantity > 0.0) {
                    query.append("&quantity=");
                    appendNumber(query, params.quantity);
                }
                break;

            case OrderType::TAKE_PROFIT:
                query.append("&type=").append(orderTypeToString[params.type]);
                query.append("&stopPrice=");
                appendNumber(query, params.stop_price);
                if (params.quantity > 0.0) {
                    query.append("&quantity=");
                    appendNumber(query, params.quantity);
                }
                break;
            case OrderType::TAKE_PROFIT_LIMIT:
                query.append("&type=").append(orderTypeToString[params.type]);
                query.append("&stopPrice=");
                appendNumber(query, params.stop_price);
                query.append("&price=");
                appendNumber(query, params.price);
                query.append("&timeInForce=").append(timeInForceToString[params.time_in_force]);
                if (params.quantity > 0.0) {
                    query.append("&quantity=");
                    appendNumber(query, params.quantity);
                }
                break;
                
            case OrderType::OCO:
                query.append("&price=");
                appendNumber(query, params.price);
                query.append("&stopPrice=");
                appendNumber(query, params.stop_price);
                query.append("&stopLimitPrice=");
                appendNumber(query, params.stop_limit_price);
                query.append("&stopLimitTimeInForce=");
                query.append(timeInForceToString[params.stop_limit_time_in_force]);
                if (params.quantity > 0.0) {
                    query.append("&quantity=");
                    appendNumber(query, params.quantity);
                }
                break;

            default:
                throw std::logic_error("Not implemented");
        }

        // Always add timestamp
        query.append("&timestamp=");
        appendNumber(query, params.timestamp > 0 ? params.timestamp : currentTimeMillis());
        return query;
    }

    //std::string buildQuery(const OrderParams<MarketType::SPOT>& params) {
    //    std::string query;
    //    query.reserve(256);
    //    query.append("symbol=").
    //        append(params.symbol).
    //        append("&side=").
    //        append(sideToString[params.side]).
    //        append("&type=").
    //        append(orderTypeToString[params.type]);
    //    
    //    // Add quantity OR quoteOrderQty (not both)
    //    if (params.quantity > 0.0) {
    //        query.append("&quantity=");
    //        appendNumber(query, params.quantity);
    //    } else if (params.quote_quantity > 0.0) {
    //        query.append("&quoteOrderQty=");
    //        appendNumber(query, params.quote_quantity);
    //    }
    //    
    //    // Add price for LIMIT orders
    //    if (params.price > 0.0) {
    //        query.append("&price=");
    //        appendNumber(query, params.price);
    //    }
    //    
    //    // Add timeInForce for LIMIT orders (not for MARKET)
    //    if (params.time_in_force != TimeInForce::GTC && params.type != OrderType::MARKET) {
    //        query.append("&timeInForce=").append(timeInForceToString[params.time_in_force]);
    //    }
    //    
    //    if (params.timestamp > 0) {
    //        query.append("&timestamp=");
    //        appendNumber(query, params.timestamp);
    //    }
    //    else {
    //        query.append("&timestamp=");
    //        appendNumber(query, currentTimestamp());
    //    }
//
    //    return query;
    //}

    /// Build URL-encoded query string from futures order params (auto-timestamps).
    std::string buildQuery(const OrderParams<MarketType::FUTURES>& params) {
        std::string query;
        query.reserve(256);
        query.append("symbol=").
            append(params.symbol).
            append("&side=").
            append(sideToString[params.side]).
            append("&type=").
            append(orderTypeToString[params.type]);
        
        // Add quantity
        if (params.quantity > 0.0) {
            query.append("&quantity=");
            appendNumber(query, params.quantity);
        }
        
        // Add price for LIMIT orders
        if (params.price > 0.0) {
            query.append("&price=");
            appendNumber(query, params.price);
        }
        
        // Add timeInForce for LIMIT orders (not for MARKET)
        if (params.time_in_force != TimeInForce::GTC && params.type != OrderType::MARKET) {
            query.append("&timeInForce=").append(timeInForceToString[params.time_in_force]);
        }

        // Add reduceOnly flag
        if (params.reduce_only) {
            query.append("&reduceOnly=true");
        }

        if (params.position_side != Side::NONE) {
            query.append("&positionSide=").append(sideToString[params.position_side]);
        }

        if (params.timestamp > 0) {
            query.append("&timestamp=");
            appendNumber(query, params.timestamp);
        }
        return query;
    }

    /// Cancel an order by order ID.
    std::string cancelOrder(
        auto& host,
        const TokenPair& symbol,
        uint64_t order_id,
        Error& error = dummy_error
    ) {
        std::string query;
        query.reserve(256);
        query.append("symbol=").append(Binance::tokenPairToString[symbol])
            .append("&timestamp=").append(std::to_string(currentTimeMillis()))
            .append("&orderId=");
        appendNumber(query, order_id);

        std::string signature = signHMAC(host.secretKey(), query);
        std::string target = std::format("{}?{}&signature={}", "/api/v3/order", query, signature);

        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };

        return host.restClient().del(target, headers, error);
    }

    /// Set leverage for a symbol — futures only (1–125x depending on symbol/tier).
    std::string setLeverage(auto& host, const TokenPair& symbol, int leverage) requires(IsFutures<M>) {
        std::string query = "symbol=" + Binance::tokenPairToString[symbol]
            + "&leverage=" + std::to_string(leverage)
            + "&timestamp=" + std::to_string(currentTimeMillis())
            + "&recvWindow=5000";

        std::string signature = signHMAC(host.secretKey(), query);
        std::string target = "/fapi/v1/leverage?" + query + "&signature=" + signature;
        
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };
    
        return host.restClient().post(target, headers);
    }

    /// Set margin type ("ISOLATED" or "CROSSED") — futures only.
    std::string setMarginType(auto& host, const TokenPair& symbol, const std::string& margin_type) requires(IsFutures<M>) {
        std::string query = "symbol=" + Binance::tokenPairToString[symbol]
            + "&marginType=" + margin_type
            + "&timestamp=" + std::to_string(currentTimeMillis())
            + "&recvWindow=5000";

        std::string signature = sign(host.secretKey(), query);
        std::string target = "/fapi/v1/marginType?" + query + "&signature=" + signature;
        
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.apiKey()}
        };
    
        return host.restClient().post(target, headers);
    }

    /// Subscribe to user data stream via WebSocket API using HMAC signature.
    template<typename Callback>
    void connectUserDataFeed(
        auto& host,
        Callback callback,
        const std::string& path = "/ws-api/v3",
        const std::uint64_t request_id = 1,
        const std::uint64_t recv_window = 5000
    ) {
        host.websocketClient().connectEndpoint(
            callback,
            BINANCE_WS_USER_HOST,
            path,
            // TODO make alias for ix::WebSocket
            [this, request_id, recv_window, &host](const std::string& url) {
                if (host.apiKey().empty() || host.secretKey().empty()) {
                    host.logger("Cannot subscribe (signature) to Binance user data stream on " + url + ": API key/secret key is empty");
                    return;
                }

                const std::uint64_t timestamp = currentTimeMillis();

                // Signature payload must use the same parameter ordering as sent.
                std::string signature_payload = "apiKey=" + host.apiKey();

                if (recv_window > 0) {
                    signature_payload += "&recvWindow=" + std::to_string(recv_window);
                }

                signature_payload += "&timestamp=" + std::to_string(timestamp);

                const std::string signature = signHMAC(host.secretKey(), signature_payload);

                std::string subscribe_message =
                    "{\"id\":" + std::to_string(request_id) +
                    ",\"method\":\"userDataStream.subscribe.signature\",\"params\":{\"apiKey\":\"" + host.apiKey() + "\"";

                if (recv_window > 0) {
                    subscribe_message += ",\"recvWindow\":" + std::to_string(recv_window);
                }

                subscribe_message +=
                    ",\"timestamp\":" + std::to_string(timestamp) +
                    ",\"signature\":\"" + signature + "\"}}";

                bool result = host.websocketClient().send(url, subscribe_message);
                if (!result) {
                    host.logger("Failed to subscribe (signature) to Binance user data stream on " + url);
                } else {
                    host.logger("Subscribed (signature) to Binance user data stream on " + url);
                }
            }
        );
    }

    /// Subscribe to order-book depth updates for one or more token pairs.
    template<typename Callback>
    void connectDepthFeed(auto& host, const std::vector<TokenPair>& tokens, Callback callback) {
        // Connect to the depth endpoint
        std::string s;
        if (tokens.size() == 1) {
            s = std::format("/ws/{}@depth", toLower(Binance::tokenPairToString[tokens[0]]));
        } else {
            s = "/stream?streams=";
            for (std::size_t i = 0; i < tokens.size(); ++i) {
                if (i > 0) s += "/";
                s += std::format("{}@depth", toLower(Binance::tokenPairToString[tokens[i]]));
            }
        }

        const std::string address = std::format("wss://{}{}", BINANCE_WS_HOST, s);

        host.websocketClient().connectEndpoint(
            callback,
            BINANCE_WS_HOST,
            s
        );
    }

    /// Subscribe to aggregated trade updates for one or more token pairs.
    template<typename Callback>
    void connectTradeFeed(auto& host, const std::vector<TokenPair>& tokens, Callback callback) {
        // Build string for all tokens
        // Combined streams format: /stream?streams=<symbol1>@<stream1>/<symbol2>@<stream2>
        std::string aggregate;
        if (tokens.size() == 1) {
            aggregate = std::format("/ws/{}@aggTrade", toLower(Binance::tokenPairToString[tokens[0]]));
        } else {
            aggregate = "/stream?streams=";
            for (std::size_t index = 0; index < tokens.size(); ++index) {
                if (index > 0) {
                    aggregate += "/";
                }
                aggregate += std::format("{}@aggTrade", toLower(Binance::tokenPairToString[tokens[index]]));
            }
        }

        const std::string address = std::format("wss://{}{}", BINANCE_WS_HOST, aggregate);

        host.websocketClient().connectEndpoint(
            callback,
            BINANCE_WS_HOST,
            aggregate
        );
    }

};

} // namespace trade_connector