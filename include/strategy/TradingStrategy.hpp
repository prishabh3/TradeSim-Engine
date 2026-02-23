#pragma once

#include <string>
#include <unordered_map>
#include <atomic>
#include "common/Types.hpp"
#include "common/ThreadSafeQueue.hpp"
#include "strategy/IndicatorEngine.hpp"
#include "strategy/RiskManager.hpp"

namespace trading {

/**
 * TradingStrategy — RSI mean-reversion + EMA trend filter.
 *
 * Signals:
 *   RSI < 30 AND EMA trending up   → BUY  (oversold reversal)
 *   RSI > 70 AND EMA trending down → SELL (overbought reversal)
 *
 * All generated orders are risk-checked before being pushed to OrderQueue.
 * Runs on the processing thread — consumes MarketTick, produces Order.
 */
class TradingStrategy {
public:
    TradingStrategy(ThreadSafeQueue<Order>& order_queue);

    // Process a market tick — may emit orders
    void on_tick(const MarketTick& tick);

    // Run loop: consume from market_queue and call on_tick()
    void run(ThreadSafeQueue<MarketTick>& market_queue,
             const std::atomic<bool>& running);

private:
    bool should_buy (const std::string& symbol, const IndicatorSnapshot& snap) const;
    bool should_sell(const std::string& symbol, const IndicatorSnapshot& snap) const;

    Order make_market_order(const std::string& symbol, OrderSide side, double qty) const;

    ThreadSafeQueue<Order>&                       order_queue_;
    std::unordered_map<std::string, double>       last_prices_;

    // Cooldown: don't flood orders — track last order direction per symbol
    std::unordered_map<std::string, OrderSide>    last_order_side_;

    double rsi_oversold_  = 30.0;
    double rsi_overbought_ = 70.0;
    double order_qty_     = 10.0;
    bool   enabled_        = true;
};

} // namespace trading
