#include "shaper.hpp"

#include <algorithm>
#include <chrono>

namespace {
constexpr std::chrono::microseconds kMinSleep{50};
// Upper bound per sleep so stop() is noticed quickly.
constexpr std::chrono::milliseconds kMaxSleep{10};
}  // namespace

Shaper::~Shaper() { stop(); }

void Shaper::enqueue(Packet pkt) {
    const uint32_t len = pkt.length();
    // A packet bigger than the bucket can never accumulate enough tokens and
    // would block the head of the queue forever.
    if (static_cast<double>(len) > bucket_.capacity() || !queue_.try_push(std::move(pkt))) {
        pkt.drop();  // try_push(T&&) leaves pkt intact on failure
        stats_.record_drop(len);
    }
}

void Shaper::start() {
    if (running_.exchange(true)) return;
    shaper_thread_ = std::thread(&Shaper::shaper_loop, this);
}

void Shaper::stop() {
    running_ = false;
    queue_.shutdown();
    if (shaper_thread_.joinable()) shaper_thread_.join();
}

void Shaper::shaper_loop() {
    // pop() keeps returning queued items after shutdown(), so the queue is
    // always drained and every packet gets a verdict.
    while (auto opt = queue_.pop()) {
        Packet pkt = std::move(*opt);
        const uint32_t len = pkt.length();
        while (!bucket_.consume(len)) {
            if (!running_) break;  // shutting down: stop pacing, flush below
            auto wait = std::clamp<std::chrono::nanoseconds>(bucket_.time_until(len), kMinSleep,
                                                             kMaxSleep);
            std::this_thread::sleep_for(wait);
        }
        pkt.accept();
        stats_.record_accept(len);
    }
}
