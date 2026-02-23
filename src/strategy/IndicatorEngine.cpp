#include "strategy/IndicatorEngine.hpp"
#include <stdexcept>
#include <cmath>

namespace trading {

IndicatorEngine& IndicatorEngine::instance() {
    static IndicatorEngine inst;
    return inst;
}

void IndicatorEngine::init(int sma_period, int ema_period, int rsi_period) {
    sma_period_ = sma_period;
    ema_period_ = ema_period;
    rsi_period_ = rsi_period;
    ema_k_      = 2.0 / (ema_period + 1);
}

IndicatorSnapshot IndicatorEngine::update(const std::string& symbol, double price) {
    auto& s = states_[symbol];
    s.last.symbol = symbol;
    s.last.price  = price;   // always record latest price

    // ── SMA ──────────────────────────────────────────────────────────────────
    double sma = compute_sma(s, price);
    s.last.sma       = sma;
    s.last.sma_valid = (static_cast<int>(s.window.size()) == sma_period_);

    // ── EMA ──────────────────────────────────────────────────────────────────
    double ema = compute_ema(s, price);
    s.last.ema       = ema;
    s.last.ema_valid = s.ema_initialized;

    // ── RSI ──────────────────────────────────────────────────────────────────
    double rsi = compute_rsi(s, price);
    s.last.rsi       = rsi;
    s.last.rsi_valid = (s.rsi_count >= rsi_period_);

    return s.last;
}

IndicatorSnapshot IndicatorEngine::snapshot(const std::string& symbol) const {
    auto it = states_.find(symbol);
    if (it == states_.end()) {
        IndicatorSnapshot empty;
        empty.symbol = symbol;
        return empty;
    }
    return it->second.last;
}

// ── Private helpers ───────────────────────────────────────────────────────────

double IndicatorEngine::compute_sma(SymbolState& s, double price) const {
    s.window.push_back(price);
    s.running_sum += price;

    if (static_cast<int>(s.window.size()) > sma_period_) {
        s.running_sum -= s.window.front();
        s.window.pop_front();
    }

    if (static_cast<int>(s.window.size()) < sma_period_) return 0.0;
    return s.running_sum / sma_period_;
}

double IndicatorEngine::compute_ema(SymbolState& s, double price) const {
    if (!s.ema_initialized) {
        // Seed EMA with first SMA value after warmup
        if (static_cast<int>(s.window.size()) == sma_period_) {
            s.ema_value      = s.running_sum / sma_period_;
            s.ema_initialized = true;
        }
        return s.ema_value;
    }
    // EMA_t = price * k + EMA_{t-1} * (1 - k)
    s.ema_value = price * ema_k_ + s.ema_value * (1.0 - ema_k_);
    return s.ema_value;
}

double IndicatorEngine::compute_rsi(SymbolState& s, double price) const {
    if (!s.prev_price.has_value()) {
        s.prev_price = price;
        return 50.0; // neutral until we have a delta
    }

    double change = price - *s.prev_price;
    double gain   = (change > 0) ? change : 0.0;
    double loss   = (change < 0) ? -change : 0.0;
    s.prev_price  = price;

    if (s.rsi_count < rsi_period_) {
        s.avg_gain += gain;
        s.avg_loss += loss;
        ++s.rsi_count;

        if (s.rsi_count == rsi_period_) {
            s.avg_gain /= rsi_period_;
            s.avg_loss /= rsi_period_;
        }
        return 50.0; // not ready yet
    }

    // Wilder smoothing: avg = (prev * (n-1) + current) / n
    s.avg_gain = (s.avg_gain * (rsi_period_ - 1) + gain) / rsi_period_;
    s.avg_loss = (s.avg_loss * (rsi_period_ - 1) + loss) / rsi_period_;

    if (s.avg_loss < 1e-10) return 100.0;
    double rs = s.avg_gain / s.avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

} // namespace trading
