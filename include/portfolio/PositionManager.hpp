#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "common/Types.hpp"

namespace trading {

/**
 * PositionManager — tracks open positions and PnL.
 *
 * Thread-safe: called from both the PnL thread (record_trade)
 * and the UI thread (snapshot). Uses a read-write pattern via shared_mutex.
 */
class PositionManager {
public:
    static PositionManager& instance();

    void init(double initial_cash);

    // Called when a trade is filled
    void record_trade(const Trade& trade, OrderSide side);

    // Called on each market tick to update unrealized PnL
    void update_mark(const std::string& symbol, double current_price);

    // Read-only snapshots for UI
    Position              get_position(const std::string& symbol) const;
    std::vector<Position> all_positions() const;

    double realized_pnl()   const;
    double unrealized_pnl() const;
    double total_pnl()      const;
    double cash()           const;
    double portfolio_value() const;

private:
    PositionManager() = default;
    PositionManager(const PositionManager&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Position> positions_;
    double cash_          = 100000.0;
    double realized_pnl_  = 0.0;
};

} // namespace trading
