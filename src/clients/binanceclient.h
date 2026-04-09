#pragma once

#include <simdjson.h>
#include <format>
#include "syncclient.h"
#include "../utils/signing.h"
#include "utils/utils.h"

namespace trade_connector {
    
template< MarketType M = MarketType::GENERIC>
class BinancePolicy {
    using Headers = std::span<const std::pair<std::string, std::string>>;

public:

    static constexpr const char* BINANCE_WS_HOST = "stream.testnet.binance.vision";
    static constexpr const char* BINANCE_REST_HOST = "api.binance.com";
    /**
    * @brief Construct REST client with custom host
    * 
    * Creates a REST client and establishes an HTTPS connection to the specified host.
    * The connection uses SSL/TLS with system default certificate verification.
    * 
    * @param host Exchange API hostname (e.g., "api.binance.com", "testnet.binance.vision")
    * @param api_key Exchange API key for authentication
    * @param secret_key Exchange secret key for request signing
    * 
    * @throws boost::beast::system_error if connection fails
    * @throws boost::beast::system_error if SSL handshake fails
    */
    BinancePolicy(
        const std::string& api_key, 
        const std::string& secret_key,
        std::function<void(const std::string&)> logger = null_logger
        
    ) : SyncClient<BinancePolicy<M>>(BINANCE_REST_HOST, BINANCE_WS_HOST, api_key, secret_key, logger){ }

    BinancePolicy(const BinancePolicy&) = delete;
    BinancePolicy& operator=(const BinancePolicy&) = delete;
    
    /** @brief Could implement these later if needed */
    BinancePolicy(BinancePolicy&&) noexcept = delete;
    BinancePolicy& operator=(BinancePolicy&&) noexcept = delete;

    ~BinancePolicy() = default;

    /**
    * @brief Create a new listen key for user data streams
    * 
    * Generates a listen key that can be used to establish a WebSocket connection
    * for receiving real-time user data (order updates, balance changes, etc.).
    * 
    * @return Listen key string (valid for 60 minutes)
    * 
    * @note Listen key must be kept alive by calling keepAliveListenKey() every 30-60 minutes
    * @note Each user can have only one active listen key at a time
    * @throws May throw if API request fails or JSON parsing fails
    * 
    * @see keepAliveListenKey() to extend validity
    * @see closeListenKey() to invalidate the key
    */
    std::string createListenKey(auto& host) {
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
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

    /**
    * @brief Extend the validity of a listen key
    * 
    * Keeps the listen key alive for another 60 minutes. This should be called
    * periodically (every 30-60 minutes) to maintain the WebSocket connection.
    * 
    * @param listen_key The listen key to keep alive
    * @return API response (typically empty on success)
    * 
    * @note If not called within 60 minutes, the listen key expires and WebSocket disconnects
    * @note Returns HTTP 200 on success
    * 
    * @see createListenKey() to generate a new key
    */
    std::string keepAliveListenKey(auto& host, const std::string& listen_key){
        std::string target = std::format("/api/v3/userDataStream?listenKey={}", listen_key);
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };
        return put(target, headers);
    }

    /**
    * @brief Close and invalidate a listen key
    * 
    * Immediately closes the user data stream and invalidates the listen key.
    * The associated WebSocket connection will be disconnected.
    * 
    * @param listen_key The listen key to close
    * @return API response (typically empty on success)
    * 
    * @note After calling this, a new listen key must be created for future streams
    * @note Returns HTTP 200 on success
    * 
    * @see createListenKey() to generate a new key
    */
    std::string closeListenKey(auto& host, const std::string& listen_key){
        std::string target = std::format("/api/v3/userDataStream?listenKey={}", listen_key);
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };
        return del(target, headers);
    }

    /**
    * @brief Get account information and balances
    * 
    * Retrieves current account information including:
    * - Account type and status
    * - Trading permissions
    * - Asset balances (free and locked)
    * - Commission rates
    * 
    * @return JSON string containing account information
    * 
    * @note SPOT: Returns all asset balances
    * @note FUTURES: Returns account balance and available margin
    * @note Requires valid API key and signature
    * @note Weight: 10 (SPOT), 5 (FUTURES)
    */
    std::string getAccountInfo(auto& host){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                + "&recvWindow=5000";

        std::string signature = signHMAC(host.secret_key, query);
        std::string target = std::format("/api/v3/account?{}&signature={}", query, signature);
        //std::string target = "/api/v3/account?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };

        return get(target, headers);
    }

    /**
    * @brief Get open positions (FUTURES only)
    * 
    * Retrieves all current position information including:
    * - Symbol and position side (LONG/SHORT)
    * - Position amount and entry price
    * - Unrealized PnL
    * - Leverage and margin type
    * - Liquidation price
    * 
    * @return JSON string containing position information for all symbols
    * 
    * @note Only available for futures markets
    * @note Returns all symbols, including those with zero position
    * @note Weight: 5
    */
    std::string getOpenPositions(auto& host) requires(IsFutures<M>){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                + "&recvWindow=5000";

        std::string signature = signHMAC(host.secret_key, query);
        std::string target = std::format("{}?{}&signature={}", "/api/v3/openOrders", query, signature);
        //std::string target = "/fapi/v2/positionRisk?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };

        return get(target, headers);
    }

    /**
    * @brief Test connectivity to the exchange API
    * 
    * Simple ping to verify that the API is reachable. Returns empty JSON object
    * on success. Useful for health checks and connection verification.
    * 
    * @return Empty JSON object "{}" on success
    * 
    * @note Does not require authentication
    * @note Weight: 1
    */
    std::string ping(auto& host){
        std::string target = "/api/v3/ping";
        return host.rest_client.get(target);
    }

    /**
    * @brief Get exchange server time
    * 
    * Retrieves the current server time in milliseconds since Unix epoch.
    * Useful for synchronizing local time with exchange time to avoid
    * timestamp-related signature errors.
    * 
    * @return JSON string containing server time: {"serverTime": 1234567890123}
    * 
    * @note Does not require authentication
    * @note Weight: 1
    * @note Use to calibrate system clock if experiencing timestamp errors
    */
    std::string getServerTime(auto& host){
        return host.rest_client.get("/api/v3/time");
    }

    /**
    * @brief Get exchange trading rules and symbol information
    * 
    * Retrieves comprehensive exchange information including:
    * - Rate limits
    * - Trading symbols and their configurations
    * - Price/quantity/notional filters
    * - Order types and time in force options
    * - Precision requirements
    * 
    * @return JSON string containing complete exchange information
    * 
    * @note Does not require authentication
    * @note Weight: 10
    * @note Essential for understanding trading constraints and symbol details
    */
    std::string getExchangeInfo(auto& host){
        return host.rest_client.get("/api/v3/exchangeInfo");
    }

    std::string getOpenOrders(auto& host) requires(IsSpot<M>){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                + "&recvWindow=5000";

        std::string signature = signHMAC(host.secret_key, query);
        std::string target = std::format("{}?{}&signature={}", "/api/v3/openOrders", query, signature);
        //std::string target = "/api/v3/openOrders?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };

        return host.rest_client.get(target, headers);
    }

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
                cancelOrder(symbol, order_id);
            }
        }
        catch(const simdjson::simdjson_error& e){
            host.logger("Failed to cancel orders");
            throw e;
        }
    }

    /**
    * @brief Get order book depth for a symbol
    * 
    * Retrieves the current order book with bids and asks at various price levels.
    * Returns the top N price levels for both sides of the order book.
    * 
    * @param symbol Trading pair symbol (e.g., "BTCUSDT")
    * @param limit Number of price levels to return (default: 100)
    *              Valid limits: 5, 10, 20, 50, 100, 500, 1000, 5000
    * @return JSON string containing order book with bids and asks
    * 
    * @note Does not require authentication
    * @note Weight: Depends on limit (1-50 for limit <= 100)
    * @note Higher limits consume more weight
    */
    std::string getOrderBook(auto& host, const TokenPair& symbol, int limit = 100){
        std::string target = std::format("{}?symbol={}&limit={}", "/api/v3/depth", Binance::tokenPairToString[symbol], limit);
        return host.rest_client.get(target);
    }

    /**
    * @brief Get recent trades for a symbol
    * 
    * Retrieves the most recent trades executed on the exchange for the specified symbol.
    * Each trade includes price, quantity, time, and trade side information.
    * 
    * @param symbol Trading pair symbol (e.g., "BTCUSDT")
    * @param limit Number of trades to return (default: 500, max: 1000)
    * @return JSON string containing array of recent trades
    * 
    * @note Does not require authentication
    * @note Weight: 1
    * @note Trades are returned in chronological order (oldest first)
    */
    std::string getRecentTrades(auto& host, const TokenPair& symbol, int limit = 500){
        std::string target = std::format("{}?symbol={}&limit={}", "/api/v3/trades", Binance::tokenPairToString[symbol], limit);
        return host.rest_client.get(target);
    }

    /**
    * @brief Send a new order to the exchange
    * 
    * Places a new order on the exchange using the provided order parameters.
    * The order is automatically signed with the secret key and timestamped.
    * 
    * Supported order types:
    * - LIMIT: Buy/sell at specific price (requires price and timeInForce)
    * - MARKET: Immediate execution at best available price
    * - STOP_LOSS: Stop-loss order (requires stopPrice)
    * - STOP_LOSS_LIMIT: Stop-loss with limit price
    * - TAKE_PROFIT: Take-profit order (requires stopPrice)
    * - TAKE_PROFIT_LIMIT: Take-profit with limit price
    * - LIMIT_MAKER: Post-only limit order (never takes liquidity)
    * 
    * @param params Order parameters specific to the market type
    *               SPOT: Supports quantity OR quoteOrderQty
    *               FUTURES: Supports position side and reduce-only flag
    * 
    * @return JSON string containing order response with:
    *         - Order ID
    *         - Status (NEW, FILLED, PARTIALLY_FILLED, etc.)
    *         - Executed quantity and price
    *         - Fills information
    * 
    * @throws May throw if insufficient balance, invalid parameters, or network error
    * 
    * @note Requires valid API key and signature
    * @note Weight: 1 (SPOT), 0 (FUTURES)
    * @note MARKET orders execute immediately
    * @note LIMIT orders require timeInForce (GTC, IOC, FOK, GTX, GTD)
    * 
    * @example
    * ```cpp
    * OrderParams<MarketType::SPOT> params;
    * params.symbol = "BTCUSDT";
    * params.side = "BUY";
    * params.type = "LIMIT";
    * params.price = 50000.0;
    * params.quantity = 0.001;
    * params.time_in_force = "GTC";
    * auto response = client.sendOrder(params);
    * ```
    */

    template<typename T>
    std::string sendOrder(auto& host, const T& params) {
        // buildquery also templated since now params is different for oco, market, limit etc
        std::string query = buildQuery(params);
        std::string signature = signHMAC(host.secret_key, query);
        auto endpoint = (params.type == OrderType::OCO) ? "/api/v3/order/oco" : "/api/v3/order";
        std::string target = std::format("{}?{}&signature={}", endpoint, query, signature);
        return sendOrder(host, target);
    }

    std::string sendOrder(auto& host, const OrderParams<M>& params, Error& error = dummy_error) {
        std::string query = buildQuery(params);
        std::string signature = signHMAC(host.secret_key, query);
        auto endpoint = (params.type == OrderType::OCO) ? "/api/v3/order/oco" : "/api/v3/order";
        std::string target = std::format("{}?{}&signature={}", endpoint, query, signature);
        return sendOrder(host, target, error);
    }

    /**
    * @brief Send order using raw query string
    * 
    * Internal method that signs and sends a pre-constructed query string.
    * Used by the public sendOrder(OrderParams) method after building the query.
    * 
    * @param query Pre-constructed URL-encoded query string (without signature)
    * @return JSON string containing order response
    * 
    * @note Automatically adds signature to the query
    * @note Requires valid API key in headers
    */
    std::string sendOrder(auto& host, std::string target, Error& error = dummy_error) {
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };
        
        return host.rest_client.post(target, headers, "", error);
    }

    /**
    * @brief Build query string from spot order parameters
    * 
    * Constructs a URL-encoded query string from the OrderParams structure.
    * Handles spot-specific parameters like quoteOrderQty and validates
    * parameter combinations.
    * 
    * @param params Spot market order parameters
    * @return URL-encoded query string (without signature)
    * 
    * @note Auto-generates timestamp if not provided
    * @note Quantity and quoteOrderQty are mutually exclusive
    */
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
        appendNumber(query, params.timestamp > 0 ? params.timestamp : currentTimestamp());
        return query;
    }

    //std::string buildQuery(const OrderParams<MarketType::SPOT>& params) {
    //    std::string query;
    //    query.reserve(256); // Preallocate for performance
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

    /**
    * @brief Build query string from futures order parameters
    * 
    * Constructs a URL-encoded query string from the OrderParams structure.
    * Handles futures-specific parameters like reduceOnly flag and position side.
    * 
    * @param params Futures market order parameters
    * @return URL-encoded query string (without signature)
    * 
    * @note Auto-generates timestamp if not provided
    * @note Includes futures-specific fields (reduceOnly, positionSide)
    */
    std::string buildQuery(const OrderParams<MarketType::FUTURES>& params) {
        std::string query;
        query.reserve(256); // Preallocate for performance
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

    /**
    * @brief Cancel an active order
    * 
    * Cancels an order on the exchange by order ID or client order ID.
    * 
    * @param symbol Trading pair symbol (e.g., "BTCUSDT")
    * @param order_id (optional) Exchange order ID
    * @param orig_client_order_id (optional) Client order ID
    * @return JSON string with cancel result
    * 
    * @note Requires valid API key and signature
    * @note At least one of order_id or orig_client_order_id must be provided
    */
    std::string cancelOrder(
        auto& host,
        const TokenPair& symbol,
        uint64_t order_id,
        Error& error = dummy_error
    ) {
        std::string query;
        query.reserve(256); // Preallocate for performance
        query.append("symbol=").append(Binance::tokenPairToString[symbol])
            .append("&timestamp=").append(std::to_string(currentTimestamp()))
            .append("&orderId=");
        appendNumber(query, order_id);

        std::string signature = signHMAC(host.secret_key, query);
        std::string target = std::format("{}?{}&signature={}", "/api/v3/order", query, signature);

        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };

        return del(target, headers, error);
    }

    /**
    * @brief Change leverage for a symbol (FUTURES only)
    * 
    * Adjusts the leverage multiplier for a specific trading symbol.
    * Higher leverage increases both potential profit and risk.
    * 
    * @param symbol Trading pair symbol (e.g., "BTCUSDT")
    * @param leverage Leverage multiplier (1-125, depending on symbol and tier)
    * @return JSON string containing new leverage and max notional value
    * 
    * @note Only available for futures markets
    * @note Maximum leverage depends on position size and symbol
    * @note Changing leverage affects margin requirements
    * @note Cannot be changed while there are open orders
    * @note Weight: 1
    * 
    * @throws May throw if invalid leverage or open orders exist
    */
    std::string setLeverage(auto& host, const TokenPair& symbol, int leverage) requires(IsFutures<M>) {
        std::string query = "symbol=" + Binance::tokenPairToString[symbol]
            + "&leverage=" + std::to_string(leverage)
            + "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = signHMAC(host.secret_key, query);
        std::string target = "/fapi/v1/leverage?" + query + "&signature=" + signature;
        
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };
    
        return host.rest_client.post(target, headers);
    }

    /**
    * @brief Change margin type for a symbol (FUTURES only)
    * 
    * Switches between isolated and cross margin mode for a specific symbol.
    * 
    * Margin types:
    * - ISOLATED: Position margin is isolated per symbol. Liquidation only affects that position.
    * - CROSSED: All cross positions share the same margin pool. Liquidation affects all positions.
    * 
    * @param symbol Trading pair symbol (e.g., "BTCUSDT")
    * @param margin_type "ISOLATED" or "CROSSED"
    * @return JSON string containing confirmation
    * 
    * @note Only available for futures markets
    * @note Cannot be changed with open positions or orders
    * @note ISOLATED is safer but requires more margin per position
    * @note CROSSED provides better capital efficiency but higher risk
    * @note Weight: 1
    * 
    * @throws May throw if open positions or orders exist
    */
    std::string setMarginType(auto& host, const TokenPair& symbol, const std::string& margin_type) requires(IsFutures<M>) {
        std::string query = "symbol=" + Binance::tokenPairToString[symbol]
            + "&marginType=" + margin_type
            + "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = sign(host.secret_key, query);
        std::string target = "/fapi/v1/marginType?" + query + "&signature=" + signature;
        
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", host.api_key}
        };
    
        return host.rest_client.post(target, headers);
    }

    /**
    * @brief Connect to Binance WebSocket API and subscribe to user data stream using signature
    *
    * Sends `userDataStream.subscribe.signature` on open with `apiKey`,
    * `timestamp`, optional `recvWindow`, and HMAC-SHA256 `signature`.
    *
    * @tparam Callback Callback function type (must be function pointer)
    * @param callback Message handler: void(*)(std::string_view)
    * @param host Binance WebSocket API host and port (default: "ws-api.binance.com:443")
    * @param path Binance WebSocket API path (default: "/ws-api/v3")
    * @param request_id Request id sent in the subscription JSON RPC payload
    * @param recv_window Optional recvWindow in milliseconds (0 omits the field)
    */
    template<typename Callback>
    void connectUserData(
        auto& host,
        Callback callback,
        //const std::string& host = "ws-api.binance.com:443",
        const std::string& host_url = "ws-api.testnet.binance.vision",
        const std::string& path = "/ws-api/v3",
        std::uint64_t request_id = 1,
        std::uint64_t recv_window = 5000
    ) {
        host.ws_client.connectEndpoint(
            callback,
            host,
            path,
            // TODO make alias for ix::WebSocket
            [this, request_id, recv_window, &host](const std::string& url) {
                if (host.api_key.empty() || host.secret_key.empty()) {
                    host.logger("Cannot subscribe (signature) to Binance user data stream on " + url + ": API key/secret key is empty");
                    return;
                }

                const std::uint64_t timestamp = currentTimestamp();

                // Signature payload must use the same parameter ordering as sent.
                std::string signature_payload = "apiKey=" + host.api_key;

                if (recv_window > 0) {
                    signature_payload += "&recvWindow=" + std::to_string(recv_window);
                }

                signature_payload += "&timestamp=" + std::to_string(timestamp);

                const std::string signature = signHMAC(host.secret_key, signature_payload);

                std::string subscribe_message =
                    "{\"id\":" + std::to_string(request_id) +
                    ",\"method\":\"userDataStream.subscribe.signature\",\"params\":{\"apiKey\":\"" + host.api_key + "\"";

                if (recv_window > 0) {
                    subscribe_message += ",\"recvWindow\":" + std::to_string(recv_window);
                }

                subscribe_message +=
                    ",\"timestamp\":" + std::to_string(timestamp) +
                    ",\"signature\":\"" + signature + "\"}}";

                bool result = host.ws_client.send(url, subscribe_message);
                if (!result) {
                    host.logger("Failed to subscribe (signature) to Binance user data stream on " + url);
                } else {
                    host.logger("Subscribed (signature) to Binance user data stream on " + url);
                }
            }
        );
    }

    template<typename Callback>
    void connectDepthFeed(auto& host, const std::vector<TokenPair>& tokens, Callback callback) {
        // Connect to the depth endpoint
        std::string s = "/ws";
        for (const auto& token : tokens) {
            s += std::format("/{}@depth", toLower(Binance::tokenPairToString[token])); 
        }

        const std::string address = std::format("wss://{}{}", BINANCE_WS_HOST, s);
        host.logger("Connected to Binance Depth Data Feed at address: {}", address);

        host.ws_client.connectEndpoint(
            callback,
            host,
            s
        );
    }

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
        host.logger("Connecting to Binance Market Data Feed: {}", address);

        host.ws_client.connectEndpoint(
            callback,
            BINANCE_WS_HOST,
            aggregate
        );
    }

};

} // namespace trade_connector