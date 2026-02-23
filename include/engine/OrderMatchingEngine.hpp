#pragma once

#include <unordered_map>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>
#include "common/Types.hpp"
#include "common/ThreadSafeQueue.hpp"
#include "engine/OrderBook.hpp"

namespace trading {

using FillCallback = std::function<void(const Trade&)>;

/**
 * OrderMatchingEngine — single-threaded engine that consumes from OrderQueue
 * and produces Trades to TradeQueue.
 *
 * No locking needed inside: the engine is the sole owner of all OrderBooks.
 * Thread safety comes from the queues at the boundaries.
 *
 * Supports:
 *   - MARKET orders: sweep opposite side regardless of price
 *   - LIMIT  orders: rest on book if no match; match if price crosses
 *   - Partial fills: residual stays on book
 */
class OrderMatchingEngine {
public:
    OrderMatchingEngine(ThreadSafeQueue<Order>& order_queue,
                        ThreadSafeQueue<Trade>& trade_queue);

    // Main loop — blocks until stop() is called
    void run();
    void stop();

    // Optional fill callback (called synchronously in engine thread)
    void set_fill_callback(FillCallback cb) { fill_cb_ = std::move(cb); }

    // Access order book for a symbol (for display thread — read-only snapshot)
    // Returns nullptr if symbol not tracked
    const OrderBook* get_book(const std::string& symbol) const;

    // Snapshot of all books for thread-safe display
    std::vector<std::string> symbols() const;

    // Inject an order directly (bypasses queue, for tests)
    void process_order_direct(Order order);

private:
    void process_order(Order order);
    void match_buy (Order& buy,  OrderBook& book);
    void match_sell(Order& sell, OrderBook& book);

    Trade make_trade(const Order& buy, const Order& sell,
                     double fill_price, double fill_qty) const;

    OrderBook& get_or_create_book(const std::string& symbol);

    ThreadSafeQueue<Order>& order_queue_;
    ThreadSafeQueue<Trade>& trade_queue_;

    std::unordered_map<std::string, OrderBook> books_;
    std::atomic<bool>  shutdown_{false};
    std::atomic<uint64_t> order_counter_{1};

    FillCallback fill_cb_;
};

} // namespace trading
