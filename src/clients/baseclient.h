#pragma once

#include "../rest/client.h"
#include "../websocket/client.h"

namespace trade_connector {

/// @brief CRTP base providing REST, WebSocket and policy-dispatched trading operations.
/// @tparam Derived  SyncClient or AsyncClient (CRTP leaf)
/// @tparam Config   Exchange policy (e.g. BinancePolicy<M>)
/// @tparam M        Market type used for OrderParams specialisation
template<typename Derived, typename Config, MarketType M = MarketType::GENERIC>
class BaseClient{

public:

    BaseClient(
        const std::string& api_key, 
        const std::string& secret_key, 
        std::function<void(const std::string&)> logger = null_logger) 
        : api_key(api_key), 
          secret_key(secret_key), 
          rest_client(config.restHost(), logger), 
          ws_client(config.wsHost(), api_key, secret_key, logger), 
          logger(logger),
          derived(static_cast<Derived&>(*this)){}

    BaseClient& operator=(const BaseClient&) = delete;
    BaseClient(const BaseClient&) = delete;

    const std::string& apiKey() const {
        return api_key;
    }

    const std::string& secretKey() const {
        return secret_key;
    }

    rest::Client& restClient() {
        return rest_client;
    }

    const rest::Client& restClient() const {
        return rest_client;
    }

    websocket::Client& websocketClient() {
        return ws_client;
    }

    const websocket::Client& websocketClient() const {
        return ws_client;
    }

    std::string buildQuery(const OrderParams<M>& params) const {
        return config.buildQuery(params);
    }

    bool isConnected(const std::string& endpoint) const {
        return ws_client.isConnected(endpoint);
    }
    
    void disconnect(const std::string& endpoint) {
        ws_client.disconnect(endpoint);
    }

    /// Place an order via the Config policy. Compile-time error if unsupported.
    std::string sendOrder(const OrderParams<M>& params, Error& error = dummy_error) {
        if constexpr(requires(Config c, BaseClient& b, const OrderParams<M>& p, Error& e){c.sendOrder(b, p, e); }) {
            return config.sendOrder(*this, params, error);
        } else {
            logger("sendOrder is not implemented for this client configuration");
            return "";
        }
    }

    /// Retrieve all open orders (Config must implement getOpenOrders).
    std::string getOpenOrders() {
        if constexpr(requires(Config c, BaseClient& b){c.getOpenOrders(b); }) {
            return config.getOpenOrders(*this);
        } else {
            logger("getOpenOrders is not implemented for this client configuration");
            return "";
        }
    }

    /// Cancel all open orders (Config must implement cancelAllOpenOrders).
    void cancelAllOpenOrders() {
        if constexpr(requires(Config c, BaseClient& b){c.cancelAllOpenOrders(b); }) {
            return config.cancelAllOpenOrders(*this);
        } else {
            logger("cancelAllOpenOrders is not implemented for this client configuration");
        }
    }

    /// Retrieve account information and balances.
    std::string getAccountInfo() {
        if constexpr(requires(Config c, BaseClient& b){c.getAccountInfo(b); }) {
            return config.getAccountInfo(*this);
        } else {
            logger("getAccountInfo is not implemented for this client configuration");
            return "";
        }
    }

    std::string getOrderBook(const TokenPair& symbol, int limit = 100) {
        if constexpr(requires(Config c, BaseClient& b, const TokenPair& s, int l){c.getOrderBook(b, s, l); }) {
            return config.getOrderBook(*this, symbol, limit);
        } else {
            logger("getOrderBook is not implemented for this client configuration");
            return "";
        }
    }

    std::string cancelOrder(const TokenPair& symbol, uint64_t order_id, Error& error = dummy_error) {
        if constexpr(requires(Config c, BaseClient& b, const TokenPair& s, uint64_t o, Error& e){c.cancelOrder(b, s, o, e); }) {
            return config.cancelOrder(*this, symbol, order_id, error);
        } else {
            logger("cancelOrder is not implemented for this client configuration");
            return "";
        }
    }

    /// Subscribe to authenticated user data feed (order updates, balance changes).
    template<typename Callback>
    void connectUserDataFeed(
        Callback callback,
        const std::string& path = "/ws-api/v3",
        const std::uint64_t request_id = 1,
        const std::uint64_t recv_window = 5000
    ){
        if constexpr(requires(Config c, BaseClient& b, Callback cb, const std::string& p, const std::uint64_t rid, const std::uint64_t rw){c.connectUserDataFeed(b, cb, p, rid, rw); }) {
            return config.connectUserDataFeed(*this, callback, path, request_id, recv_window);
        } else {
            logger("connectUserDataFeed is not implemented for this client configuration");
        }
    }

    /// Subscribe to order-book depth stream for the given token pairs.
    template<typename Callback>
    void connectDepthFeed(const std::vector<TokenPair>& tokens, Callback callback){
        if constexpr(requires(Config c, BaseClient& b, const std::vector<TokenPair>& t, Callback cb){c.connectDepthFeed(b, t, cb); }) {
            return config.connectDepthFeed(*this, tokens, callback);
        } else {
            logger("connectDepthFeed is not implemented for this client configuration");
        }
    }

    /// Subscribe to aggregate trade stream for the given token pairs.
    template<typename Callback>
    void connectTradeFeed(const std::vector<TokenPair>& tokens, Callback callback){
        if constexpr(requires(Config c, BaseClient& b, const std::vector<TokenPair>& t, Callback cb){c.connectTradeFeed(b, t, cb); }) {
            return config.connectTradeFeed(*this, tokens, callback);
        } else {
            logger("connectTradeFeed is not implemented for this client configuration");
        }
    }

public:
    std::function<void(const std::string&)> logger;  ///< Logger instance for logging messages

protected:
    const std::string secret_key;    ///< Exchange secret key for request signing
    const std::string api_key;       ///< Exchange API key for authentication
    websocket::Client ws_client;
    rest::Client rest_client;

private:
    Derived& derived;
    Config config;
};

} // namespace trade_connector