#include "common/ThreadSafeQueue.hpp"
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

using namespace trading;

// ── Test 1: Basic push/pop ────────────────────────────────────────────────────
void test_basic_push_pop() {
    ThreadSafeQueue<int> q;
    q.push(42);
    auto val = q.try_pop();
    assert(val.has_value());
    assert(*val == 42);
    assert(q.empty());
    std::cout << "  [PASS] Basic push/pop\n";
}

// ── Test 2: Blocking pop returns after push ───────────────────────────────────
void test_blocking_pop() {
    ThreadSafeQueue<int> q;
    std::atomic<bool> received{false};

    std::thread consumer([&] {
        auto val = q.pop(std::chrono::milliseconds(500));
        assert(val.has_value());
        assert(*val == 99);
        received = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.push(99);
    consumer.join();
    assert(received);
    std::cout << "  [PASS] Blocking pop\n";
}

// ── Test 3: Multi-producer / multi-consumer correctness ───────────────────────
void test_mpmc() {
    ThreadSafeQueue<int> q;
    const int N = 1000;
    std::atomic<int> sum_produced{0};
    std::atomic<int> sum_consumed{0};

    // 4 producers
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&, i] {
            for (int j = i * (N / 4); j < (i + 1) * (N / 4); ++j) {
                q.push(j);
                sum_produced += j;
            }
        });
    }

    // 4 consumers
    std::atomic<bool> stop{false};
    std::vector<std::thread> consumers;
    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back([&] {
            while (!stop || !q.empty()) {
                auto val = q.try_pop();
                if (val) sum_consumed += *val;
                else std::this_thread::yield();
            }
        });
    }

    for (auto& t : producers) t.join();
    stop = true;
    for (auto& t : consumers) t.join();

    assert(sum_produced == sum_consumed);
    std::cout << "  [PASS] MPMC correctness (sum=" << sum_consumed << ")\n";
}

// ── Test 4: Shutdown unblocks waiting consumer ────────────────────────────────
void test_shutdown_unblocks() {
    ThreadSafeQueue<int> q;
    std::atomic<bool> exited{false};

    std::thread consumer([&] {
        auto val = q.pop(std::chrono::seconds(10));
        assert(!val.has_value()); // shutdown, no value
        exited = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.shutdown();
    consumer.join();
    assert(exited);
    std::cout << "  [PASS] Shutdown unblocks consumer\n";
}

// ── Test 5: try_pop on empty queue ────────────────────────────────────────────
void test_try_pop_empty() {
    ThreadSafeQueue<std::string> q;
    auto val = q.try_pop();
    assert(!val.has_value());
    std::cout << "  [PASS] try_pop on empty queue\n";
}

int main() {
    std::cout << "=== ThreadSafeQueue Tests ===\n";
    test_basic_push_pop();
    test_blocking_pop();
    test_mpmc();
    test_shutdown_unblocks();
    test_try_pop_empty();
    std::cout << "All ThreadSafeQueue tests passed!\n";
    return 0;
}
