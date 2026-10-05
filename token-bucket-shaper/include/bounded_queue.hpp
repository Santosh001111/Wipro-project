#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

/**
 * @brief Thread-safe bounded FIFO queue (header-only template).
 *
 * After shutdown(): blocked callers wake up, push()/try_push() reject new
 * items, and pop() keeps returning the remaining items until the queue is
 * empty, then returns std::nullopt.
 */
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity) : capacity_(capacity), shutdown_(false) {}

    /// Blocking push. Waits while full. Discards the item if shut down.
    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < capacity_ || shutdown_; });
        if (shutdown_) return;
        queue_.push_back(std::move(item));
        not_empty_.notify_one();
    }

    /**
     * @brief Non-blocking push (rvalue). On failure @p item is left untouched,
     *        so the caller still owns it (e.g. to issue a drop verdict).
     */
    [[nodiscard]] bool try_push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_ || queue_.size() >= capacity_) return false;
        queue_.push_back(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    /// Non-blocking push (copy).
    [[nodiscard]] bool try_push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_ || queue_.size() >= capacity_) return false;
        queue_.push_back(item);
        not_empty_.notify_one();
        return true;
    }

    /// Blocking pop. Returns std::nullopt only after shutdown() and once empty.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return item;
    }

    /// Wake every waiter and refuse further pushes.
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::deque<T> queue_;
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::atomic<bool> shutdown_;
};
