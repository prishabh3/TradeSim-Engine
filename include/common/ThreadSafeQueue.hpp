#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <cstddef>

namespace trading {

/**
 * Thread-safe MPMC queue using mutex + condition_variable.
 *
 * Producers call push()   — non-blocking, O(1)
 * Consumers call pop()    — blocking with timeout (avoids spin)
 *              try_pop()  — non-blocking (for hot-path consumers)
 *
 * Graceful shutdown: call shutdown() to unblock all waiting consumers.
 */
template <typename T>
class ThreadSafeQueue {
public:
    // ── Producer ────────────────────────────────────────────────────────────
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_) return;
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    // ── Consumer: blocking with timeout ─────────────────────────────────────
    std::optional<T> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, timeout, [this] {
            return !queue_.empty() || shutdown_;
        });

        if (queue_.empty()) return std::nullopt;

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // ── Consumer: non-blocking (hot path, avoids CV overhead) ───────────────
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // ── Lifecycle ────────────────────────────────────────────────────────────
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) queue_.pop();
        shutdown_ = false;
    }

    // ── Observers ────────────────────────────────────────────────────────────
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    bool is_shutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::queue<T>           queue_;
    bool                    shutdown_ = false;
};

} // namespace trading
