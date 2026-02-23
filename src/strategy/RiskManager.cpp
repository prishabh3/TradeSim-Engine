#include "strategy/RiskManager.hpp"
#include "common/Logger.hpp"
#include <sstream>
#include <iomanip>

namespace trading {

RiskManager& RiskManager::instance() {
    static RiskManager inst;
    return inst;
}

void RiskManager::init(const RiskConfig& cfg) {
    cfg_ = cfg;
}

bool RiskManager::approve(const Order& order, double current_price) const {
    double notional = order.quantity * current_price;

    // 1. Per-trade loss guard
    if (notional > cfg_.max_loss_per_trade_usd * 20) {
        LOG_WARN("RISK REJECTED [trade too large]: " + order.symbol +
                 " notional=" + std::to_string(notional));
        return false;
    }

    // 2. Per-symbol exposure
    double sym_exp = get_exposure(order.symbol);
    if (order.side == OrderSide::BUY) {
        if (sym_exp + notional > cfg_.max_position_usd) {
            LOG_WARN("RISK REJECTED [symbol exposure]: " + order.symbol +
                     " current=" + std::to_string(sym_exp) +
                     " new=" + std::to_string(notional));
            return false;
        }
    }

    // 3. Total portfolio exposure
    double total = total_exposure() + notional;
    if (total > cfg_.max_total_exposure_usd) {
        LOG_WARN("RISK REJECTED [portfolio exposure]: total=" + std::to_string(total));
        return false;
    }

    return true;
}

void RiskManager::record_fill(const Trade& trade, OrderSide side) {
    double notional = trade.quantity * trade.price;
    if (side == OrderSide::BUY)
        exposure_[trade.symbol] += notional;
    else
        exposure_[trade.symbol] -= notional;

    // Clamp to 0 (can't have negative exposure)
    if (exposure_[trade.symbol] < 0.0)
        exposure_[trade.symbol] = 0.0;
}

double RiskManager::get_exposure(const std::string& symbol) const {
    auto it = exposure_.find(symbol);
    return (it != exposure_.end()) ? it->second : 0.0;
}

double RiskManager::total_exposure() const {
    double total = 0.0;
    for (const auto& [sym, val] : exposure_) total += val;
    return total;
}

} // namespace trading
