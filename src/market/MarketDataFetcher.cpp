#include "market/MarketDataFetcher.hpp"
#include "common/Config.hpp"
#include "common/Logger.hpp"
#include <thread>
#include <chrono>
#include <sstream>
#include <cmath>
#include <curl/curl.h>
#include <json/json.h>

namespace trading {

MarketDataFetcher::MarketDataFetcher(ThreadSafeQueue<MarketTick>& market_queue)
    : market_queue_(market_queue),
      rng_(std::random_device{}()),
      dist_(0.0, 1.0)
{
    auto& cfg = Config::instance();
    symbols_    = cfg.symbols();
    volatility_ = cfg.volatility();
    poll_ms_    = cfg.poll_interval_ms();
    mode_       = cfg.mode();

    // Initialize simulation prices from config
    for (const auto& sym : symbols_) {
        prices_[sym] = cfg.initial_price(sym);
    }
}

MarketDataFetcher::~MarketDataFetcher() {
    stop();
}

void MarketDataFetcher::run(const std::atomic<bool>& running) {
    LOG_INFO("MarketDataFetcher starting in [" + mode_ + "] mode");
    if (mode_ == "simulation")
        run_simulation(running);
    else
        run_live(running);
    LOG_INFO("MarketDataFetcher stopped");
}

void MarketDataFetcher::stop() {
    // Stopping is handled by the external `running` flag
}

// ─── Simulation ──────────────────────────────────────────────────────────────
void MarketDataFetcher::run_simulation(const std::atomic<bool>& running) {
    while (running) {
        for (const auto& sym : symbols_) {
            if (!running) break;
            auto tick = generate_tick(sym);
            market_queue_.push(tick);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms_));
    }
}

MarketTick MarketDataFetcher::generate_tick(const std::string& symbol) {
    // Geometric Brownian Motion: dS = S * vol * dW
    double& price = prices_[symbol];
    double  dW    = dist_(rng_);
    price = price * std::exp(volatility_ * dW);
    price = std::max(price, 1.0); // floor at $1

    double spread = price * 0.001; // 0.1% bid-ask spread

    MarketTick tick;
    tick.symbol = symbol;
    tick.price  = price;
    tick.volume = 100.0 + std::abs(dW) * 500.0;
    tick.bid    = price - spread / 2.0;
    tick.ask    = price + spread / 2.0;
    tick.time   = std::chrono::steady_clock::now();
    return tick;
}

// ─── Live cURL ────────────────────────────────────────────────────────────────
std::size_t MarketDataFetcher::write_callback(char* buf, std::size_t size,
                                               std::size_t nmemb, void* user_data) {
    auto* str = reinterpret_cast<std::string*>(user_data);
    str->append(buf, size * nmemb);
    return size * nmemb;
}

void MarketDataFetcher::run_live(const std::atomic<bool>& running) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    while (running) {
        for (const auto& sym : symbols_) {
            if (!running) break;
            MarketTick tick;
            bool ok = false;
            retry_with_backoff([&] { return (ok = fetch_tick_curl(sym, tick)); });
            if (ok) market_queue_.push(tick);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms_));
    }
    curl_global_cleanup();
}

bool MarketDataFetcher::fetch_tick_curl(const std::string& symbol, MarketTick& out_tick) {
    auto& cfg = Config::instance();
    std::string url = cfg.api_url()
        + "?function=GLOBAL_QUOTE&symbol=" + symbol
        + "&apikey=" + cfg.api_key();

    std::string response_body;
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_WARN("cURL error for " + symbol + ": " + curl_easy_strerror(res));
        return false;
    }

    // Parse Alpha Vantage JSON response
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::istringstream iss(response_body);
    std::string errs;
    if (!Json::parseFromStream(builder, iss, &root, &errs)) {
        LOG_WARN("JSON parse error: " + errs);
        return false;
    }

    try {
        const auto& quote = root["Global Quote"];
        double price = std::stod(quote["05. price"].asString());
        double volume = std::stod(quote["06. volume"].asString());
        double spread = price * 0.001;

        out_tick.symbol = symbol;
        out_tick.price  = price;
        out_tick.volume = volume;
        out_tick.bid    = price - spread / 2.0;
        out_tick.ask    = price + spread / 2.0;
        out_tick.time   = std::chrono::steady_clock::now();
        return true;
    } catch (...) {
        LOG_WARN("Failed to extract price for " + symbol);
        return false;
    }
}

template <typename Fn>
bool MarketDataFetcher::retry_with_backoff(Fn&& fn, int max_retries) {
    int attempt = 0;
    while (attempt < max_retries) {
        if (fn()) return true;
        ++attempt;
        auto delay = std::chrono::milliseconds(200 * (1 << attempt)); // 400, 800, 1600ms
        LOG_WARN("Retrying (attempt " + std::to_string(attempt) + "/" +
                 std::to_string(max_retries) + ") after " +
                 std::to_string(delay.count()) + "ms");
        std::this_thread::sleep_for(delay);
    }
    return false;
}

} // namespace trading
