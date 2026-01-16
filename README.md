# 🚀 TradeConnector

> **Modern C++20 high-performance trading library for cryptocurrency exchanges**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

A performant and type-safe library including REST and websocket clients for communicating with cryptocurrency exchanges. TradeConnector is designed for algorithmic trading and focuses on performance and usability. Currently has support for the Binance and experimental support for Kraken and Coinbase.

---

## ✨ Key Features

### 🎯 **Type Safety with Zero Runtime Cost**
```cpp
// Compile-time market validation using C++20 concepts
Client<BinanceConfig, MarketType::SPOT> spot_client(api_key, secret);
Client<BinanceConfig, MarketType::FUTURES> futures_client(api_key, secret);

spot_client.setLeverage(...);  // ❌ Compile error - not available for SPOT
futures_client.setLeverage("BTCUSDT", 10);  // ✅ Compiles - futures only
```

### ⚡ **Extreme Performance**
- **Zero-copy WebSocket message delivery** using `std::string_view`
- **Persistent HTTPS connections** with HTTP keep-alive
- **No virtual functions** - templates enable zero-overhead abstraction
- **Stack-based number formatting** with `std::to_chars` (no heap allocations)
- **Compile-time configuration** via `constexpr`

### 🔧 **WebSocket Support**
- **IXWebSocket**: Easy to use, multi-threaded WebSocket client
- Zero-copy message delivery with `std::string_view` callbacks
- Automatic reconnection and connection management

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
        .side("BUY")
        .type("LIMIT")
        .price(50000.0)
        .quantity(0.001)
        .timeInForce("GTC")
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

using namespace trade_connector::websocket;

int main() {
    Client ws_client;
    
    // Subscribe to real-time trades (zero-copy callback)
    ws_client.connectEndpoint(
        +[](std::string_view msg) {
            simdjson::ondemand::parser parser;
            auto doc = parser.iterate(msg);
            
            auto price = doc["p"].get_double();
            auto qty = doc["q"].get_double();
            
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

using namespace trade_connector;

int main() {
    rest::Client<BinanceConfig, MarketType::FUTURES> client(api_key, secret);
    
    // Set leverage to 10x
    client.setLeverage("BTCUSDT", 10);
    
    // Set isolated margin mode
    client.setMarginType("BTCUSDT", "ISOLATED");
    
    // Place long position
    auto order = rest::OrderBuilder<MarketType::FUTURES>()
        .symbol("BTCUSDT")
        .side("BUY")
        .type("MARKET")
        .quantity(0.1)
        .positionSide("LONG")
        .build();
    
    auto response = client.sendOrder(order);
    std::cout << response << std::endl;
    
    return 0;
}
```

### Market Making Strategy
```cpp
#include "tradeconnector/src/rest/client.h"
#include "tradeconnector/src/websocket/client.h"

class MarketMaker {
    rest::Client<MarketType::SPOT>& rest_client;
    websocket::Client ws_client;
    double best_bid = 0, best_ask = 0;
    
public:
    MarketMaker(rest::Client<MarketType::SPOT>& client) 
        : rest_client(client) {}
    
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
        // Parse order book
        // Update best_bid and best_ask
        // Place orders with spread
        placeOrders();
    }
    
    void placeOrders() {
        // Place bid slightly below best bid
        auto bid = rest::OrderBuilder<MarketType::SPOT>()
            .symbol("BTCUSDT")
            .side("BUY")
            .type("LIMIT")
            .price(best_bid - 0.01)
            .quantity(0.001)
            .timeInForce("GTC")
            .build();
        
        rest_client.sendOrder(bid);
        
        // Place ask slightly above best ask
        auto ask = rest::OrderBuilder<MarketType::SPOT>()
            .symbol("BTCUSDT")
            .side("SELL")
            .type("LIMIT")
            .price(best_ask + 0.01)
            .quantity(0.001)
            .timeInForce("GTC")
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

### Design Principles

1. **Zero-Cost Abstractions**: Templates and concepts provide type safety without runtime overhead
2. **Compile-Time Configuration**: Exchange endpoints are `constexpr` for zero runtime cost
3. **Resource Safety**: RAII ensures proper cleanup of connections and resources

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
    .symbol(string)           // Trading pair
    .side(string)             // "BUY" or "SELL"
    .type(string)             // "LIMIT", "MARKET", etc.
    .price(double)            // Limit price
    .quantity(double)         // Amount to trade
    .timeInForce(string)      // "GTC", "IOC", "FOK"
    .quoteOrderQty(double)    // SPOT only: quote quantity
    .reduceOnly(bool)         // FUTURES only
    .positionSide(string)     // FUTURES only: "LONG"/"SHORT"
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

---

## ⚠️ Disclaimer

**This software is for educational and research purposes. Trading cryptocurrency carries significant risk.**

- Always test with testnet/paper trading first
- Never trade with more than you can afford to lose
- Past performance does not indicate future results
- The authors are not responsible for any financial losses

---

## 🙏 Acknowledgments

- [Binance](https://www.binance.com/) for comprehensive API documentation
- [simdjson](https://github.com/simdjson/simdjson) for blazingly fast JSON parsing
- [IXWebSocket](https://github.com/machinezone/IXWebSocket) for reliable WebSocket implementation
- [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/) for HTTP/HTTPS support

---

<div align="center">

**⭐ Star this repo if you find it useful!**

[Documentation](docs/) · [Examples](examples/) · [Changelog](CHANGELOG.md)

</div>
