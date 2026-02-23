#include "portfolio/PositionManager.hpp"
#include "common/Logger.hpp"
#include <sstream>
#include <iomanip>
#include <numeric>

namespace trading {

PositionManager& PositionManager::instance() {
    static PositionManager inst;
    return inst;
}

void PositionManager::init(double initial_cash) {
    std::lock_guard<std::mutex> lock(mutex_);
    cash_ = initial_cash;
    LOG_INFO("PositionManager init: cash = $" + std::to_string(initial_cash));
}

void PositionManager::record_trade(const Trade& trade, OrderSide side) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& pos = positions_[trade.symbol];
    pos.symbol = trade.symbol;

    double cost = trade.price * trade.quantity;

    if (side == OrderSide::BUY) {
        // Weighted average cost basis
        double total_cost = pos.avg_cost * pos.quantity + cost;
        pos.quantity += trade.quantity;
        pos.avg_cost  = (pos.quantity > 1e-9) ? total_cost / pos.quantity : 0.0;
        cash_        -= cost;
    } else {
        // Realize PnL on SELL
        double realized = (trade.price - pos.avg_cost) * trade.quantity;
        pos.realized_pnl += realized;
        realized_pnl_    += realized;
        pos.quantity     -= trade.quantity;
        cash_            += cost;

        if (pos.quantity < 1e-9) {
            pos.quantity = 0.0;
            pos.avg_cost = 0.0;
        }

        std::ostringstream oss;
        oss << "PnL realized " << trade.symbol
            << ": " << (realized >= 0 ? "+" : "") << std::fixed << std::setprecision(2)
            << realized << " USD";
        LOG_INFO(oss.str());
    }

    pos.last_price = trade.price;
}

void PositionManager::update_mark(const std::string& symbol, double current_price) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = positions_.find(symbol);
    if (it != positions_.end())
        it->second.last_price = current_price;
}

Position PositionManager::get_position(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = positions_.find(symbol);
    if (it != positions_.end()) return it->second;
    Position p; p.symbol = symbol; return p;
}

std::vector<Position> PositionManager::all_positions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Position> result;
    for (const auto& [sym, pos] : positions_)
        if (pos.quantity > 1e-9)
            result.push_back(pos);
    return result;
}

double PositionManager::realized_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return realized_pnl_;
}

double PositionManager::unrealized_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double total = 0.0;
    for (const auto& [sym, pos] : positions_)
        total += pos.unrealized_pnl();
    return total;
}

double PositionManager::total_pnl() const {
    return realized_pnl() + unrealized_pnl();
}

double PositionManager::cash() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cash_;
}

double PositionManager::portfolio_value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double equity = 0.0;
    for (const auto& [sym, pos] : positions_)
        equity += pos.quantity * pos.last_price;
    return cash_ + equity;
}

} // namespace trading
