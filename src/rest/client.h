/**
 * @brief REST API client for cryptocurrency exchange interactions
 * 
 * Provides a high-performance, template-based REST client for interacting with
 * cryptocurrency exchange APIs. Supports both spot and futures markets with
 * compile-time market-specific method validation using C++20 concepts.
 * 
 * Features:
 * - Market-specific endpoint configuration via templates
 * - Persistent HTTPS connections with keep-alive
 * - Automatic request signing (HMAC-SHA256)
 * - Connection resilience with automatic reconnection
 * - Type-safe order submission with OrderParams
 */

#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <simdjson.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include "../utils/utils.h"
#include "../types.h"
#include "../configs.h"

namespace trade_connector::rest {

/**
 * @class Client
 * @brief Market-specific REST API client for exchange operations
 * 
 * Thread-safe REST client that maintains a persistent HTTPS connection to the
 * exchange. Handles authentication, request signing, and automatic reconnection.
 * 
 * The client is templated on MarketType, enabling compile-time validation of
 * market-specific methods (e.g., futures-only leverage settings).
 * 
 * @tparam M Market type (MarketType::SPOT or MarketType::FUTURES)
 * 
 * Key features:
 * - Persistent SSL/TLS connection with HTTP keep-alive
 * - Automatic HMAC-SHA256 request signing
 * - Market-specific endpoint routing
 * - Type-safe order parameter handling
 * - Connection health monitoring and reconnection
 * 
 * @note Non-copyable, non-movable to maintain connection integrity
 * @note All methods are blocking and thread-safe
 * 
 * @example
 * ```cpp
 * Client<MarketType::SPOT> client(api_key, secret_key);
 * auto account = client.getAccountInfo();
 * 
 * order_builder.
 *       reset().
 *       symbol("BTCUSDT").
 *       side("BUY").
 *       type("LIMIT").
 *       timeInForce("GTC").
 *       quantity(0.001).
 *       price(50000.0).
 *       quoteOrderQty(100.0).
 *       timestamp(currentTimestamp());
 * auto response = rest_client.sendOrder(order_builder.build());
 * ```
 */
template<
    template<MarketType> typename ExchangeConfig, 
    MarketType M = MarketType::GENERIC
>
class Client{
    using Config = ExchangeConfig<M>;
    using Headers = std::span<const std::pair<std::string, std::string>>;

public:

    static void null_logger(const std::string&) {}
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
    Client(
        const std::string& host, 
        const std::string& api_key, 
        const std::string& secret_key,
        std::function<void(const std::string&)> logger = null_logger
        
    ) : 
      host(host), 
      api_key(api_key), 
      secret_key(secret_key),
      ssl_ctx(boost::asio::ssl::context::sslv23_client), 
      stream(ioc, ssl_ctx),
      logger(logger) {
        ssl_ctx.set_default_verify_paths();
        connect();
    }

    /**
     * @brief Construct REST client with default exchange URL
     * 
     * Creates a REST client using the default testnet URL from the market configuration.
     * Automatically establishes HTTPS connection upon construction.
     * 
     * @param api_key Exchange API key for authentication
     * @param secret_key Exchange secret key for request signing
     * 
     * @note Uses Config::test_url as the default host
     * @throws boost::beast::system_error if connection fails
     */
    Client(
        const std::string& api_key, 
        const std::string& secret_key,
        std::function<void(const std::string&)> logger = null_logger
    ) : 
      host(Config::test_url), 
      api_key(api_key), 
      secret_key(secret_key),
      ssl_ctx(boost::asio::ssl::context::sslv23_client), 
      stream(ioc, ssl_ctx),
      logger(logger) {

        ssl_ctx.set_default_verify_paths();
        connect();
    }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    
    /** @brief Could implement these later if needed */
    Client(Client&&) noexcept = delete;
    Client& operator=(Client&&) noexcept = delete;

    /**
     * @brief Destructor - gracefully closes the HTTPS connection
     * 
     * Performs a clean SSL shutdown before destroying the client.
     * Errors during shutdown are ignored to ensure destructor never throws.
     */
    ~Client() {
        // TODO: Check for errors
        boost::beast::error_code ec;
        stream.shutdown(ec);
    }

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
    std::string createListenKey(){
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
        };

        auto response = post(Config::listen_key, headers);
        simdjson::ondemand::parser parser;
        simdjson::padded_string json(response);
        std::string listen_key;
        try{
            auto doc = parser.iterate(json);
            listen_key = std::string(doc["listenKey"].get_string().value());
        }
        catch(const simdjson::simdjson_error& e){
            logger("Failed to fetch listen key");
            logger("Full response: " + response);
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
    std::string keepAliveListenKey(const std::string& listen_key){
        std::string target = std::string(Config::listen_key) + "?listenKey=" + listen_key;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
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
    std::string closeListenKey(const std::string& listen_key){
        std::string target = std::string(Config::listen_key) + "?listenKey=" + listen_key;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
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
    std::string getAccountInfo(){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                  + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::account_info) + "?" + query + "&signature=" + signature;
        //std::string target = "/api/v3/account?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
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
    std::string getOpenPositions() requires(IsFutures<M>){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                  + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::open_positions) + "?" + query + "&signature=" + signature;
        //std::string target = "/fapi/v2/positionRisk?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
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
    std::string ping(){
        std::string target = Config::ping;
        return get(target);
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
    std::string getServerTime(){
        return get(Config::server_time);
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
    std::string getExchangeInfo(){
        return get(Config::exchange_info);
    }

    std::string getOpenOrders() requires(IsSpot<M>){
        std::string query = "timestamp=" + std::to_string(currentTimestamp())
                  + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::open_orders) + "?" + query + "&signature=" + signature;
        //std::string target = "/api/v3/openOrders?" + query + "&signature=" + signature;
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
        };

        return get(target, headers);
    }

    void cancelAllOpenOrders() requires(IsSpot<M>){
        auto open_orders = getOpenOrders();

        simdjson::ondemand::parser parser;
        simdjson::padded_string json_orders(open_orders);
        try{
            auto orders_doc = parser.iterate(json_orders);
            auto orders = orders_doc.get_array().value();
            for (auto order : orders) {
                auto symbol = order["symbol"].get_string().value();
                uint64_t order_id = order["orderId"].get_uint64().value();
                cancelOrder(symbol, order_id);
            }
        }
        catch(const simdjson::simdjson_error& e){
            logger("Failed to cancel orders");
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
    std::string getOrderBook(const std::string& symbol, int limit = 100){
        std::string target = std::string(Config::depth) + "?symbol=" + symbol + "&limit=" + std::to_string(limit);
        return get(target);
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
    std::string getRecentTrades(const std::string& symbol, int limit = 500){
        std::string target = std::string(Config::trades) + "?symbol=" + symbol + "&limit=" + std::to_string(limit);
        return get(target);
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
    std::string sendOrder(const T& params) {
        // buildquery also templated since now params is different for oco, market, limit etc
        std::string query = buildQuery(params);
        std::string signature = sign(secret_key, query);
        auto endpoint = (params.type == OrderType::OCO) ? Config::oco : Config::order;
        std::string target = std::string(endpoint) + "?" + query + "&signature=" + signature;
        return sendOrder(target);
    }

    std::string sendOrder(const OrderParams<M>& params, Error& error = dummy_error) {
        std::string query = buildQuery(params);
        std::string signature = sign(secret_key, query);
        auto endpoint = (params.type == OrderType::OCO) ? Config::oco : Config::order;
        std::string target = std::string(endpoint) + "?" + query + "&signature=" + signature;
        return sendOrder(target, error);
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
    std::string sendOrder(std::string target, Error& error = dummy_error) {
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
        };
        
        return post(target, headers, "", error);
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
        const std::string_view& symbol,
        uint64_t order_id,
        Error& error = dummy_error
    ) {
        std::string query;
        query.reserve(256); // Preallocate for performance
        query.append("symbol=").append(symbol)
             .append("&timestamp=").append(std::to_string(currentTimestamp()))
             .append("&orderId=");
        appendNumber(query, order_id);

        std::string signature = sign(secret_key, query);
        std::string target = std::string(Config::order) + "?" + query + "&signature=" + signature;

        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
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
    std::string setLeverage(const std::string& symbol, int leverage) requires(IsFutures<M>) {
        std::string query = "symbol=" + symbol
            + "&leverage=" + std::to_string(leverage)
            + "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = "/fapi/v1/leverage?" + query + "&signature=" + signature;
        
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
        };
    
        return post(target, headers);
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
    std::string setMarginType(const std::string& symbol, const std::string& margin_type) requires(IsFutures<M>) {
        std::string query = "symbol=" + symbol
            + "&marginType=" + margin_type
            + "&timestamp=" + std::to_string(currentTimestamp())
            + "&recvWindow=5000";

        std::string signature = sign(secret_key, query);
        std::string target = "/fapi/v1/marginType?" + query + "&signature=" + signature;
        
        std::pair<std::string, std::string> headers[] = {
            {"X-MBX-APIKEY", api_key}
        };
    
        return post(target, headers);
    }

    /**
     * @brief Read HTTP response from the persistent connection
     * 
     * Reads the complete HTTP response including headers and body from the
     * SSL stream. Handles connection errors and triggers reconnection if needed.
     * 
     * @return Response body as string (empty string on error)
     * 
     * @note Blocking call - waits for complete response
     * @note Automatically attempts reconnection on connection failure
     * @note Errors are logged to std::cout
     */
    std::string readFromStream(
        Error& error = dummy_error
    ) {
        boost::beast::error_code ec;
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::string_body> res;
        boost::beast::http::read(stream, buffer, res, ec);

        if (ec) {
            std::string error_message = "Error reading from stream: " + ec.message();
            error = Error(error_message, 1);
            logger("Error reading from stream: " + ec.message() 
              + " [" + ec.category().name() 
              + ":" + std::to_string(ec.value()) + "]");
            // Connection died, try to reconnect
            reconnectStream();

            return "";
        }

        return res.body();
    }

    template<typename RequestType>
    void writeToStream(
        RequestType& request,
        Error& error = dummy_error
    ) {
        boost::beast::error_code ec;
        boost::beast::http::write(stream, request, ec);

        if (ec) {
            std::string error_message = "Error writing to stream: " + ec.message();
            error = Error(error_message, 2);
            logger("Error writing to stream: " + ec.message() 
              + " [" + ec.category().name() 
              + ":" + std::to_string(ec.value()) + "]");
            // Connection died, try to reconnect
            reconnectStream();
        }
    }

    /**
     * @brief Reconnect the SSL/TLS stream
     * 
     * Attempts to re-establish the HTTPS connection after a connection failure.
     * Called automatically by readFromStream() and writeToStream() on errors.
     * 
     * @note Logs reconnection attempt to std::cout
     */
    void reconnectStream() {
        logger("Reconnecting stream...");
        connect();
    }

    /**
     * @brief Send HTTP POST request
     * 
     * Constructs and sends an HTTP POST request with optional headers and body.
     * Uses the persistent connection with HTTP/1.1 keep-alive.
     * 
     * @param target Request target/path (e.g., "/api/v3/order")
     * @param headers Optional custom headers (e.g., {"X-MBX-APIKEY", key})
     * @param body Optional request body (default: empty)
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string post(
        const std::string& target,
        Headers headers = {},
        const std::string& body = "",
        Error& error = dummy_error
    ) {
        // Build HTTP POST request
        RequestString req{
            boost::beast::http::verb::post, target, 
            11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
        req.body() = body;
        req.prepare_payload();
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        logger("POST " + target + " Body: " + body);

        // Send request
        writeToStream<RequestString>(req, error);
    
        // Receive response
        auto result = readFromStream(error);
        return result;
    }

    /**
     * @brief Send HTTP GET request
     * 
     * Constructs and sends an HTTP GET request with optional headers.
     * Uses the persistent connection with HTTP/1.1 keep-alive.
     * 
     * @param target Request target/path with query string (e.g., "/api/v3/ping")
     * @param headers Optional custom headers (e.g., {"X-MBX-APIKEY", key})
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string get(
        const std::string&  target,
        Headers headers = {},
        Error& error = dummy_error
    ) {
        // Build HTTP GET request
        // HTTP version 1.1 is represented by 11
        RequestEmpty req{boost::beast::http::verb::get, target, 11};
        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        // Send request
        writeToStream<RequestEmpty>(req, error);
    
        // Receive response
        auto result = readFromStream(error);
        return result;
    }

    /**
     * @brief Send HTTP PUT request
     * 
     * Constructs and sends an HTTP PUT request with optional headers and body.
     * Used for updating resources (e.g., extending listen key validity).
     * 
     * @param target Request target/path
     * @param headers Optional custom headers
     * @param body Optional request body (default: empty)
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string put(
        const std::string& target,
        Headers headers = {},
        const std::string& body = "",
        Error& error = dummy_error
    ) {
        RequestString req{
            boost::beast::http::verb::put, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
        req.body() = body;
        req.prepare_payload();
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        writeToStream<RequestString>(req, error);
    
        auto result = readFromStream(error);
        return result;
    }

    /**
     * @brief Send HTTP DELETE request
     * 
     * Constructs and sends an HTTP DELETE request with optional headers.
     * Used for deleting resources (e.g., closing listen keys, canceling orders).
     * 
     * @param target Request target/path
     * @param headers Optional custom headers
     * @param error Error object for capturing errors
     * @return Response body as string
     * 
     * @note Automatically sets Host, User-Agent, and Connection headers
     * @note Uses HTTP/1.1
     */
    std::string del(
        const std::string& target,
        Headers headers = {},
        Error& error = dummy_error
    ) {
        RequestEmpty req{
            boost::beast::http::verb::delete_, target, 11
        };

        req.set(boost::beast::http::field::host, host);
        req.set(boost::beast::http::field::user_agent, "trade-connector-client");
        req.set(boost::beast::http::field::connection, "keep-alive");
    
        for (auto& h : headers)
            req.set(h.first, h.second);

        writeToStream<RequestEmpty>(req, error);
    
        auto result = readFromStream(error);
        return result;
    }

private:

    /**
     * @brief Establish HTTPS connection to the exchange
     * 
     * Performs DNS resolution, TCP connection, and SSL/TLS handshake.
     * Called by constructors to establish initial connection and by
     * reconnectStream() to restore connection after failures.
     * 
     * Process:
     * 1. DNS resolution of hostname to IP addresses
     * 2. TCP connection to port 443 (HTTPS)
     * 3. SNI (Server Name Indication) configuration
     * 4. TLS handshake and certificate verification
     * 
     * @throws boost::beast::system_error if DNS resolution fails
     * @throws boost::beast::system_error if TCP connection fails
     * @throws boost::beast::system_error if SSL handshake fails
     * @throws boost::beast::system_error if certificate verification fails
     */
    void connect(){
        // Resolve host into ip addresses
        boost::asio::ip::tcp::resolver resolver(ioc);
        // Port 443 is the standard HTTPS port.
        auto const results = resolver.resolve(host, "443");
    
        // Connect to IP over TCP. TLS layer on top of TCP layer
        // Connect the TCP layer before TLS can start.
        boost::beast::get_lowest_layer(stream).connect(results);
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        {
            boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::beast::system_error{ec};
        }
        // TLS handshake. upgrade TCP connection to a secure HTTPS connection
        stream.handshake(boost::asio::ssl::stream_base::client);
    }

    const std::string host;          ///< Exchange API hostname
    const std::string api_key;       ///< Exchange API key for authentication
    const std::string secret_key;    ///< Exchange secret key for request signing
    boost::asio::io_context ioc;    ///< Boost.Asio I/O context for async operations
    boost::asio::ssl::context ssl_ctx;  ///< SSL context with system certificate store
    boost::asio::ssl::stream<boost::beast::tcp_stream> stream;  ///< Persistent SSL/TLS stream
    std::function<void(const std::string&)> logger;                  ///< Logger instance for logging messages
};

} // namespace trade_connector::rest