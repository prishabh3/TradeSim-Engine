#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <json/json.h>

namespace trading {

/**
 * Singleton config loader — reads config/config.json once at startup.
 * All subsystems call Config::instance().get_*() to read params.
 */
class Config {
public:
    static Config& instance();

    void load(const std::string& filepath);

    // Market data
    std::string              mode()             const;
    std::string              api_url()          const;
    std::string              api_key()          const;
    int                      poll_interval_ms() const;
    std::vector<std::string> symbols()          const;

    // Indicators
    int sma_period() const;
    int ema_period() const;
    int rsi_period() const;

    // Risk
    double max_position_usd()      const;
    double max_total_exposure_usd() const;
    double max_loss_per_trade_usd() const;
    double stop_loss_pct()         const;

    // Strategy
    double rsi_oversold()    const;
    double rsi_overbought()  const;
    double order_quantity()  const;
    bool   strategy_enabled() const;

    // Logging
    std::string log_level() const;
    std::string log_file()  const;

    // Simulation
    double initial_price(const std::string& symbol) const;
    double volatility()    const;
    double initial_cash()  const;

    // Display
    int display_refresh_ms()    const;
    int max_recent_trades()     const;

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    Json::Value root_;
    bool loaded_ = false;
};

} // namespace trading
