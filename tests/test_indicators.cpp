#include "strategy/IndicatorEngine.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace trading;

static bool approx_eq(double a, double b, double eps = 0.01) {
    return std::abs(a - b) < eps;
}

// ── Test 1: SMA correctness ───────────────────────────────────────────────────
void test_sma() {
    IndicatorEngine::instance().init(3, 3, 3); // period=3 for fast testing

    // Feed 3 prices: SMA should = (10+20+30)/3 = 20
    IndicatorEngine::instance().update("TEST_SMA", 10.0);
    IndicatorEngine::instance().update("TEST_SMA", 20.0);
    auto snap = IndicatorEngine::instance().update("TEST_SMA", 30.0);

    assert(snap.sma_valid);
    assert(approx_eq(snap.sma, 20.0));
    std::cout << "  [PASS] SMA = " << snap.sma << " (expected 20.0)\n";

    // Feed a 4th price: sliding window → (20+30+40)/3 = 30
    snap = IndicatorEngine::instance().update("TEST_SMA", 40.0);
    assert(approx_eq(snap.sma, 30.0));
    std::cout << "  [PASS] SMA sliding window = " << snap.sma << " (expected 30.0)\n";
}

// ── Test 2: EMA seeded from SMA ───────────────────────────────────────────────
void test_ema() {
    IndicatorEngine::instance().init(3, 3, 3);

    IndicatorEngine::instance().update("TEST_EMA", 10.0);
    IndicatorEngine::instance().update("TEST_EMA", 20.0);
    auto snap = IndicatorEngine::instance().update("TEST_EMA", 30.0);

    // After 3 ticks, EMA should be seeded to SMA = 20
    assert(snap.ema_valid);
    assert(approx_eq(snap.ema, 20.0));
    std::cout << "  [PASS] EMA seeded from SMA = " << snap.ema << "\n";

    // 4th price = 40, k = 2/(3+1) = 0.5
    // EMA = 40*0.5 + 20*0.5 = 30
    snap = IndicatorEngine::instance().update("TEST_EMA", 40.0);
    assert(approx_eq(snap.ema, 30.0, 0.1));
    std::cout << "  [PASS] EMA after 40: " << snap.ema << " (expected ~30.0)\n";
}

// ── Test 3: RSI — constant rise → RSI → 100 ──────────────────────────────────
void test_rsi_trending_up() {
    IndicatorEngine::instance().init(14, 14, 3); // period=3 for RSI

    // 4 rising prices → no losses → RSI → 100
    IndicatorEngine::instance().update("TEST_RSI_UP", 100.0);
    IndicatorEngine::instance().update("TEST_RSI_UP", 101.0);
    IndicatorEngine::instance().update("TEST_RSI_UP", 102.0);
    auto snap = IndicatorEngine::instance().update("TEST_RSI_UP", 103.0);

    assert(snap.rsi_valid);
    assert(snap.rsi > 90.0); // purely rising → should be high
    std::cout << "  [PASS] RSI (all gains) = " << snap.rsi << " > 90\n";
}

// ── Test 4: RSI — constant fall → RSI → 0 ────────────────────────────────────
void test_rsi_trending_down() {
    IndicatorEngine::instance().init(14, 14, 3);

    IndicatorEngine::instance().update("TEST_RSI_DN", 100.0);
    IndicatorEngine::instance().update("TEST_RSI_DN", 99.0);
    IndicatorEngine::instance().update("TEST_RSI_DN", 98.0);
    auto snap = IndicatorEngine::instance().update("TEST_RSI_DN", 97.0);

    assert(snap.rsi_valid);
    assert(snap.rsi < 10.0); // purely falling → should be low
    std::cout << "  [PASS] RSI (all losses) = " << snap.rsi << " < 10\n";
}

// ── Test 5: Warmup period respected ─────────────────────────────────────────
void test_warmup() {
    IndicatorEngine::instance().init(5, 5, 5);

    auto snap = IndicatorEngine::instance().update("TEST_WARM", 100.0);
    assert(!snap.sma_valid); // not ready yet
    assert(!snap.ema_valid);
    std::cout << "  [PASS] Warmup: indicators not valid before period\n";
}

int main() {
    std::cout << "=== IndicatorEngine Tests ===\n";
    test_sma();
    test_ema();
    test_rsi_trending_up();
    test_rsi_trending_down();
    test_warmup();
    std::cout << "All IndicatorEngine tests passed!\n";
    return 0;
}
