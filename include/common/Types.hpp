#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace trading {

using Timestamp = std::chrono::steady_clock::time_point;

// ─── Market data ────────────────────────────────────────────────────────────
struct MarketTick {
    std::string symbol;
    double      price  = 0.0;
    double      volume = 0.0;
    double      bid    = 0.0;
    double      ask    = 0.0;
    Timestamp   time   = std::chrono::steady_clock::now();
};

// ─── Order ───────────────────────────────────────────────────────────────────
enum class OrderType   { MARKET, LIMIT };
enum class OrderSide   { BUY, SELL };
enum class OrderStatus { PENDING, PARTIAL, FILLED, CANCELLED, REJECTED };

struct Order {
    uint64_t    id          = 0;
    std::string symbol;
    OrderType   type        = OrderType::LIMIT;
    OrderSide   side        = OrderSide::BUY;
    double      price       = 0.0;      // Ignored for MARKET orders
    double      quantity    = 0.0;
    double      filled_qty  = 0.0;
    OrderStatus status      = OrderStatus::PENDING;
    Timestamp   timestamp   = std::chrono::steady_clock::now();

    double remaining() const { return quantity - filled_qty; }
    bool   is_complete() const { return filled_qty >= quantity - 1e-9; }
};

// ─── Trade ───────────────────────────────────────────────────────────────────
struct Trade {
    uint64_t    buy_order_id  = 0;
    uint64_t    sell_order_id = 0;
    std::string symbol;
    double      price         = 0.0;
    double      quantity      = 0.0;
    Timestamp   time          = std::chrono::steady_clock::now();
};

// ─── Position ─────────────────────────────────────────────────────────────────
struct Position {
    std::string symbol;
    double      quantity     = 0.0;
    double      avg_cost     = 0.0;
    double      realized_pnl = 0.0;
    double      last_price   = 0.0;

    double unrealized_pnl() const { return (last_price - avg_cost) * quantity; }
    double total_pnl()      const { return realized_pnl + unrealized_pnl(); }
};

// ─── Indicator snapshot ────────────────────────────────────────────────────
struct IndicatorSnapshot {
    std::string symbol;
    double price  = 0.0;   // last price fed
    double sma    = 0.0;
    double ema    = 0.0;
    double rsi    = 0.0;
    bool   sma_valid = false;
    bool   ema_valid = false;
    bool   rsi_valid = false;
};

} // namespace trading
