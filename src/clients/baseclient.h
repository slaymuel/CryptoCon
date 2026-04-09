#pragma once

#include "../rest/client.h"
#include "../websocket/client.h"
#include "../utils/signing.h"

namespace trade_connector {
    
static void null_logger(const std::string&) {}
/**
 * @class Client
 * @brief Unified client interface for REST and WebSocket interactions
 * Provides a high-level client that encapsulates both REST and WebSocket 
 * clients for a given exchange and market type. This class serves as the 
 * main entry point and provides easy access to both REST and WebSocket 
 * functionalities through a single interface.
 * 
 * @example
 * ```cpp
 * auto client = Client<BinanceConfig<MarketType::SPOT>>(api_key, secret_key);
 * ```
 */
 // We probably have to use CRTP since the configs should probably have access to the clients.
 // Should add convinience methods that make it easier to make custom configs, like signing etc.
 // API would then be something like:
 // auto client = BinanceClient(api_key, secret_key);
 // Where BinanceClient is a type alias for Client<BinanceConfig<MarketType::SPOT>>
 // TODO: Add some hooks for users.
template<typename Derived, typename Config, MarketType M = MarketType::GENERIC>
class BaseClient{

public:

    BaseClient(
        const std::string& rest_host, 
        const std::string& ws_host, 
        const std::string& api_key, 
        const std::string& secret_key, 
        std::function<void(const std::string&)> logger = null_logger) 
        : rest_host(rest_host), 
          ws_host(ws_host), 
          api_key(api_key), 
          secret_key(secret_key), 
          rest_client(rest_host, logger), 
          ws_client(api_key, secret_key, logger), 
          logger(logger){
        // SyncClient or AsyncClient
        derived = static_cast<Derived&>(*this);
    }

    const std::string& apiKey(){
        return api_key;
    }

    const std::string& secretKey(){
        return secret_key;
    }

    rest::Client& restClient(){
        return rest_client;
    }

    websocket::Client& websocketClient(){
        return ws_client;
    }

    std::string buildQuery(const OrderParams<M>& params){
        return config.buildQuery(params);
    }

    std::string sendOrder(const OrderParams<M>& params, Error& error = dummy_error) {
        if constexpr(requires(Config c, BaseClient& b, const OrderParams<M>& p, Error& e){c.sendOrder(b, p, e); }) {
            return config.sendOrder(*this, params, error);
        } else {
            logger("sendOrder is not implemented for this client configuration");
            return "";
        }
    }
    
    template<typename Callback>
    void connectUserData(
        Callback callback,
        //const std::string& host = "ws-api.binance.com:443",
        const std::string& host = "ws-api.testnet.binance.vision",
        const std::string& path = "/ws-api/v3",
        std::uint64_t request_id = 1,
        std::uint64_t recv_window = 5000
    ){
        if constexpr(requires(Config c, BaseClient& b, Callback cb, const std::string& h, const std::string& p, std::uint64_t r, std::uint64_t rw){c.connectUserData(b, cb, h, p, r, rw); }) {
            return config.connectUserData(*this, callback, host, path, request_id, recv_window);
        } else {
            logger("connectUserData is not implemented for this client configuration");
        }
    }
    std::string sendOrder(auto& host, const OrderParams<M>& params, Error& error = dummy_error) {
        return config.sendOrder(host, params, error);
    }

protected:
    const std::string rest_host;     ///< Exchange REST API hostname
    const std::string ws_host;       ///< Exchange WebSocket API hostname
    const std::string secret_key;    ///< Exchange secret key for request signing
    const std::string api_key;       ///< Exchange API key for authentication
    websocket::Client ws_client;
    rest::Client rest_client;
    std::function<void(const std::string&)> logger;  ///< Logger instance for logging messages

private:
    Derived& derived;
    Config config;
};

} // namespace trade_connector