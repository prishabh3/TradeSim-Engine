#include "api/ApiServer.hpp"

// ── httplib (header-only) ─────────────────────────────────────────────────────
// OpenSSL/Zlib disabled via CMakeLists.txt compile definitions
#include "api/httplib.h"

#include "portfolio/PositionManager.hpp"
#include "strategy/IndicatorEngine.hpp"
#include "engine/OrderMatchingEngine.hpp"
#include "ui/DisplayManager.hpp"
#include "common/Config.hpp"
#include "common/Logger.hpp"

#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <ctime>

// Forward-declare the global engine pointer defined in main.cpp
namespace trading {
    extern OrderMatchingEngine* g_engine;
}

namespace trading {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string to_json_str(const Json::Value& v) {
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    return Json::writeString(wb, v);
}

static void add_cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
}

static void json_response(httplib::Response& res, const Json::Value& body) {
    add_cors(res);
    res.set_content(to_json_str(body), "application/json");
}

// ── Singleton ─────────────────────────────────────────────────────────────────

ApiServer& ApiServer::instance() {
    static ApiServer inst;
    return inst;
}

void ApiServer::init(ThreadSafeQueue<Order>& order_queue, int port) {
    order_queue_ = &order_queue;
    port_ = port;
}

// ── Run (blocks until running == false) ──────────────────────────────────────

void ApiServer::run(const std::atomic<bool>& running) {
    httplib::Server svr;

    // ── GET /api/portfolio ────────────────────────────────────────────────────
    svr.Get("/api/portfolio", [](const httplib::Request&, httplib::Response& res) {
        auto& pm = PositionManager::instance();
        Json::Value v;
        v["cash"]            = pm.cash();
        v["portfolio_value"] = pm.portfolio_value();
        v["total_pnl"]       = pm.total_pnl();
        v["realized_pnl"]    = pm.realized_pnl();
        v["unrealized_pnl"]  = pm.unrealized_pnl();
        json_response(res, v);
    });

    // ── GET /api/positions ────────────────────────────────────────────────────
    svr.Get("/api/positions", [](const httplib::Request&, httplib::Response& res) {
        auto& pm = PositionManager::instance();
        auto positions = pm.all_positions();
        Json::Value arr(Json::arrayValue);
        for (const auto& p : positions) {
            Json::Value item;
            item["symbol"]        = p.symbol;
            item["quantity"]      = p.quantity;
            item["avg_cost"]      = p.avg_cost;
            item["last_price"]    = p.last_price;
            item["unrealized_pnl"]= p.unrealized_pnl();
            item["realized_pnl"]  = p.realized_pnl;
            item["total_pnl"]     = p.total_pnl();
            arr.append(item);
        }
        Json::Value v;
        v["positions"] = arr;
        json_response(res, v);
    });

    // ── GET /api/indicators ───────────────────────────────────────────────────
    svr.Get("/api/indicators", [](const httplib::Request&, httplib::Response& res) {
        auto& ie      = IndicatorEngine::instance();
        auto& cfg     = Config::instance();
        auto  symbols = cfg.symbols();

        Json::Value arr(Json::arrayValue);
        for (const auto& sym : symbols) {
            auto snap = ie.snapshot(sym);
            Json::Value item;
            item["symbol"]     = sym;
            item["sma"]        = snap.sma_valid ? snap.sma : 0.0;
            item["ema"]        = snap.ema_valid ? snap.ema : 0.0;
            item["rsi"]        = snap.rsi_valid ? snap.rsi : 50.0;
            item["sma_valid"]  = snap.sma_valid;
            item["ema_valid"]  = snap.ema_valid;
            item["rsi_valid"]  = snap.rsi_valid;

            // Signal from RSI
            std::string signal = "HOLD";
            if (snap.rsi_valid) {
                if (snap.rsi < 30.0)      signal = "BUY";
                else if (snap.rsi > 70.0) signal = "SELL";
            }
            item["signal"] = signal;
            item["last_price"] = snap.price;   // populated by IndicatorEngine::update()

            arr.append(item);
        }
        Json::Value v;
        v["indicators"] = arr;
        json_response(res, v);
    });

    // ── GET /api/orderbook?symbol=AAPL ────────────────────────────────────────
    svr.Get("/api/orderbook", [](const httplib::Request& req, httplib::Response& res) {
        std::string symbol = "AAPL";
        if (req.has_param("symbol")) symbol = req.get_param_value("symbol");

        Json::Value bids_arr(Json::arrayValue);
        Json::Value asks_arr(Json::arrayValue);

        if (g_engine) {
            const auto* book = g_engine->get_book(symbol);
            if (book) {
                auto bids = book->top_bids(5);
                for (const auto& o : bids) {
                    Json::Value entry;
                    entry["price"]    = o.price;
                    entry["quantity"] = o.remaining();
                    bids_arr.append(entry);
                }
                auto asks = book->top_asks(5);
                for (const auto& o : asks) {
                    Json::Value entry;
                    entry["price"]    = o.price;
                    entry["quantity"] = o.remaining();
                    asks_arr.append(entry);
                }
            }
        }

        Json::Value v;
        v["symbol"] = symbol;
        v["bids"]   = bids_arr;
        v["asks"]   = asks_arr;
        json_response(res, v);
    });

    // ── GET /api/symbols ──────────────────────────────────────────────────────
    svr.Get("/api/symbols", [](const httplib::Request&, httplib::Response& res) {
        auto& cfg     = Config::instance();
        auto  symbols = cfg.symbols();
        Json::Value arr(Json::arrayValue);
        for (const auto& s : symbols) arr.append(s);
        Json::Value v;
        v["symbols"] = arr;
        json_response(res, v);
    });

    // ── GET /api/trades ───────────────────────────────────────────────────────
    svr.Get("/api/trades", [](const httplib::Request&, httplib::Response& res) {
        auto trades = DisplayManager::instance().recent_trades();
        Json::Value arr(Json::arrayValue);
        for (const auto& t : trades) {
            Json::Value item;
            item["id"]       = static_cast<Json::UInt64>(t.buy_order_id);
            item["symbol"]   = t.symbol;
            item["price"]    = t.price;
            item["quantity"] = t.quantity;
            item["time"]     = "";
            arr.append(item);
        }
        Json::Value v;
        v["trades"] = arr;
        json_response(res, v);
    });

    // ── POST /api/order ───────────────────────────────────────────────────────
    svr.Post("/api/order", [this](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        if (!order_queue_) {
            res.status = 503;
            res.set_content("{\"error\":\"order queue not available\"}", "application/json");
            return;
        }

        Json::Value body;
        Json::CharReaderBuilder rb;
        std::string errs;
        std::istringstream ss(req.body);
        if (!Json::parseFromStream(rb, ss, &body, &errs)) {
            res.status = 400;
            res.set_content("{\"error\":\"invalid JSON\"}", "application/json");
            return;
        }

        static std::atomic<uint64_t> order_id{10000};

        Order o;
        o.id       = ++order_id;
        o.symbol   = body.get("symbol", "AAPL").asString();
        o.quantity = body.get("quantity", 1.0).asDouble();
        o.price    = body.get("price", 0.0).asDouble();

        std::string side_str = body.get("side", "BUY").asString();
        o.side = (side_str == "SELL") ? OrderSide::SELL : OrderSide::BUY;

        std::string type_str = body.get("type", "LIMIT").asString();
        o.type = (type_str == "MARKET") ? OrderType::MARKET : OrderType::LIMIT;
        if (o.type == OrderType::MARKET) o.price = 0.0;

        o.status    = OrderStatus::PENDING;
        o.timestamp = std::chrono::steady_clock::now();

        order_queue_->push(o);

        Json::Value resp;
        resp["status"]   = "accepted";
        resp["order_id"] = static_cast<Json::UInt64>(o.id);
        resp["symbol"]   = o.symbol;
        resp["side"]     = side_str;
        resp["quantity"] = o.quantity;
        resp["price"]    = o.price;
        json_response(res, resp);

        LOG_INFO("REST order placed: " + o.symbol + " " + side_str +
                 " qty=" + std::to_string(o.quantity) +
                 " px="  + std::to_string(o.price));
    });

    // ── OPTIONS preflight (CORS) ───────────────────────────────────────────────
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.status = 204;
    });

    // ── Start listening ───────────────────────────────────────────────────────
    LOG_INFO("ApiServer listening on http://localhost:" + std::to_string(port_));

    // Poll running flag every 200ms to honour shutdown
    svr.set_keep_alive_timeout(1);
    std::thread stop_thread([&] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        svr.stop();
    });

    svr.listen("0.0.0.0", port_);

    if (stop_thread.joinable()) stop_thread.join();

    LOG_INFO("ApiServer stopped.");
}

} // namespace trading
