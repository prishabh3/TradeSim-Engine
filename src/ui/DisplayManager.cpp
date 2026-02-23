#include "ui/DisplayManager.hpp"
#include "engine/OrderMatchingEngine.hpp"
#include "portfolio/PositionManager.hpp"
#include "strategy/IndicatorEngine.hpp"
#include "common/Config.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <algorithm>

// Forward declare the global engine for book access
namespace trading { extern OrderMatchingEngine* g_engine; }

namespace trading {

// ── ANSI color helpers ────────────────────────────────────────────────────────
std::string DisplayManager::green (const std::string& s) { return "\033[32m" + s + "\033[0m"; }
std::string DisplayManager::red   (const std::string& s) { return "\033[31m" + s + "\033[0m"; }
std::string DisplayManager::yellow(const std::string& s) { return "\033[33m" + s + "\033[0m"; }
std::string DisplayManager::bold  (const std::string& s) { return "\033[1m"  + s + "\033[0m"; }
std::string DisplayManager::cyan  (const std::string& s) { return "\033[36m" + s + "\033[0m"; }
std::string DisplayManager::dim   (const std::string& s) { return "\033[2m"  + s + "\033[0m"; }

void DisplayManager::clear_screen()    { std::cout << "\033[2J"; }
void DisplayManager::move_cursor_home(){ std::cout << "\033[H";  }

DisplayManager& DisplayManager::instance() {
    static DisplayManager inst;
    return inst;
}

void DisplayManager::init(int refresh_ms, int max_trades) {
    refresh_ms_ = refresh_ms;
    max_trades_ = max_trades;
}

void DisplayManager::on_trade(const Trade& trade) {
    std::lock_guard<std::mutex> lock(mutex_);
    recent_trades_.push_front(trade);
    if ((int)recent_trades_.size() > max_trades_)
        recent_trades_.pop_back();
}

std::vector<Trade> DisplayManager::recent_trades() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<Trade>(recent_trades_.begin(), recent_trades_.end());
}

void DisplayManager::on_tick(const MarketTick& tick) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& t : latest_ticks_) {
        if (t.symbol == tick.symbol) { t = tick; return; }
    }
    latest_ticks_.push_back(tick);
}

void DisplayManager::run(const std::atomic<bool>& running) {
    clear_screen();
    while (running) {
        render();
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms_));
    }
}

void DisplayManager::stop() {}

// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::render() {
    // Time string
    auto now   = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&now_t, &tm_buf);
    char tbuf[32];
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    move_cursor_home();

    render_header(std::string(tbuf));
    render_order_book();
    render_positions();
    render_indicators();
    render_trades();

    std::cout.flush();
}

static std::string pad(const std::string& s, int w, bool right_align = false) {
    if ((int)s.size() >= w) return s.substr(0, w);
    std::string padded = right_align
        ? std::string(w - s.size(), ' ') + s
        : s + std::string(w - s.size(), ' ');
    return padded;
}

static std::string fmt(double v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}

void DisplayManager::render_header(const std::string& now_str) {
    auto& pm = PositionManager::instance();
    double pv    = pm.portfolio_value();
    double tpnl  = pm.total_pnl();
    double cash  = pm.cash();

    (void)0; // sep removed — header uses box-drawing strings directly
    std::cout << bold(cyan("╔══════════════════════════════════════════════════════════════════════════════╗\n"));
    std::cout << bold(cyan("║")) << bold("  🏦  REAL-TIME TRADING SIMULATION SYSTEM")
              << dim("          " + now_str + "  ")
              << bold(cyan("║\n"));
    std::cout << bold(cyan("╠══════════════════════════════════════════════════════════════════════════════╣\n"));
    std::cout << bold(cyan("║")) << "  Portfolio: " << bold(fmt(pv, 2))
              << "  │  Cash: " << fmt(cash, 2)
              << "  │  Total PnL: "
              << (tpnl >= 0 ? green("+" + fmt(tpnl)) : red(fmt(tpnl)))
              << "                    "
              << bold(cyan("║\n"));
    std::cout << bold(cyan("╚══════════════════════════════════════════════════════════════════════════════╝\n"));
    std::cout << "\n";
}

void DisplayManager::render_order_book() {
    if (!g_engine) return;

    auto syms = g_engine->symbols();
    if (syms.empty()) { std::cout << dim("  [Order Book: No data yet]\n\n"); return; }

    // Rotate through symbols
    const std::string& sym = syms[book_symbol_idx_ % syms.size()];
    book_symbol_idx_ = (book_symbol_idx_ + 1) % syms.size();

    const OrderBook* book = g_engine->get_book(sym);
    if (!book) return;

    std::cout << bold("  📋 ORDER BOOK  ") << cyan("[" + sym + "]") << "\n";
    std::cout << dim("  " + std::string(60, '-')) << "\n";
    std::cout << "  " << bold(green(pad("BID QTY", 10)))
              << "  " << bold(green(pad("BID", 10, true)))
              << "  │  "
              << bold(red(pad("ASK", 10, true)))
              << "  " << bold(red(pad("ASK QTY", 10))) << "\n";
    std::cout << dim("  " + std::string(60, '-')) << "\n";

    auto bids = book->top_bids(5);
    auto asks = book->top_asks(5);
    int  rows = std::max(bids.size(), asks.size());

    for (int i = 0; i < std::max(rows, 1); ++i) {
        std::string bid_qty = (i < (int)bids.size()) ? fmt(bids[i].remaining(), 0) : "";
        std::string bid_px  = (i < (int)bids.size()) ? fmt(bids[i].price, 4) : "";
        std::string ask_px  = (i < (int)asks.size()) ? fmt(asks[i].price, 4) : "";
        std::string ask_qty = (i < (int)asks.size()) ? fmt(asks[i].remaining(), 0) : "";

        std::cout << "  " << green(pad(bid_qty, 10))
                  << "  " << green(pad(bid_px, 10, true))
                  << "  │  "
                  << red(pad(ask_px, 10, true))
                  << "  " << red(pad(ask_qty, 10)) << "\n";
    }
    double sp = book->spread();
    std::cout << "  " << dim("Spread: " + fmt(sp, 4)) << "\n\n";
}

void DisplayManager::render_positions() {
    auto positions = PositionManager::instance().all_positions();

    std::cout << bold("  💼 POSITIONS\n");
    std::cout << dim("  " + std::string(70, '-')) << "\n";
    std::cout << "  "
              << bold(pad("SYMBOL", 8)) << "  "
              << bold(pad("QTY", 8, true)) << "  "
              << bold(pad("AVG COST", 10, true)) << "  "
              << bold(pad("LAST", 10, true)) << "  "
              << bold(pad("UNREAL PnL", 12, true)) << "  "
              << bold(pad("REAL PnL", 10, true)) << "\n";
    std::cout << dim("  " + std::string(70, '-')) << "\n";

    if (positions.empty()) {
        std::cout << dim("  No open positions\n");
    } else {
        for (const auto& pos : positions) {
            double upnl = pos.unrealized_pnl();
            std::cout << "  "
                      << pad(pos.symbol, 8) << "  "
                      << pad(fmt(pos.quantity, 0), 8, true) << "  "
                      << pad(fmt(pos.avg_cost, 4), 10, true) << "  "
                      << pad(fmt(pos.last_price, 4), 10, true) << "  "
                      << (upnl >= 0
                              ? green(pad("+" + fmt(upnl), 12, true))
                              : red(pad(fmt(upnl), 12, true))) << "  "
                      << (pos.realized_pnl >= 0
                              ? green(pad("+" + fmt(pos.realized_pnl), 10, true))
                              : red(pad(fmt(pos.realized_pnl), 10, true))) << "\n";
        }
    }

    // Totals row
    double rpnl = PositionManager::instance().realized_pnl();
    double upnl = PositionManager::instance().unrealized_pnl();
    std::cout << dim("  " + std::string(70, '-')) << "\n";
    std::cout << "  " << bold("TOTAL") << std::string(43, ' ')
              << (upnl >= 0 ? green(pad("+" + fmt(upnl), 12, true))
                            : red(pad(fmt(upnl), 12, true))) << "  "
              << (rpnl >= 0 ? green(pad("+" + fmt(rpnl), 10, true))
                            : red(pad(fmt(rpnl), 10, true))) << "\n\n";
}

void DisplayManager::render_indicators() {
    std::vector<MarketTick> ticks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ticks = latest_ticks_;
    }

    std::cout << bold("  📊 INDICATORS\n");
    std::cout << dim("  " + std::string(70, '-')) << "\n";
    std::cout << "  "
              << bold(pad("SYMBOL", 7)) << "  "
              << bold(pad("PRICE", 10, true)) << "  "
              << bold(pad("SMA", 10, true)) << "  "
              << bold(pad("EMA", 10, true)) << "  "
              << bold(pad("RSI", 7, true)) << "  "
              << bold(pad("SIGNAL", 8)) << "\n";
    std::cout << dim("  " + std::string(70, '-')) << "\n";

    for (const auto& tick : ticks) {
        auto snap = IndicatorEngine::instance().snapshot(tick.symbol);
        std::string rsi_str = snap.rsi_valid ? fmt(snap.rsi, 1) : "-";
        std::string sma_str = snap.sma_valid ? fmt(snap.sma, 4) : "-";
        std::string ema_str = snap.ema_valid ? fmt(snap.ema, 4) : "-";

        std::string signal = "  -";
        if (snap.rsi_valid) {
            if (snap.rsi < 30)       signal = green("▲ BUY");
            else if (snap.rsi > 70)  signal = red  ("▼ SELL");
            else                     signal = dim  ("  HOLD");
        }

        // RSI colored by zone
        std::string rsi_colored = snap.rsi_valid
            ? (snap.rsi < 30 ? green(rsi_str) : snap.rsi > 70 ? red(rsi_str) : yellow(rsi_str))
            : dim(rsi_str);

        std::cout << "  "
                  << pad(tick.symbol, 7) << "  "
                  << pad(fmt(tick.price, 4), 10, true) << "  "
                  << pad(sma_str, 10, true) << "  "
                  << pad(ema_str, 10, true) << "  "
                  << pad(rsi_colored, 7 + 9, true) << "  " // +9 for ANSI escapes
                  << signal << "\n";
    }
    std::cout << "\n";
}

void DisplayManager::render_trades() {
    std::deque<Trade> trades;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        trades = recent_trades_;
    }

    std::cout << bold("  🔄 RECENT TRADES\n");
    std::cout << dim("  " + std::string(60, '-')) << "\n";

    if (trades.empty()) {
        std::cout << dim("  No trades yet\n");
    } else {
        std::cout << "  "
                  << bold(pad("SYMBOL", 7)) << "  "
                  << bold(pad("QTY", 8, true)) << "  "
                  << bold(pad("PRICE", 10, true)) << "  "
                  << bold(pad("VALUE", 12, true)) << "\n";
        for (const auto& t : trades) {
            std::cout << "  "
                      << pad(t.symbol, 7) << "  "
                      << pad(fmt(t.quantity, 0), 8, true) << "  "
                      << pad(fmt(t.price, 4), 10, true) << "  "
                      << pad("$" + fmt(t.quantity * t.price, 2), 12, true) << "\n";
        }
    }
    std::cout << "\n";
    std::cout << dim("  [Press Ctrl+C to exit]") << "\n";
}

} // namespace trading
