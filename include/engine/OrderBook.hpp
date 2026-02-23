#pragma once

#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <ostream>
#include "common/Types.hpp"

namespace trading {

// ── Comparators ──────────────────────────────────────────────────────────────
struct BidComparator {
    // max-heap: highest price first (buyers compete upward)
    bool operator()(const Order& a, const Order& b) const {
        if (a.price != b.price) return a.price < b.price;
        return a.timestamp > b.timestamp; // FIFO for same price
    }
};

struct AskComparator {
    // min-heap: lowest price first (sellers compete downward)
    bool operator()(const Order& a, const Order& b) const {
        if (a.price != b.price) return a.price > b.price;
        return a.timestamp > b.timestamp; // FIFO for same price
    }
};

/**
 * OrderBook — maintains separate bid and ask priority queues.
 *
 * Thread-ownership: exclusively owned by the matching engine thread.
 * No internal locking — the engine is the single writer.
 */
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);

    // Add an order to the appropriate side
    void add_order(const Order& order);

    // Remove best bid/ask after matching
    void pop_best_bid();
    void pop_best_ask();

    // Peek at best prices (nullopt if book side is empty)
    std::optional<Order> best_bid() const;
    std::optional<Order> best_ask() const;
    std::optional<Order> best_ask_mutable();

    bool bids_empty() const;
    bool asks_empty() const;

    // Replace top of ask queue after partial fill
    void update_top_ask(const Order& updated);
    void update_top_bid(const Order& updated);

    // For display
    std::vector<Order> top_bids(int n = 5) const;
    std::vector<Order> top_asks(int n = 5) const;

    double spread() const;
    const std::string& symbol() const { return symbol_; }

    void print(std::ostream& os) const;

private:
    std::string symbol_;

    // Using priority_queue over multimap for O(log N) push/pop
    std::priority_queue<Order, std::vector<Order>, BidComparator> bids_;
    std::priority_queue<Order, std::vector<Order>, AskComparator> asks_;
};

} // namespace trading
