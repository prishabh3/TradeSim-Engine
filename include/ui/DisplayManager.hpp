#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include "common/Types.hpp"

namespace trading {

/**
 * DisplayManager — ANSI terminal UI that refreshes every 500ms.
 *
 * Renders:
 *   ┌ Header bar (system status, time, cash, portfolio value)
 *   ├ Order book (top 5 bids/asks per symbol — rotating)
 *   ├ Positions & PnL table
 *   ├ Indicators table (SMA/EMA/RSI per symbol)
 *   └ Recent trades feed
 *
 * Thread-safe: all mutable state protected by mutex_.
 * Runs on its own dedicated UI thread.
 */
class DisplayManager {
public:
    static DisplayManager& instance();

    void init(int refresh_ms, int max_trades);

    // Called from PnL/engine threads to deliver data for display
    void on_trade(const Trade& trade);
    void on_tick (const MarketTick& tick);

    // Snapshot for API server
    std::vector<Trade> recent_trades() const;

    // Main UI loop — blocks until stop() is called
    void run(const std::atomic<bool>& running);
    void stop();

private:
    DisplayManager() = default;
    DisplayManager(const DisplayManager&) = delete;

    void render();
    void render_header    (const std::string& now_str);
    void render_order_book();
    void render_positions ();
    void render_indicators();
    void render_trades    ();

    // ── ANSI helpers ──────────────────────────────────────────────────────────
    static std::string green (const std::string& s);
    static std::string red   (const std::string& s);
    static std::string yellow(const std::string& s);
    static std::string bold  (const std::string& s);
    static std::string cyan  (const std::string& s);
    static std::string dim   (const std::string& s);

    static void clear_screen();
    static void move_cursor_home();

    // ── State (protected by mutex_) ───────────────────────────────────────────
    mutable std::mutex     mutex_;
    std::deque<Trade>      recent_trades_;
    std::vector<MarketTick> latest_ticks_; // current_price per symbol

    int refresh_ms_ = 500;
    int max_trades_ = 10;

    // Which symbol to show order book for (rotates)
    std::size_t book_symbol_idx_ = 0;
};

} // namespace trading
