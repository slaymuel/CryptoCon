# 🚀 TradeConnector (Work  in progress)

> **Modern and performant trading library for cryptocurrency exchanges**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

A performant and type-safe library including REST (using [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/)) and websocket (using [IXWebSocket](https://github.com/machinezone/IXWebSocket)) clients for communicating with cryptocurrency exchanges. TradeConnector is designed for algorithmic trading and focuses on performance and usability. Currently has support for the Binance and experimental support for Kraken and Coinbase.

---

## ✨ Key Features

### 🎯 **Type Safety with Zero Runtime Cost**
```cpp
// Compile-time market validation using C++20 concepts
rest::Client<BinanceConfig, MarketType::SPOT>    spot_client(api_key, secret);
rest::Client<BinanceConfig, MarketType::FUTURES> futures_client(api_key, secret);

spot_client.setLeverage("BTCUSDT", 10);    // ❌ Compile error - not available for SPOT
futures_client.setLeverage("BTCUSDT", 10); // ✅ Compiles - futures only
```

### 🛡️ **Modern C++ Design**
- C++20 concepts for compile-time constraints
- Builder pattern for intuitive order construction
- Template-based market specialization

---

## 🚀 Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt install libssl-dev libboost-all-dev

# Install IXWebSocket (required)
git clone https://github.com/machinezone/IXWebSocket.git
cd IXWebSocket && mkdir build && cd build
cmake -DUSE_TLS=1 ..
make -j4 && sudo make install

# macOS
brew install openssl boost ixwebsocket
```

### Installation

**TradeConnector is header-only!** Simply clone and include:

```bash
git clone https://github.com/slaymuel/tradeconnector.git
```

Add to your compiler flags:
```bash
g++ -std=c++20 -O3 main.cpp -lssl -lcrypto -lboost_system -lixwebsocket -lpthread -o trader
```

### Sending an order

```cpp
// main.cpp
#include "tradeconnector/src/rest/client.h"
#include "tradeconnector/src/rest/orderbuilder.h"

using namespace trade_connector;

int main() {
    // Initialize REST client
    rest::Client<BinanceConfig, MarketType::SPOT> client(
        "testnet.binance.vision",  // Testnet for safety
        "your_api_key",
        "your_secret_key"
    );

    // Build and submit order using fluent API
    auto order = rest::OrderBuilder<MarketType::SPOT>()
        .symbol("BTCUSDT")
        .side(Side::BUY)
        .type(OrderType::LIMIT)
        .price(50000.0)
        .quantity(0.001)
        .timeInForce(TimeInForce::GTC)
        .build();

    auto response = client.sendOrder(order);
    std::cout << "Order placed: " << response << std::endl;

    return 0;
}
```

---

## 📖 Usage Examples

### WebSocket Market Data Stream
```cpp
#include "tradeconnector/src/websocket/client.h"
#include <simdjson.h>

using namespace trade_connector;

int main() {
    // Empty credentials for public (unauthenticated) streams
    websocket::Client ws_client("", "");

    // Subscribe to real-time trades
    ws_client.connectEndpoint(
        +[](std::string_view msg) {
            simdjson::ondemand::parser parser;
            simdjson::padded_string padded(msg);
            auto doc = parser.iterate(padded);

            double price = doc["p"].get_double();
            double qty   = doc["q"].get_double();

            std::cout << "Trade: " << price << " @ " << qty << std::endl;
        },
        "stream.binance.com:9443",
        "/ws/btcusdt@trade"
    );

    ws_client.wait();  // Keep running
    return 0;
}
```

### Futures Trading with Leverage
```cpp
#include "tradeconnector/src/rest/client.h"
#include "tradeconnector/src/rest/orderbuilder.h"

using namespace trade_connector;

int main() {
    rest::Client<BinanceConfig, MarketType::FUTURES> client(
        "your_api_key",
        "your_secret_key"
    );

    // Set leverage to 10x
    client.setLeverage("BTCUSDT", 10);

    // Set isolated margin mode
    client.setMarginType("BTCUSDT", "ISOLATED");

    // Place long position
    auto order = rest::OrderBuilder<MarketType::FUTURES>()
        .symbol("BTCUSDT")
        .side(Side::BUY)
        .type(OrderType::MARKET)
        .quantity(0.1)
        .positionSide(Side::LONG)
        .build();

    auto response = client.sendOrder(order);
    std::cout << response << std::endl;

    return 0;
}
```

### Market Making Strategy
```cpp
#include "tradeconnector/src/rest/client.h"
#include "tradeconnector/src/rest/orderbuilder.h"
#include "tradeconnector/src/websocket/client.h"

using namespace trade_connector;

class MarketMaker {
    rest::Client<BinanceConfig, MarketType::SPOT>& rest_client;
    websocket::Client ws_client;
    double best_bid = 0, best_ask = 0;

public:
    MarketMaker(rest::Client<BinanceConfig, MarketType::SPOT>& client)
        : rest_client(client), ws_client("", "") {}

    void start(const std::string& symbol) {
        // Subscribe to order book updates
        ws_client.connectEndpoint(
            [this](std::string_view msg) {
                this->onOrderBookUpdate(msg);
            },
            "stream.binance.com:9443",
            "/ws/" + symbol + "@depth@100ms"
        );

        ws_client.wait();
    }

private:
    void onOrderBookUpdate(std::string_view msg) {
        // Parse order book and update best_bid / best_ask
        placeOrders();
    }

    void placeOrders() {
        // Place bid slightly below best bid
        auto bid = rest::OrderBuilder<MarketType::SPOT>()
            .symbol("BTCUSDT")
            .side(Side::BUY)
            .type(OrderType::LIMIT)
            .price(best_bid - 0.01)
            .quantity(0.001)
            .timeInForce(TimeInForce::GTC)
            .build();

        rest_client.sendOrder(bid);

        // Place ask slightly above best ask
        auto ask = rest::OrderBuilder<MarketType::SPOT>()
            .symbol("BTCUSDT")
            .side(Side::SELL)
            .type(OrderType::LIMIT)
            .price(best_ask + 0.01)
            .quantity(0.001)
            .timeInForce(TimeInForce::GTC)
            .build();

        rest_client.sendOrder(ask);
    }
};
```

---

## 🏗️ Architecture

```
tradeconnector/
├── src/
│   ├── configs.h          # Exchange endpoint configurations
│   ├── types.h            # Core type definitions
│   ├── rest/
│   │   ├── client.h       # REST API client (template-based)
│   │   └── orderbuilder.h # Fluent order builder
│   ├── websocket/
│   │   └── client.h       # IXWebSocket implementation
│   └── utils/
│       └── utils.h        # HMAC signing, timestamps, formatting
└── README.md
```

### Header-Only Design

All implementation is contained in headers with `inline` functions and templates. Simply include the headers you need—no linking required (except for system dependencies like OpenSSL, Boost, and IXWebSocket).

**Required Dependencies:**
- OpenSSL (for HMAC signing and HTTPS)
- Boost.Beast (for HTTP/HTTPS client)
- Boost.Asio (for async I/O)
- IXWebSocket (for WebSocket connections)

---

## 🔌 Supported Exchanges

| Exchange | Spot | Futures | Status |
|----------|------|---------|--------|
| Binance | ✅ | ✅ | Ready |
| Coinbase | 🚧 | ❌ | Planned |
| Kraken | 🚧 | ❌ | Planned |
| OKX | 🚧 | 🚧 | Planned |

*Want to add your favorite exchange? Contributions welcome!*

---

## 📚 Documentation

### API Reference

#### REST Client
- `getAccountInfo()` - Retrieve account balances and info
- `sendOrder(params)` - Place new order
- `getOrderBook(symbol, limit)` - Get order book depth
- `getRecentTrades(symbol, limit)` - Get recent trades
- `createListenKey()` - Create WebSocket listen key
- **Futures Only:**
  - `setLeverage(symbol, leverage)` - Adjust leverage
  - `setMarginType(symbol, type)` - Set margin mode
  - `getOpenPositions()` - Get current positions

#### WebSocket Client
- `connectEndpoint(callback, host, path)` - Subscribe to stream
- `send(endpoint, message)` - Send message to endpoint
- `disconnect(endpoint)` - Close specific connection
- `wait()` - Block until all connections close

#### Order Builder
```cpp
OrderBuilder<MarketType>()
    .symbol(string)              // Trading pair
    .side(Side)                  // Side::BUY or Side::SELL
    .type(OrderType)             // OrderType::LIMIT, OrderType::MARKET, etc.
    .price(double)               // Limit price
    .quantity(double)            // Amount to trade
    .timeInForce(TimeInForce)    // TimeInForce::GTC, TimeInForce::IOC, TimeInForce::FOK
    .quoteOrderQty(double)       // SPOT only: quote quantity
    .reduceOnly(bool)            // FUTURES only
    .positionSide(Side)          // FUTURES only: Side::LONG or Side::SHORT
    .build()                  // Construct OrderParams
```

---

## 🛠️ Advanced Configuration

### Custom Exchange Endpoints
```cpp
// Create custom configuration
template<>
struct CustomConfig<MarketType::SPOT> {
    static constexpr const char* url = "api.myexchange.com";
    static constexpr const char* order = "/api/v1/order";
    // ... define other endpoints
};

// Use custom config
Client<CustomConfig, MarketType::SPOT> client(api_key, secret);
```

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
