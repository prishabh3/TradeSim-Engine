#include "strategy/TradingStrategy.hpp"
#include "common/Config.hpp"
#include "common/Logger.hpp"
#include <sstream>
#include <iomanip>

namespace trading {

TradingStrategy::TradingStrategy(ThreadSafeQueue<Order>& order_queue)
    : order_queue_(order_queue)
{
    auto& cfg      = Config::instance();
    rsi_oversold_  = cfg.rsi_oversold();
    rsi_overbought_ = cfg.rsi_overbought();
    order_qty_     = cfg.order_quantity();
    enabled_       = cfg.strategy_enabled();
}

void TradingStrategy::run(ThreadSafeQueue<MarketTick>& market_queue,
                          const std::atomic<bool>& running) {
    LOG_INFO("TradingStrategy started (RSI mean-reversion)");
    while (running) {
        auto opt = market_queue.pop(std::chrono::milliseconds(100));
        if (!opt) continue;
        on_tick(*opt);
    }
    LOG_INFO("TradingStrategy stopped");
}

void TradingStrategy::on_tick(const MarketTick& tick) {
    last_prices_[tick.symbol] = tick.price;

    // Update indicators and get snapshot
    auto snap = IndicatorEngine::instance().update(tick.symbol, tick.price);

    if (!enabled_) return;
    if (!snap.rsi_valid || !snap.ema_valid) return; // wait for warmup

    auto& risk = RiskManager::instance();

    if (should_buy(tick.symbol, snap)) {
        Order order = make_market_order(tick.symbol, OrderSide::BUY, order_qty_);
        if (risk.approve(order, tick.price)) {
            std::ostringstream oss;
            oss << "SIGNAL BUY  " << tick.symbol
                << " RSI=" << std::fixed << std::setprecision(1) << snap.rsi
                << " EMA=" << std::setprecision(4) << snap.ema
                << " px="  << tick.price;
            LOG_INFO(oss.str());
            order_queue_.push(std::move(order));
            last_order_side_[tick.symbol] = OrderSide::BUY;
        }
    } else if (should_sell(tick.symbol, snap)) {
        Order order = make_market_order(tick.symbol, OrderSide::SELL, order_qty_);
        if (risk.approve(order, tick.price)) {
            std::ostringstream oss;
            oss << "SIGNAL SELL " << tick.symbol
                << " RSI=" << std::fixed << std::setprecision(1) << snap.rsi
                << " EMA=" << std::setprecision(4) << snap.ema
                << " px="  << tick.price;
            LOG_INFO(oss.str());
            order_queue_.push(std::move(order));
            last_order_side_[tick.symbol] = OrderSide::SELL;
        }
    }
}

bool TradingStrategy::should_buy(const std::string& symbol,
                                  const IndicatorSnapshot& snap) const {
    // Avoid repeated signals in same direction
    auto it = last_order_side_.find(symbol);
    if (it != last_order_side_.end() && it->second == OrderSide::BUY) return false;

    return snap.rsi < rsi_oversold_; // oversold, RSI < 30
}

bool TradingStrategy::should_sell(const std::string& symbol,
                                   const IndicatorSnapshot& snap) const {
    auto it = last_order_side_.find(symbol);
    if (it != last_order_side_.end() && it->second == OrderSide::SELL) return false;

    return snap.rsi > rsi_overbought_; // overbought, RSI > 70
}

Order TradingStrategy::make_market_order(const std::string& symbol,
                                          OrderSide side, double qty) const {
    Order o;
    o.symbol   = symbol;
    o.type     = OrderType::MARKET;
    o.side     = side;
    o.quantity = qty;
    o.price    = 0.0;
    o.status   = OrderStatus::PENDING;
    o.timestamp = std::chrono::steady_clock::now();
    return o;
}

} // namespace trading
