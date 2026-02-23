#include "common/Config.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace trading {

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::load(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open())
        throw std::runtime_error("Cannot open config file: " + filepath);

    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, ifs, &root_, &errs))
        throw std::runtime_error("Config parse error: " + errs);

    loaded_ = true;
    std::cout << "[Config] Loaded: " << filepath << "\n";
}

// ── Market data ─────────────────────────────────────────────────────────────
std::string Config::mode()             const { return root_["market_data"]["mode"].asString(); }
std::string Config::api_url()          const { return root_["market_data"]["api_url"].asString(); }
std::string Config::api_key()          const { return root_["market_data"]["api_key"].asString(); }
int         Config::poll_interval_ms() const { return root_["market_data"]["poll_interval_ms"].asInt(); }

std::vector<std::string> Config::symbols() const {
    std::vector<std::string> syms;
    for (const auto& s : root_["market_data"]["symbols"])
        syms.push_back(s.asString());
    return syms;
}

// ── Indicators ───────────────────────────────────────────────────────────────
int Config::sma_period() const { return root_["indicators"]["sma_period"].asInt(); }
int Config::ema_period() const { return root_["indicators"]["ema_period"].asInt(); }
int Config::rsi_period() const { return root_["indicators"]["rsi_period"].asInt(); }

// ── Risk ─────────────────────────────────────────────────────────────────────
double Config::max_position_usd()       const { return root_["risk"]["max_position_usd"].asDouble(); }
double Config::max_total_exposure_usd() const { return root_["risk"]["max_total_exposure_usd"].asDouble(); }
double Config::max_loss_per_trade_usd() const { return root_["risk"]["max_loss_per_trade_usd"].asDouble(); }
double Config::stop_loss_pct()          const { return root_["risk"]["stop_loss_pct"].asDouble(); }

// ── Strategy ─────────────────────────────────────────────────────────────────
double Config::rsi_oversold()    const { return root_["strategy"]["rsi_oversold_threshold"].asDouble(); }
double Config::rsi_overbought()  const { return root_["strategy"]["rsi_overbought_threshold"].asDouble(); }
double Config::order_quantity()  const { return root_["strategy"]["order_quantity"].asDouble(); }
bool   Config::strategy_enabled() const { return root_["strategy"]["enabled"].asBool(); }

// ── Logging ───────────────────────────────────────────────────────────────────
std::string Config::log_level() const { return root_["logging"]["level"].asString(); }
std::string Config::log_file()  const { return root_["logging"]["file"].asString(); }

// ── Simulation ────────────────────────────────────────────────────────────────
double Config::initial_price(const std::string& symbol) const {
    return root_["simulation"]["initial_prices"][symbol].asDouble();
}
double Config::volatility()   const { return root_["simulation"]["volatility"].asDouble(); }
double Config::initial_cash() const { return root_["simulation"]["initial_cash"].asDouble(); }

// ── Display ───────────────────────────────────────────────────────────────────
int Config::display_refresh_ms() const { return root_["display"]["refresh_interval_ms"].asInt(); }
int Config::max_recent_trades()  const { return root_["display"]["max_recent_trades"].asInt(); }

} // namespace trading
