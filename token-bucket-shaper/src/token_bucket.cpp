#include "token_bucket.hpp"

#include <algorithm>
#include <limits>

TokenBucket::TokenBucket(double rate_bytes_per_sec, double burst_bytes)
    : tokens_(burst_bytes),
      capacity_(burst_bytes),
      rate_(rate_bytes_per_sec),
      last_refill_(std::chrono::steady_clock::now()) {
    if (!(rate_bytes_per_sec > 0.0)) {
        throw std::invalid_argument("rate must be > 0");
    }
    if (!(burst_bytes >= kMinBurstBytes)) {
        throw std::invalid_argument("burst must be >= 1500 bytes (MTU)");
    }
}

void TokenBucket::refill() {
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - last_refill_).count();
    tokens_ = std::min(tokens_ + dt * rate_, capacity_);
    last_refill_ = now;
}

bool TokenBucket::consume(uint32_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill();
    const double need = static_cast<double>(bytes);
    if (tokens_ >= need) {
        tokens_ -= need;
        return true;
    }
    return false;
}

void TokenBucket::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_ = capacity_;
    last_refill_ = std::chrono::steady_clock::now();
}

double TokenBucket::tokens() {
    std::lock_guard<std::mutex> lock(mutex_);
    refill();
    return tokens_;
}

std::chrono::nanoseconds TokenBucket::time_until(uint32_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill();
    const double need = static_cast<double>(bytes);
    if (need > capacity_) return std::chrono::nanoseconds::max();
    if (tokens_ >= need) return std::chrono::nanoseconds::zero();
    const double secs = (need - tokens_) / rate_;
    return std::chrono::nanoseconds(static_cast<int64_t>(secs * 1e9) + 1);
}
