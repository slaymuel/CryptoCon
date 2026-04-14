#include <iostream>
#include <atomic>
#include <clients/syncclient.h>
#include <clients/binancepolicy.h>

using namespace trade_connector;

/// Mixin that prints every WebSocket message and tracks message count.
template<class Derived>
struct MessagePrinter {
    void printStats() const {
        std::cout << "Total messages received: " << message_count_.load() << std::endl;
    }

    /// Subscribe to trades and print each message as it arrives.
    void connectAndPrint(const std::vector<TokenPair>& tokens) {
        auto& self = static_cast<Derived&>(*this);

        self.connectTradeFeed(tokens,
            [this](std::string_view msg) {
                message_count_.fetch_add(1, std::memory_order_relaxed);
                std::cout << "[MSG #" << message_count_.load() << "] " << msg << "\n";
            }
        );
    }

private:
    std::atomic<uint64_t> message_count_{0};
};

int main() {
    auto logger = [](const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    };

    // No API key needed for public market data
    SyncClient<BinancePolicy<MarketType::SPOT>, MessagePrinter> client("", "", logger);

    std::cout << "Subscribing to BTC and ETH trade feeds...\n";
    client.connectAndPrint({TokenPair::BTCUSD, TokenPair::ETHUSD});

    // Block until the WebSocket connection closes
    client.websocketClient().wait();

    client.printStats();
}
