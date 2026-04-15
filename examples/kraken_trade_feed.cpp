#include <iostream>
#include <iostream>
#include <clients/syncclient.h>
#include <policies/krakenpolicy.h>

using namespace trade_connector;

int main() {
    auto logger = [](const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    };

    // No API key needed for public market data
    SyncClient<KrakenPolicy<MarketType::SPOT>> client("", "", logger);

    std::cout << "Subscribing to BTC and ETH trade feeds...\n";
    client.connectTradeFeed(
        {TokenPair::BTCUSD, TokenPair::ETHUSD},
        +[](std::string_view msg) {
            std::cout << "[TRADE] " << msg << "\n";
        }
    );

    // Block until the WebSocket connection closes
    client.websocketClient().wait();
}