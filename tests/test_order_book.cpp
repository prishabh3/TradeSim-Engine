#include "engine/OrderBook.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace trading;

static uint64_t next_id = 1;

Order make_order(OrderSide side, double price, double qty) {
    Order o;
    o.id       = next_id++;
    o.symbol   = "TEST";
    o.type     = OrderType::LIMIT;
    o.side     = side;
    o.price    = price;
    o.quantity = qty;
    o.status   = OrderStatus::PENDING;
    return o;
}

// ── Test 1: Best bid = highest bid ────────────────────────────────────────────
void test_best_bid() {
    OrderBook book("TEST");
    book.add_order(make_order(OrderSide::BUY, 100.0, 10));
    book.add_order(make_order(OrderSide::BUY, 102.0, 5));
    book.add_order(make_order(OrderSide::BUY, 101.0, 8));

    auto best = book.best_bid();
    assert(best.has_value());
    assert(best->price == 102.0); // highest bid
    std::cout << "  [PASS] Best bid = " << best->price << " (expected 102.0)\n";
}

// ── Test 2: Best ask = lowest ask ─────────────────────────────────────────────
void test_best_ask() {
    OrderBook book("TEST");
    book.add_order(make_order(OrderSide::SELL, 105.0, 10));
    book.add_order(make_order(OrderSide::SELL, 103.0, 5));
    book.add_order(make_order(OrderSide::SELL, 106.0, 8));

    auto best = book.best_ask();
    assert(best.has_value());
    assert(best->price == 103.0); // lowest ask
    std::cout << "  [PASS] Best ask = " << best->price << " (expected 103.0)\n";
}

// ── Test 3: Spread calculation ─────────────────────────────────────────────────
void test_spread() {
    OrderBook book("SPREAD");
    book.add_order(make_order(OrderSide::BUY,  100.0, 10));
    book.add_order(make_order(OrderSide::SELL, 102.0, 10));

    double spread = book.spread();
    assert(std::abs(spread - 2.0) < 1e-6);
    std::cout << "  [PASS] Spread = " << spread << " (expected 2.0)\n";
}

// ── Test 4: pop_best_bid removes top ──────────────────────────────────────────
void test_pop_bid() {
    OrderBook book("POP");
    book.add_order(make_order(OrderSide::BUY, 100.0, 10));
    book.add_order(make_order(OrderSide::BUY, 105.0, 5));

    book.pop_best_bid();
    auto best = book.best_bid();
    assert(best.has_value());
    assert(best->price == 100.0); // 105 was popped
    std::cout << "  [PASS] pop_best_bid removes highest\n";
}

// ── Test 5: Empty book returns nullopt ────────────────────────────────────────
void test_empty() {
    OrderBook book("EMPTY");
    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());
    assert(book.bids_empty());
    assert(book.asks_empty());
    std::cout << "  [PASS] Empty book returns nullopt\n";
}

// ── Test 6: top_bids returns correct count ──────────────────────────────────
void test_top_n() {
    OrderBook book("TOPN");
    for (int i = 1; i <= 7; ++i)
        book.add_order(make_order(OrderSide::BUY, (double)i * 10, 5));

    auto top = book.top_bids(3);
    assert(top.size() == 3);
    assert(top[0].price == 70.0); // highest first
    assert(top[1].price == 60.0);
    assert(top[2].price == 50.0);
    std::cout << "  [PASS] top_bids(3) returns correct order\n";
}

int main() {
    std::cout << "=== OrderBook Tests ===\n";
    test_best_bid();
    test_best_ask();
    test_spread();
    test_pop_bid();
    test_empty();
    test_top_n();
    std::cout << "All OrderBook tests passed!\n";
    return 0;
}
