#pragma once

#include "baseclient.h"

namespace trade_connector{

    /// @brief Asynchronous trading client with optional mixin extensions.
    /// @tparam Config      Exchange policy (e.g. BinancePolicy<MarketType::SPOT>)
    /// @tparam Extensions  Zero or more CRTP mixin templates for user-defined functionality
    template<typename Config, template<class> class... Extensions>
    class AsyncClient : public BaseClient<AsyncClient<Config, Extensions...>, Config, Config::market_type>, 
                       public Extensions<AsyncClient<Config, Extensions...>>... {
        public:
        AsyncClient(
            const std::string& rest_host,
            const std::string& ws_host,
            const std::string& api_key, 
            const std::string& secret_key, 
        std::function<void(const std::string&)> logger = null_logger) 
        : BaseClient<AsyncClient<Config, Extensions...>, Config, Config::market_type>(rest_host, ws_host, api_key, secret_key, logger) {}

        AsyncClient(const AsyncClient&) = delete;
        AsyncClient& operator=(const AsyncClient&) = delete;
        
        /** @brief Could implement these later if needed */
        AsyncClient(AsyncClient&&) noexcept = delete;
        AsyncClient& operator=(AsyncClient&&) noexcept = delete;
};

} // namespace trade_connector