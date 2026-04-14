# TradeConnector (Work in progress)

> Modern C++20 trading library for cryptocurrency exchanges

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A type-safe static library with REST ([Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/)) and WebSocket ([IXWebSocket](https://github.com/machinezone/IXWebSocket)) clients for cryptocurrency exchanges. Uses a policy-based design with CRTP and C++20 concepts for compile-time market validation. Currently supports Binance (spot + futures) with experimental Kraken configs.

---

## Features

- **Compile-time market constraints** — futures-only methods like `setLeverage()` won't compile on spot clients
- **Policy-based design** — exchange logic is encapsulated in policy classes (e.g. `BinancePolicy<MarketType::SPOT>`)
- **Mixin extensions** — extend clients with custom functionality via variadic template template parameters
- **Persistent HTTPS** — keep-alive connections with automatic reconnection (PIMPL hides Boost internals)
- **Multi-endpoint WebSocket** — concurrent connections with per-endpoint callbacks

```cpp
using namespace trade_connector;

// Compile-time market validation — hosts are configured on the policy
SyncClient<BinancePolicy<MarketType::FUTURES>> futures(api_key, secret);
futures.setLeverage(TokenPair::BTCUSDT, 10);  // OK — futures only

SyncClient<BinancePolicy<MarketType::SPOT>> spot(api_key, secret);
// spot.setLeverage(...)  // Won't compile — not available for SPOT
```

---

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt install libssl-dev libboost-all-dev zlib1g-dev cmake g++
```

simdjson and IXWebSocket are fetched automatically via CMake FetchContent.

## Building

```bash
git clone https://github.com/slaymuel/tradeconnector.git
cd tradeconnector
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Installation

```bash
sudo cmake --install build
# or install to a local prefix:
cmake --install build --prefix ~/.local
```

Downstream projects can then use:

```cmake
find_package(trade_connector REQUIRED)
target_link_libraries(my_app PRIVATE trade_connector::trade_connector)
```

## Build options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_LTO` | OFF | Enable link-time optimization (requires matching compiler toolchain) |
| `TC_BUILD_EXAMPLES` | OFF | Build example programs |
| `TC_BUILD_BENCHMARKS` | OFF | Build benchmarks (requires Google Benchmark) |

---

## Quick Start

### Placing an order (Spot)

```cpp
#include <clients/syncclient.h>
#include <clients/binancepolicy.h>

using namespace trade_connector;

int main() {
    // Hosts are defined on the policy (BinancePolicy::BINANCE_REST_HOST, etc.)
    SyncClient<BinancePolicy<MarketType::SPOT>> client(
        "your_api_key",
        "your_secret_key"
    );

    OrderParams<MarketType::SPOT> order{
        .symbol    = TokenPair::BTCUSDT,
        .side      = Side::BUY,
        .type      = OrderType::LIMIT,
        .price     = 50000.0,
        .quantity  = 0.001,
    };

    auto response = client.sendOrder(order);
    std::cout << response << std::endl;
}
```

### Streaming trades via WebSocket

```cpp
#include <clients/syncclient.h>
#include <clients/binancepolicy.h>

using namespace trade_connector;

int main() {
    SyncClient<BinancePolicy<MarketType::SPOT>> client;

    client.connectTradeFeed(
        {TokenPair::BTCUSDT, TokenPair::ETHUSDT},
        +[](std::string_view msg) {
            std::cout << msg << std::endl;
        }
    );

    client.websocketClient().wait();
}
```

### Futures with leverage

```cpp
SyncClient<BinancePolicy<MarketType::FUTURES>> client(api_key, secret_key);

client.setLeverage(TokenPair::BTCUSDT, 10);
client.setMarginType(TokenPair::BTCUSDT, "ISOLATED");

OrderParams<MarketType::FUTURES> order{
    .symbol        = TokenPair::BTCUSDT,
    .side          = Side::BUY,
    .type          = OrderType::MARKET,
    .quantity      = 0.1,
    .position_side = Side::LONG,
};

client.sendOrder(order);
```

### Extending with mixins

```cpp
// A mixin that adds a rate limiter to order placement
template<class Derived>
struct RateLimiter {
    std::string sendOrderThrottled(const auto& params, 
                                   std::chrono::milliseconds min_interval 
                                       = std::chrono::milliseconds(100)) {
        auto& self = static_cast<Derived&>(*this);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_order_time_
        );

        if (elapsed < min_interval) {
            self.logger("Order throttled — " 
                + std::to_string((min_interval - elapsed).count()) + "ms remaining");
            return "";
        }

        last_order_time_ = now;
        return self.sendOrder(params);
    }

private:
    std::chrono::steady_clock::time_point last_order_time_{};
};

// A mixin that tracks P&L across fills
template<class Derived>
struct PnLTracker {
    void recordFill(Side side, double price, double qty) {
        double signed_qty = (side == Side::BUY) ? qty : -qty;
        position_ += signed_qty;
        realized_pnl_ -= signed_qty * price;  // cash flow
    }

    double unrealizedPnL(double mark_price) const {
        return realized_pnl_ + position_ * mark_price;
    }

    double position() const { return position_; }

private:
    double position_ = 0.0;
    double realized_pnl_ = 0.0;
};

// Compose multiple mixins at compile time
SyncClient<BinancePolicy<MarketType::SPOT>, RateLimiter, PnLTracker> client(
    api_key, secret
);

// Use the rate-limited order method from RateLimiter
OrderParams<MarketType::SPOT> order{ /* ... */ };
client.sendOrderThrottled(order, std::chrono::milliseconds(200));

// Track fills from PnLTracker
client.recordFill(Side::BUY, 67500.0, 0.01);
std::cout << "Unrealized P&L: " << client.unrealizedPnL(68000.0) << std::endl;
```

---

## Architecture

```
src/
├── configs.h                  # Compile-time exchange endpoint configs (Binance, Kraken)
├── types.h                    # MarketType, Side, OrderType, OrderParams, TokenPair, Error
├── clients/
│   ├── baseclient.h           # CRTP base — REST, WS, and policy dispatch
│   ├── syncclient.h           # Synchronous client with mixin support
│   ├── asyncclient.h          # Async client (placeholder)
│   └── binancepolicy.h        # Binance exchange policy (REST + WS operations)
├── rest/
│   ├── client.h / client.cpp  # HTTPS REST client (Boost.Beast, PIMPL)
│   └── orderbuilder.h         # (legacy) Order builder
├── websocket/
│   └── client.h / client.cpp  # Multi-endpoint WebSocket client (IXWebSocket)
└── utils/
    ├── signing.h / signing.cpp # HMAC-SHA256 signing
    └── utils.h                 # Timestamps, string helpers, null logger
```

### Design

- **`BaseClient<Derived, Config, M>`** — CRTP base that owns a `rest::Client` and `websocket::Client`. Host endpoints are obtained from the `Config` policy at construction time. Dispatches trading operations to the `Config` policy using `if constexpr(requires(...))`.
- **`SyncClient<Config, Extensions...>`** — concrete leaf that inherits `BaseClient` and zero or more mixin extensions via variadic template template parameters. Constructor takes only API key, secret key, and an optional logger.
- **`BinancePolicy<M>`** — policy implementing Binance REST endpoints (order placement, account info, listen keys) and WebSocket streams (user data, depth, aggTrade). Stores host configuration as `static constexpr` members. Market-specific methods are gated with `requires(IsFutures<M>)` / `requires(IsSpot<M>)`.
- **`rest::Client`** — synchronous HTTPS client using Boost.Beast over OpenSSL with PIMPL to hide Boost headers from consumers.
- **`websocket::Client`** — manages multiple concurrent WebSocket connections with per-endpoint function-pointer callbacks.

---

## API Reference

### BaseClient / SyncClient

| Method | Description |
|--------|-------------|
| `sendOrder(params, error)` | Place an order |
| `getAccountInfo()` | Account balances and info |
| `getOpenOrders()` | All open orders |
| `cancelAllOpenOrders()` | Cancel all open orders |
| `connectTradeFeed(tokens, callback)` | Subscribe to aggregated trades |
| `connectDepthFeed(tokens, callback)` | Subscribe to order book depth |
| `connectUserDataFeed(callback, ...)` | Authenticated user data stream |
| **Futures only** | |
| `setLeverage(symbol, n)` | Adjust leverage (1-125x) |
| `setMarginType(symbol, type)` | Set ISOLATED or CROSSED margin |
| `getOpenPositions()` | Current positions and risk |

### BinancePolicy (additional)

| Method | Description |
|--------|-------------|
| `createListenKey(host)` | Create user data stream listen key |
| `keepAliveListenKey(host, key)` | Extend listen key validity |
| `closeListenKey(host, key)` | Close listen key |
| `ping(host)` | Test connectivity |
| `getServerTime(host)` | Exchange server time |
| `getExchangeInfo(host)` | Trading rules and symbol info |
| `getOrderBook(host, symbol, limit)` | Order book snapshot |
| `getRecentTrades(host, symbol, limit)` | Recent trade list |

---

## Supported Exchanges

| Exchange | Spot | Futures | Status |
|----------|------|---------|--------|
| Binance  | Yes  | Yes     | Active |
| Kraken   | Config only | —  | Experimental |

---

## Dependencies

| Library | Purpose | Linking |
|---------|---------|---------|
| [simdjson](https://github.com/simdjson/simdjson) | JSON parsing (exposed in policy headers) | PUBLIC |
| [Boost.Beast/Asio](https://www.boost.org/) | HTTP/HTTPS REST client | PRIVATE (hidden by PIMPL) |
| [OpenSSL](https://www.openssl.org/) | TLS and HMAC-SHA256 signing | PRIVATE |
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | WebSocket connections | PRIVATE |
| [zlib](https://zlib.net/) | Compression (IXWebSocket dependency) | PRIVATE |

simdjson and IXWebSocket are fetched via CMake FetchContent. Boost, OpenSSL, and zlib must be installed on the system.

---

## License

MIT — see [LICENSE](LICENSE) for details.
