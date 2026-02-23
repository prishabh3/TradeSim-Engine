#pragma once

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include "common/Types.hpp"

namespace trading {

/**
 * Per-symbol indicator state machine.
 * All computations are O(1) after warmup — no full-window recalculation.
 *
 * Usage:
 *   auto& ie = IndicatorEngine::instance();
 *   ie.update("AAPL", 175.5);
 *   auto snap = ie.snapshot("AAPL");
 */
class IndicatorEngine {
public:
    static IndicatorEngine& instance();

    void init(int sma_period, int ema_period, int rsi_period);

    // Feed a new price for a symbol; returns updated snapshot
    IndicatorSnapshot update(const std::string& symbol, double price);

    // Get the latest snapshot (without feeding new price)
    IndicatorSnapshot snapshot(const std::string& symbol) const;

private:
    IndicatorEngine() = default;
    IndicatorEngine(const IndicatorEngine&) = delete;

    struct SymbolState {
        // SMA sliding window
        std::deque<double> window;
        double             running_sum = 0.0;

        // EMA
        double ema_value       = 0.0;
        bool   ema_initialized = false;

        // RSI (Wilder smoothing)
        std::optional<double> prev_price;
        double avg_gain  = 0.0;
        double avg_loss  = 0.0;
        int    rsi_count = 0;

        // Cached last snapshot
        IndicatorSnapshot last;
    };

    double compute_sma(SymbolState& s, double price) const;
    double compute_ema(SymbolState& s, double price) const;
    double compute_rsi(SymbolState& s, double price) const;

    std::unordered_map<std::string, SymbolState> states_;
    int sma_period_ = 14;
    int ema_period_ = 14;
    int rsi_period_ = 14;
    double ema_k_   = 2.0 / (14 + 1); // EMA smoothing factor
};

} // namespace trading
