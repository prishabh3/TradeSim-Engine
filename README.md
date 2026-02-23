# Real-Time Trading Simulation System

A production-style, low-latency trading simulation written in **C++17**. Designed to showcase:
multi-threaded architecture, lock-minimal data structures, order matching, real-time PnL tracking,
technical indicators, and risk management.

---

## Features

| Layer | Components |
|---|---|
| **Market Data** | Brownian-motion simulation + live REST API (Alpha Vantage) via libcurl |
| **Order Engine** | Limit/Market orders, priority-queue order book, partial fills |
| **Strategy** | RSI mean-reversion + EMA trend filter |
| **Indicators** | O(1) SMA, EMA, RSI (Wilder smoothing) |
| **Risk** | Per-symbol & portfolio exposure caps, stop-loss guard |
| **Portfolio** | Weighted avg cost basis, realized + unrealized PnL |
| **UI** | Live ANSI terminal dashboard (refreshes every 500ms) |
| **Logging** | Async background-thread logger, color coded by level |

---

## Architecture

```
MarketDataFetcher  ──[MarketTick queue]──▶  TradingStrategy
      │                                           │
      └── [Mark prices] ──▶ PositionManager       │  [Order queue]
                                                  ▼
                                       OrderMatchingEngine
                                           │
                                    [Trade queue]
                                      │         │
                               PositionManager  DisplayManager
                               RiskManager
```

**6 Threads:**
1. `data_thread` — Fetches / simulates market ticks
2. `fanout_thread` — Fans ticks out to strategy + mark prices
3. `strategy_thread` — RSI signals → order generation
4. `engine_thread` — Matches orders, emits fills
5. `pnl_thread` — Updates PnL + risk tracking
6. `ui_thread` — Renders live terminal UI

---

## Project Structure

```
├── CMakeLists.txt
├── config/
│   └── config.json          ← All parameters (symbols, risk, strategy)
├── include/
│   ├── common/              ← Types, ThreadSafeQueue, Logger, Config
│   ├── engine/              ← Order, OrderBook, OrderMatchingEngine
│   ├── market/              ← MarketDataFetcher
│   ├── portfolio/           ← PositionManager
│   ├── strategy/            ← IndicatorEngine, RiskManager, TradingStrategy
│   └── ui/                  ← DisplayManager
├── src/                     ← Implementation files (mirrors include/)
└── tests/                   ← Unit tests (no external framework)
```

---

## Dependencies

| Library | macOS (Homebrew) | Ubuntu |
|---|---|---|
| **libcurl** | `brew install curl` | `sudo apt-get install libcurl4-openssl-dev` |
| **jsoncpp** | `brew install jsoncpp` | `sudo apt-get install libjsoncpp-dev` |
| **cmake ≥ 3.16** | `brew install cmake` | `sudo apt-get install cmake` |

---

## Build & Run

```bash
# Clone / enter project
cd "Real-Time Stock Trading App"

# Install dependencies (macOS)
brew install cmake curl jsoncpp

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)

# Run (from build directory — config path auto-detected)
./trading_system

# Run with custom config
./trading_system ../config/config.json
```

### Debug build (AddressSanitizer + UBSan enabled)
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.logicalcpu)
```

---

## Running Tests

```bash
cd build
ctest --output-on-failure
# or individually:
./tests/test_thread_safe_queue
./tests/test_indicators
./tests/test_order_book
./tests/test_matching_engine
```

---

## Configuration (`config/config.json`)

| Key | Default | Description |
|---|---|---|
| `market_data.mode` | `"simulation"` | `"simulation"` or `"live"` |
| `market_data.symbols` | `["AAPL","GOOGL",...]` | Symbols to track |
| `indicators.rsi_period` | `14` | RSI lookback period |
| `strategy.rsi_oversold_threshold` | `30` | Buy signal threshold |
| `strategy.rsi_overbought_threshold` | `70` | Sell signal threshold |
| `risk.max_position_usd` | `10000` | Per-symbol cap ($) |
| `risk.max_total_exposure_usd` | `50000` | Portfolio cap ($) |
| `simulation.volatility` | `0.002` | Price volatility per tick |
| `simulation.initial_cash` | `100000` | Starting cash ($) |

### Live API Mode
1. Get a free API key from [Alpha Vantage](https://www.alphavantage.co/)
2. Set `market_data.mode: "live"` and `market_data.api_key: "YOUR_KEY"` in `config.json`

---

## Key C++ Patterns Used

| Pattern | Where |
|---|---|
| `std::priority_queue` with custom comparator | OrderBook bid/ask heaps |
| `std::condition_variable` + `std::mutex` | ThreadSafeQueue blocking pop |
| `std::atomic<bool>` | Shutdown signal (no mutex needed) |
| RAII + `std::unique_lock` | Scoped mutex in PositionManager |
| Smart pointers (`unique_ptr`) | Engine ownership of order books |
| Move semantics (`std::move`) | Zero-copy queue pushes |
| Template classes | `ThreadSafeQueue<T>` |
| Singleton pattern | Logger, Config, IndicatorEngine, RiskManager |
| Geometric Brownian Motion | Realistic price simulation |
| Exponential backoff | cURL retry on API failure |

---

## Live UI Preview

```
╔══════════════════════════════════════════════════════════╗
║      REAL-TIME TRADING SIMULATION SYSTEM  2026-02-23 ... ║
╠══════════════════════════════════════════════════════════╣
║  Portfolio: $101234.56  │  Cash: $92000  │  PnL: +$1234 ║
╚══════════════════════════════════════════════════════════╝

  ORDER BOOK  [AAPL]
  BID QTY    BID        │    ASK       ASK QTY
  ──────────────────────────────────────────────
      50   174.9800  │  175.0200      30
      40   174.9600  │  175.0400      50

  POSITIONS
  SYMBOL    QTY   AVG COST     LAST   UNREAL PnL  REAL PnL
  AAPL       50   174.5000  175.020    +$26.00    +$80.00

  INDICATORS
  SYMBOL    PRICE      SMA       EMA    RSI   SIGNAL
  AAPL    175.0200  174.800  174.850   28.5   ▲ BUY
  GOOGL   141.2000  140.500  140.800   72.1   ▼ SELL
```

---

## License

MIT License — free to use for learning, interviews, and personal projects.
