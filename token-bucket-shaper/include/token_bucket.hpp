#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>

/**
 * @brief Thread-safe token bucket with lazy (on-demand) refill.
 *
 * Tokens are measured in bytes. The bucket starts full and is refilled
 * lazily from a monotonic clock every time it is queried.
 */
class TokenBucket {
public:
    /// Smallest legal burst size: one Ethernet MTU.
    static constexpr double kMinBurstBytes = 1500.0;

    /**
     * @param rate_bytes_per_sec Sustained fill rate; must be > 0.
     * @param burst_bytes        Bucket capacity; must be >= kMinBurstBytes.
     * @throws std::invalid_argument on invalid parameters.
     */
    TokenBucket(double rate_bytes_per_sec, double burst_bytes);

    /**
     * @brief Try to remove @p bytes tokens.
     * @return true if enough tokens were available (and were removed).
     */
    [[nodiscard]] bool consume(uint32_t bytes);

    /// Refill the bucket to capacity and restart the refill clock.
    void reset();

    /// Bucket capacity in bytes.
    [[nodiscard]] double capacity() const noexcept { return capacity_; }

    /// Currently available tokens (after a refill). Mainly for tests/diagnostics.
    [[nodiscard]] double tokens();

    /**
     * @brief Time until @p bytes tokens will be available.
     * @return zero if already available; a very large duration if
     *         @p bytes exceeds the bucket capacity (can never be satisfied).
     */
    [[nodiscard]] std::chrono::nanoseconds time_until(uint32_t bytes);

private:
    void refill();  // caller must hold mutex_

    double tokens_;
    const double capacity_;
    const double rate_;
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};
