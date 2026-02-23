#pragma once

#include <string>
#include <unordered_map>
#include "common/Types.hpp"

namespace trading {

struct RiskConfig {
    double max_position_usd      = 10000.0;
    double max_total_exposure_usd = 50000.0;
    double max_loss_per_trade_usd = 500.0;
    double stop_loss_pct         = 0.02;
};

/**
 * RiskManager — stateful gate that approves/rejects orders.
 *
 * Checks:
 *   1. Per-symbol notional exposure
 *   2. Total portfolio notional
 *   3. Per-trade loss budget
 *
 * Thread note: called only from the strategy/process thread.
 * record_fill() must be called on every trade to keep exposure accurate.
 */
class RiskManager {
public:
    static RiskManager& instance();

    void init(const RiskConfig& cfg);

    // Returns true if order passes all risk checks
    bool approve(const Order& order, double current_price) const;

    // Update internal exposure tracking after a fill
    void record_fill(const Trade& trade, OrderSide side);

    double get_exposure(const std::string& symbol) const;
    double total_exposure() const;

    const RiskConfig& config() const { return cfg_; }

private:
    RiskManager() = default;
    RiskManager(const RiskManager&) = delete;

    RiskConfig cfg_;
    std::unordered_map<std::string, double> exposure_; // symbol → net USD exposure
};

} // namespace trading
