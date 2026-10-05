#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

/**
 * @brief Lock-free packet/byte counters plus a periodic reporter thread.
 */
class Statistics {
public:
    Statistics() = default;
    ~Statistics();
    Statistics(const Statistics&) = delete;
    Statistics& operator=(const Statistics&) = delete;

    void record_accept(uint32_t bytes) noexcept {
        packets_accepted_.fetch_add(1, std::memory_order_relaxed);
        bytes_accepted_.fetch_add(bytes, std::memory_order_relaxed);
    }
    void record_drop(uint32_t bytes) noexcept {
        packets_dropped_.fetch_add(1, std::memory_order_relaxed);
        bytes_dropped_.fetch_add(bytes, std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t packets_accepted() const noexcept {
        return packets_accepted_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint64_t packets_dropped() const noexcept {
        return packets_dropped_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint64_t bytes_accepted() const noexcept {
        return bytes_accepted_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint64_t bytes_dropped() const noexcept {
        return bytes_dropped_.load(std::memory_order_relaxed);
    }

    /// Start the reporter thread (prints every @p interval_ms).
    void start(uint32_t interval_ms = 5000);
    /// Stop the reporter (returns promptly) and print one final report.
    void stop();
    /// Print one line: pass / drop / average throughput since start().
    void report() const;

private:
    void run();

    std::atomic<uint64_t> packets_accepted_{0};
    std::atomic<uint64_t> packets_dropped_{0};
    std::atomic<uint64_t> bytes_accepted_{0};
    std::atomic<uint64_t> bytes_dropped_{0};

    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
    std::thread stats_thread_;
    bool running_{false};  // guarded by wake_mutex_
    std::mutex wake_mutex_;
    std::condition_variable wake_cv_;
    uint32_t interval_ms_{5000};
    bool started_{false};
};
