#pragma one
#include "baseclient.h"

namespace trade_connector{

    // Host class for the policies. Users can also derive from this if they want to implement custom clients with more functionality.
    template<typename Config>
    class SyncClient : public BaseClient<SyncClient<Config>, Config, Config::market_type>
    {
        public:
        SyncClient(
            const std::string& rest_host,
            const std::string& ws_host,
            const std::string& api_key, 
            const std::string& secret_key, 
        std::function<void(const std::string&)> logger = null_logger) 
        : BaseClient<SyncClient<Config>, Config, Config::market_type>(rest_host, ws_host, api_key, secret_key, logger) {}

        SyncClient(const SyncClient&) = delete;
        SyncClient& operator=(const SyncClient&) = delete;
        
        /** @brief Could implement these later if needed */
        SyncClient(SyncClient&&) noexcept = delete;
        SyncClient& operator=(SyncClient&&) noexcept = delete;
};

} // namespace trade_connector