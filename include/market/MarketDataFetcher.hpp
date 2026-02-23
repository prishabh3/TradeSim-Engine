#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <random>
#include "common/Types.hpp"
#include "common/ThreadSafeQueue.hpp"

// Forward declare CURL to avoid including curl.h in header
using CURL = void;

namespace trading {

/**
 * MarketDataFetcher — runs on a dedicated data thread.
 *
 * Two modes (set in config.json):
 *   "simulation" — generates realistic Brownian-motion price data (default)
 *   "live"       — polls Alpha Vantage REST API via libcurl + JsonCpp
 *
 * In both modes, MarketTick objects are pushed to the shared market_queue.
 */
class MarketDataFetcher {
public:
    explicit MarketDataFetcher(ThreadSafeQueue<MarketTick>& market_queue);
    ~MarketDataFetcher();

    // Called from the data thread — blocks until stop() is called
    void run(const std::atomic<bool>& running);

    void stop();

private:
    // ── Simulation ────────────────────────────────────────────────────────────
    void run_simulation(const std::atomic<bool>& running);
    MarketTick generate_tick(const std::string& symbol);

    // ── Live API ──────────────────────────────────────────────────────────────
    void run_live(const std::atomic<bool>& running);
    bool fetch_tick_curl(const std::string& symbol, MarketTick& out_tick);

    static std::size_t write_callback(char* buf, std::size_t size,
                                      std::size_t nmemb, void* user_data);

    // ── Retry ─────────────────────────────────────────────────────────────────
    template <typename Fn>
    bool retry_with_backoff(Fn&& fn, int max_retries = 3);

    ThreadSafeQueue<MarketTick>&               market_queue_;
    std::vector<std::string>                   symbols_;
    std::unordered_map<std::string, double>    prices_;   // last known prices
    std::mt19937                               rng_;
    std::normal_distribution<double>           dist_;
    double                                     volatility_;
    int                                        poll_ms_;
    std::string                                mode_;
};

} // namespace trading
