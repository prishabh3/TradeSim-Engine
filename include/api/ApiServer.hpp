#pragma once

#include <atomic>
#include <string>
#include "common/ThreadSafeQueue.hpp"
#include "common/Types.hpp"

namespace trading {

/**
 * ApiServer — Thread 7: lightweight HTTP REST server.
 *
 * Endpoints:
 *   GET  /api/portfolio   — cash, value, pnl
 *   GET  /api/positions   — all open positions
 *   GET  /api/indicators  — SMA/EMA/RSI per symbol
 *   GET  /api/orderbook   — top-5 bids/asks (query: ?symbol=AAPL)
 *   GET  /api/trades      — last N fills
 *   GET  /api/symbols     — active symbol list
 *   POST /api/order       — place a new order into the matching engine
 */
class ApiServer {
public:
    static ApiServer& instance();

    void init(ThreadSafeQueue<Order>& order_queue, int port = 8080);

    // Blocks until running becomes false
    void run(const std::atomic<bool>& running);

private:
    ApiServer() = default;
    ApiServer(const ApiServer&) = delete;

    int port_ = 8080;
    ThreadSafeQueue<Order>* order_queue_ = nullptr;
};

} // namespace trading
