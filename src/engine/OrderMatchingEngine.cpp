#include "engine/OrderMatchingEngine.hpp"
#include "common/Logger.hpp"
#include <sstream>
#include <iomanip>

namespace trading {

OrderMatchingEngine::OrderMatchingEngine(ThreadSafeQueue<Order>& order_queue,
                                         ThreadSafeQueue<Trade>& trade_queue)
    : order_queue_(order_queue), trade_queue_(trade_queue) {}

void OrderMatchingEngine::run() {
    LOG_INFO("OrderMatchingEngine started");
    while (!shutdown_) {
        auto opt = order_queue_.pop(std::chrono::milliseconds(100));
        if (!opt) continue;
        process_order(std::move(*opt));
    }
    LOG_INFO("OrderMatchingEngine stopped");
}

void OrderMatchingEngine::stop() {
    shutdown_ = true;
    order_queue_.shutdown();
}

void OrderMatchingEngine::process_order_direct(Order order) {
    process_order(std::move(order));
}

const OrderBook* OrderMatchingEngine::get_book(const std::string& symbol) const {
    auto it = books_.find(symbol);
    return (it != books_.end()) ? &it->second : nullptr;
}

std::vector<std::string> OrderMatchingEngine::symbols() const {
    std::vector<std::string> syms;
    for (const auto& [k, _] : books_) syms.push_back(k);
    return syms;
}

OrderBook& OrderMatchingEngine::get_or_create_book(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end()) {
        books_.emplace(symbol, OrderBook(symbol));
        it = books_.find(symbol);
    }
    return it->second;
}

void OrderMatchingEngine::process_order(Order order) {
    // Assign a monotonic order ID
    order.id = order_counter_++;
    auto& book = get_or_create_book(order.symbol);

    if (order.side == OrderSide::BUY)
        match_buy(order, book);
    else
        match_sell(order, book);
}

void OrderMatchingEngine::match_buy(Order& buy, OrderBook& book) {
    while (!book.asks_empty() && buy.remaining() > 1e-9) {
        auto ask_opt = book.best_ask();
        if (!ask_opt) break;
        Order ask = *ask_opt;

        // LIMIT: only match if ask price <= buy limit price
        if (buy.type == OrderType::LIMIT && ask.price > buy.price) break;

        double fill_qty = std::min(buy.remaining(), ask.remaining());
        double fill_px  = ask.price; // price-time priority: use resting order's price

        buy.filled_qty += fill_qty;
        ask.filled_qty += fill_qty;

        Trade t = make_trade(buy, ask, fill_px, fill_qty);
        trade_queue_.push(t);
        if (fill_cb_) fill_cb_(t);

        std::ostringstream oss;
        oss << "FILL " << buy.symbol << " BUY " << std::fixed << std::setprecision(4)
            << fill_qty << " @ " << fill_px;
        LOG_INFO(oss.str());

        // Update or remove the ask
        book.update_top_ask(ask);
    }

    // Residual of a LIMIT BUY rests on the book
    if (buy.type == OrderType::LIMIT && buy.remaining() > 1e-9) {
        buy.status = (buy.filled_qty > 0) ? OrderStatus::PARTIAL : OrderStatus::PENDING;
        book.add_order(buy);
    } else {
        buy.status = OrderStatus::FILLED;
    }
}

void OrderMatchingEngine::match_sell(Order& sell, OrderBook& book) {
    while (!book.bids_empty() && sell.remaining() > 1e-9) {
        auto bid_opt = book.best_bid();
        if (!bid_opt) break;
        Order bid = *bid_opt;

        // LIMIT: only match if bid >= sell limit price
        if (sell.type == OrderType::LIMIT && bid.price < sell.price) break;

        double fill_qty = std::min(sell.remaining(), bid.remaining());
        double fill_px  = bid.price;

        sell.filled_qty += fill_qty;
        bid.filled_qty  += fill_qty;

        Trade t = make_trade(bid, sell, fill_px, fill_qty);
        trade_queue_.push(t);
        if (fill_cb_) fill_cb_(t);

        std::ostringstream oss;
        oss << "FILL " << sell.symbol << " SELL " << std::fixed << std::setprecision(4)
            << fill_qty << " @ " << fill_px;
        LOG_INFO(oss.str());

        book.update_top_bid(bid);
    }

    if (sell.type == OrderType::LIMIT && sell.remaining() > 1e-9) {
        sell.status = (sell.filled_qty > 0) ? OrderStatus::PARTIAL : OrderStatus::PENDING;
        book.add_order(sell);
    } else {
        sell.status = OrderStatus::FILLED;
    }
}

Trade OrderMatchingEngine::make_trade(const Order& buy, const Order& sell,
                                       double fill_price, double fill_qty) const {
    Trade t;
    t.buy_order_id  = buy.id;
    t.sell_order_id = sell.id;
    t.symbol        = buy.symbol;
    t.price         = fill_price;
    t.quantity      = fill_qty;
    t.time          = std::chrono::steady_clock::now();
    return t;
}

} // namespace trading
