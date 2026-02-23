#include "engine/OrderMatchingEngine.hpp"
#include "common/ThreadSafeQueue.hpp"
#include <cassert>
#include <iostream>
#include <cmath>
#include <atomic>

using namespace trading;

static uint64_t next_id = 1;

Order make_order(const std::string& sym, OrderType type, OrderSide side,
                 double price, double qty) {
    Order o;
    o.id       = next_id++;
    o.symbol   = sym;
    o.type     = type;
    o.side     = side;
    o.price    = price;
    o.quantity = qty;
    o.status   = OrderStatus::PENDING;
    return o;
}

// ── Test 1: Market BUY sweeps resting LIMIT SELL ──────────────────────────────
void test_market_buy_matches_limit_sell() {
    ThreadSafeQueue<Order>  oq;
    ThreadSafeQueue<Trade>  tq;
    OrderMatchingEngine engine(oq, tq);

    // Place a resting SELL limit @ 100
    auto sell = make_order("AAPL", OrderType::LIMIT, OrderSide::SELL, 100.0, 10.0);
    engine.process_order_direct(sell);

    // Market BUY for 5 shares
    auto buy = make_order("AAPL", OrderType::MARKET, OrderSide::BUY, 0.0, 5.0);
    engine.process_order_direct(buy);

    auto trade = tq.try_pop();
    assert(trade.has_value());
    assert(std::abs(trade->price - 100.0) < 1e-6);
    assert(std::abs(trade->quantity - 5.0) < 1e-6);
    std::cout << "  [PASS] Market buy matches limit sell @ "
              << trade->price << " qty=" << trade->quantity << "\n";
}

// ── Test 2: Limit BUY only matches if ask <= buy price ────────────────────────
void test_limit_buy_no_match_if_ask_too_high() {
    ThreadSafeQueue<Order>  oq;
    ThreadSafeQueue<Trade>  tq;
    OrderMatchingEngine engine(oq, tq);

    // Resting ask @ 105
    engine.process_order_direct(
        make_order("GOOG", OrderType::LIMIT, OrderSide::SELL, 105.0, 10.0));

    // Limit BUY @ 100 — should NOT match
    engine.process_order_direct(
        make_order("GOOG", OrderType::LIMIT, OrderSide::BUY, 100.0, 10.0));

    auto trade = tq.try_pop();
    assert(!trade.has_value()); // no fill expected
    std::cout << "  [PASS] Limit buy @ 100 does NOT match ask @ 105\n";
}

// ── Test 3: Limit BUY matches when bid >= ask ─────────────────────────────────
void test_limit_buy_matches_when_price_crosses() {
    ThreadSafeQueue<Order>  oq;
    ThreadSafeQueue<Trade>  tq;
    OrderMatchingEngine engine(oq, tq);

    // Resting ask @ 100
    engine.process_order_direct(
        make_order("TSLA", OrderType::LIMIT, OrderSide::SELL, 100.0, 5.0));

    // Limit BUY @ 102 — should match at ask price (100)
    engine.process_order_direct(
        make_order("TSLA", OrderType::LIMIT, OrderSide::BUY, 102.0, 5.0));

    auto trade = tq.try_pop();
    assert(trade.has_value());
    assert(std::abs(trade->price - 100.0) < 1e-6);
    std::cout << "  [PASS] Limit buy @ 102 matches ask @ 100, filled @ "
              << trade->price << "\n";
}

// ── Test 4: Partial fill — residual stays on book ─────────────────────────────
void test_partial_fill() {
    ThreadSafeQueue<Order>  oq;
    ThreadSafeQueue<Trade>  tq;
    OrderMatchingEngine engine(oq, tq);

    // Resting SELL 10 @ 100
    engine.process_order_direct(
        make_order("MSFT", OrderType::LIMIT, OrderSide::SELL, 100.0, 10.0));

    // Market BUY 6 — partial fill
    engine.process_order_direct(
        make_order("MSFT", OrderType::MARKET, OrderSide::BUY, 0.0, 6.0));

    auto trade = tq.try_pop();
    assert(trade.has_value());
    assert(std::abs(trade->quantity - 6.0) < 1e-6);

    // Remaining 4 should still be on book — place another buy
    engine.process_order_direct(
        make_order("MSFT", OrderType::MARKET, OrderSide::BUY, 0.0, 4.0));

    auto trade2 = tq.try_pop();
    assert(trade2.has_value());
    assert(std::abs(trade2->quantity - 4.0) < 1e-6);

    std::cout << "  [PASS] Partial fill: 6 filled, then remaining 4 filled\n";
}

// ── Test 5: Multiple symbols have independent books ───────────────────────────
void test_isolated_books() {
    ThreadSafeQueue<Order>  oq;
    ThreadSafeQueue<Trade>  tq;
    OrderMatchingEngine engine(oq, tq);

    // Place sell on AAPL only
    engine.process_order_direct(
        make_order("AAPL", OrderType::LIMIT, OrderSide::SELL, 100.0, 10.0));

    // Market buy on GOOG — different symbol, should NOT match AAPL's ask
    engine.process_order_direct(
        make_order("GOOG", OrderType::MARKET, OrderSide::BUY, 0.0, 5.0));

    auto trade = tq.try_pop();
    assert(!trade.has_value()); // GOOG book is empty
    std::cout << "  [PASS] Isolated order books per symbol\n";
}

int main() {
    std::cout << "=== OrderMatchingEngine Tests ===\n";
    test_market_buy_matches_limit_sell();
    test_limit_buy_no_match_if_ask_too_high();
    test_limit_buy_matches_when_price_crosses();
    test_partial_fill();
    test_isolated_books();
    std::cout << "All OrderMatchingEngine tests passed!\n";
    return 0;
}
