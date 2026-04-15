#pragma once

#include "baseclient.h"

namespace trade_connector{

    /// @brief Synchronous trading client with optional mixin extensions.
    /// @tparam Config      Exchange policy (e.g. BinancePolicy<MarketType::SPOT>)
    /// @tparam Extensions  Zero or more CRTP mixin templates for user-defined functionality
    template<typename Config, template<class> class... Extensions>
    class SyncClient : public BaseClient<SyncClient<Config, Extensions...>, Config>, 
                       public Extensions<SyncClient<Config, Extensions...>>... {
    public:
        SyncClient(
            const std::string& api_key = "", 
            const std::string& secret_key = "", 
            std::function<void(const std::string&)> logger = null_logger) 
        : BaseClient<SyncClient<Config, Extensions...>, Config>(api_key, secret_key, logger) {}

        SyncClient(const SyncClient&) = delete;
        SyncClient& operator=(const SyncClient&) = delete;
        
        /** @brief Could implement these later if needed */
        SyncClient(SyncClient&&) noexcept = delete;
        SyncClient& operator=(SyncClient&&) noexcept = delete;
};

} // namespace trade_connector