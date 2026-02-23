#include "engine/OrderBook.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace trading {

OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {}

void OrderBook::add_order(const Order& order) {
    if (order.side == OrderSide::BUY)
        bids_.push(order);
    else
        asks_.push(order);
}

void OrderBook::pop_best_bid() {
    if (!bids_.empty()) bids_.pop();
}

void OrderBook::pop_best_ask() {
    if (!asks_.empty()) asks_.pop();
}

std::optional<Order> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.top();
}

std::optional<Order> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.top();
}

bool OrderBook::bids_empty() const { return bids_.empty(); }
bool OrderBook::asks_empty() const { return asks_.empty(); }

void OrderBook::update_top_ask(const Order& updated) {
    if (!asks_.empty()) asks_.pop();
    if (updated.remaining() > 1e-9) asks_.push(updated);
}

void OrderBook::update_top_bid(const Order& updated) {
    if (!bids_.empty()) bids_.pop();
    if (updated.remaining() > 1e-9) bids_.push(updated);
}

double OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return 0.0;
    return ask->price - bid->price;
}

// Extract top N bids (draining copy trick since priority_queue has no iterator)
std::vector<Order> OrderBook::top_bids(int n) const {
    auto tmp = bids_; // copy
    std::vector<Order> result;
    while (!tmp.empty() && (int)result.size() < n) {
        result.push_back(tmp.top());
        tmp.pop();
    }
    return result;
}

std::vector<Order> OrderBook::top_asks(int n) const {
    auto tmp = asks_; // copy
    std::vector<Order> result;
    while (!tmp.empty() && (int)result.size() < n) {
        result.push_back(tmp.top());
        tmp.pop();
    }
    return result;
}

void OrderBook::print(std::ostream& os) const {
    os << "=== Order Book: " << symbol_ << " ===\n";
    os << std::setw(12) << "BID QTY" << "  " << std::setw(10) << "BID"
       << "  " << std::setw(10) << "ASK" << "  " << std::setw(12) << "ASK QTY" << "\n";
    os << std::string(50, '-') << "\n";

    auto bids = top_bids(5);
    auto asks = top_asks(5);
    int rows = std::max(bids.size(), asks.size());

    for (int i = 0; i < rows; ++i) {
        std::string bid_qty = (i < (int)bids.size())
            ? std::to_string((int)bids[i].remaining()) : "";
        std::string bid_px = (i < (int)bids.size())
            ? std::to_string(bids[i].price).substr(0, 8) : "";
        std::string ask_px = (i < (int)asks.size())
            ? std::to_string(asks[i].price).substr(0, 8) : "";
        std::string ask_qty = (i < (int)asks.size())
            ? std::to_string((int)asks[i].remaining()) : "";

        os << std::setw(12) << bid_qty << "  "
           << std::setw(10) << bid_px    << "  "
           << std::setw(10) << ask_px    << "  "
           << std::setw(12) << ask_qty   << "\n";
    }
    os << "  Spread: " << std::fixed << std::setprecision(4) << spread() << "\n";
}

} // namespace trading
