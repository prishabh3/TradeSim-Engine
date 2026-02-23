#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <string>
#include <filesystem>

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "common/ThreadSafeQueue.hpp"
#include "common/Types.hpp"
#include "market/MarketDataFetcher.hpp"
#include "engine/OrderMatchingEngine.hpp"
#include "strategy/IndicatorEngine.hpp"
#include "strategy/RiskManager.hpp"
#include "strategy/TradingStrategy.hpp"
#include "portfolio/PositionManager.hpp"
#include "ui/DisplayManager.hpp"
#include "api/ApiServer.hpp"

namespace trading {
    // Global engine pointer used by DisplayManager for order book access
    OrderMatchingEngine* g_engine = nullptr;
}

static std::atomic<bool> g_running{true};

void signal_handler(int /*sig*/) {
    g_running = false;
    trading::Logger::instance().warn("Shutdown signal received — stopping all threads...");
}

int main(int argc, char* argv[]) {
    using namespace trading;

    // ── Locate config file ────────────────────────────────────────────────────
    std::string config_path = "config/config.json";
    if (argc > 1) config_path = argv[1];

    // Handle running from build subdirectory
    if (!std::filesystem::exists(config_path)) {
        config_path = "../config/config.json";
    }

    // ── Load config ───────────────────────────────────────────────────────────
    try {
        Config::instance().load(config_path);
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Config load failed: " << e.what() << "\n";
        return 1;
    }

    auto& cfg = Config::instance();

    // ── Init logger ───────────────────────────────────────────────────────────
    LogLevel log_level = LogLevel::INFO;
    if      (cfg.log_level() == "DEBUG") log_level = LogLevel::DEBUG;
    else if (cfg.log_level() == "WARN")  log_level = LogLevel::WARN;
    else if (cfg.log_level() == "ERROR") log_level = LogLevel::ERROR;

    Logger::instance().init(cfg.log_file(), log_level);
    LOG_INFO("=== Real-Time Trading Simulation System starting ===");

    // ── Init subsystems ───────────────────────────────────────────────────────
    IndicatorEngine::instance().init(cfg.sma_period(), cfg.ema_period(), cfg.rsi_period());

    RiskConfig risk_cfg;
    risk_cfg.max_position_usd       = cfg.max_position_usd();
    risk_cfg.max_total_exposure_usd  = cfg.max_total_exposure_usd();
    risk_cfg.max_loss_per_trade_usd  = cfg.max_loss_per_trade_usd();
    risk_cfg.stop_loss_pct           = cfg.stop_loss_pct();
    RiskManager::instance().init(risk_cfg);

    PositionManager::instance().init(cfg.initial_cash());
    DisplayManager::instance().init(cfg.display_refresh_ms(), cfg.max_recent_trades());

    // ── Shared queues ─────────────────────────────────────────────────────────
    // Separate queues for market data, orders, and trades
    // Each queue is the thread-boundary between subsystems
    ThreadSafeQueue<MarketTick> market_queue_strategy; // fetcher → strategy
    ThreadSafeQueue<MarketTick> market_queue_display;  // fetcher → display
    ThreadSafeQueue<Order>      order_queue;            // strategy → engine
    ThreadSafeQueue<Trade>      trade_queue;            // engine → pnl/display

    ApiServer::instance().init(order_queue, 8080);

    // ── Create engine (before threads start) ─────────────────────────────────
    OrderMatchingEngine engine(order_queue, trade_queue);
    trading::g_engine = &engine;

    // ── Register fill callback: engine → risk + position ─────────────────────
    engine.set_fill_callback([&](const Trade& trade) {
        // Determine side from internal tracking is non-trivial; use a heuristic:
        // For simplicity, we record as BUY (position manager will handle closing via SELL)
        // A real system would tag orders with side before matching
        // Here we just note the fill for both display and PnL
        trade_queue.push(trade); // also push to trade_queue for PnL thread
    });

    // ── Register signal handlers ──────────────────────────────────────────────
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 1: Market Data Fetcher
    //   Produces MarketTick → market_queue_strategy, market_queue_display
    // ─────────────────────────────────────────────────────────────────────────
    MarketDataFetcher fetcher(market_queue_strategy);

    std::thread data_thread([&] {
        fetcher.run(g_running);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 2: Market data fan-out (copy ticks to display queue)
    // ─────────────────────────────────────────────────────────────────────────
    std::thread fanout_thread([&] {
        while (g_running) {
            auto tick = market_queue_strategy.pop(std::chrono::milliseconds(100));
            if (!tick) continue;
            // Update mark-to-market prices
            PositionManager::instance().update_mark(tick->symbol, tick->price);
            market_queue_display.push(*tick);
            // Also re-push to strategy queue via a separate queue
            // (Strategy will read from market_queue_strategy,
            //  but we already popped it — strategy reads from market_queue_display)
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 3: Strategy / Processing Thread
    //   Consumes MarketTick, computes indicators, generates Orders
    // ─────────────────────────────────────────────────────────────────────────
    TradingStrategy strategy(order_queue);
    std::thread strategy_thread([&] {
        strategy.run(market_queue_display, g_running);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 4: Order Matching Engine
    //   Consumes Order → produces Trade
    // ─────────────────────────────────────────────────────────────────────────
    std::thread engine_thread([&] {
        engine.run();
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 5: PnL / Trade Processing Thread
    //   Consumes Trade → updates PositionManager + RiskManager + DisplayManager
    // ─────────────────────────────────────────────────────────────────────────
    std::thread pnl_thread([&] {
        LOG_INFO("PnL thread started");
        // Track order sides simply by looking at sequence (buy first, sell second)
        // In a real system, you'd tag each order with side and join on order ID
        static bool buy_turn = true;
        while (g_running) {
            auto trade = trade_queue.pop(std::chrono::milliseconds(100));
            if (!trade) continue;

            // Alternate BUY/SELL for demonstration (real system tags per order ID)
            OrderSide side = buy_turn ? OrderSide::BUY : OrderSide::SELL;
            buy_turn = !buy_turn;

            PositionManager::instance().record_trade(*trade, side);
            RiskManager::instance().record_fill(*trade, side);
            DisplayManager::instance().on_trade(*trade);

            LOG_INFO("Trade processed: " + trade->symbol +
                     " qty=" + std::to_string(trade->quantity) +
                     " px="  + std::to_string(trade->price));
        }
        LOG_INFO("PnL thread stopped");
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 6: UI Display Thread
    //   Reads snapshots from all subsystems — renders to terminal
    // ─────────────────────────────────────────────────────────────────────────
    std::thread ui_thread([&] {
        DisplayManager::instance().run(g_running);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Thread 7: REST API Server
    //   Serves JSON snapshots over HTTP on port 8080 for the web dashboard
    // ─────────────────────────────────────────────────────────────────────────
    std::thread api_thread([&] {
        ApiServer::instance().run(g_running);
    });

    LOG_INFO("All threads started. Running (Ctrl+C to stop)...");

    // ── Wait for all threads ──────────────────────────────────────────────────
    data_thread.join();
    LOG_INFO("Data thread joined");

    fanout_thread.join();

    // Signal downstream queues to shut down
    market_queue_display.shutdown();
    strategy_thread.join();
    LOG_INFO("Strategy thread joined");

    order_queue.shutdown();
    engine.stop();
    engine_thread.join();
    LOG_INFO("Engine thread joined");

    trade_queue.shutdown();
    pnl_thread.join();
    LOG_INFO("PnL thread joined");

    ui_thread.join();
    LOG_INFO("UI thread joined");

    api_thread.join();
    LOG_INFO("API thread joined");

    Logger::instance().info("=== Trading system shutdown complete ===");
    Logger::instance().shutdown();

    return 0;
}
