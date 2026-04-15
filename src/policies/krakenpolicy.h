#include "types.h"
#include <format>
#include <vector>

namespace trade_connector {

template<MarketType M = MarketType::GENERIC>
struct KrakenPolicy{
    constexpr static const char* KRAKEN_REST_HOST = "api.kraken.com";
    constexpr static const char* KRAKEN_WS_HOST = "ws.kraken.com";
    
    constexpr const char* restHost() const {
        return KRAKEN_REST_HOST;
    }

    constexpr const char* wsHost() const {
        return KRAKEN_WS_HOST;
    }

    /// Subscribe to aggregated trade updates for one or more token pairs.
    template<typename Callback>
    void connectTradeFeed(auto& host, const std::vector<TokenPair>& tokens, const Callback callback) const {
        std::string subscribe_message = "{\"event\":\"subscribe\",\"pair\":[";
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) subscribe_message += ",";
            subscribe_message += std::format("\"{}\"", Kraken::tokenPairToString[tokens[i]]);
        }
        subscribe_message += "],\"subscription\":{\"name\":\"trade\"}}";
        host.websocketClient().connectEndpoint(
            callback,
            KRAKEN_WS_HOST,
            "",
            [this, &host, subscribe_message](const std::string& url) {
                host.websocketClient().send(subscribe_message);
                host.logger("Subscribed to Kraken trade feed on " + url);
            }
        );
    }
};

}