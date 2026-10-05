#pragma once

#include <atomic>
#include <cstddef>
#include <thread>

#include "bounded_queue.hpp"
#include "packet.hpp"
#include "statistics.hpp"
#include "token_bucket.hpp"

/**
 * @brief Shaping mode: excess packets are queued and released at the
 * configured rate by a dedicated pacing thread.
 */
class Shaper {
public:
    Shaper(TokenBucket& bucket, Statistics& stats, size_t queue_depth)
        : bucket_(bucket), queue_(queue_depth), stats_(stats), running_(false) {}
    ~Shaper();
    Shaper(const Shaper&) = delete;
    Shaper& operator=(const Shaper&) = delete;

    /**
     * @brief Hot path (capture thread). Buffers @p pkt, or drops it immediately
     * if the queue is full or it is larger than the bucket (it could never pass).
     */
    void enqueue(Packet pkt);

    void start();
    /// Stop pacing; packets still queued are flushed with NF_ACCEPT, so no
    /// packet is left without a verdict.
    void stop();

private:
    void shaper_loop();

    TokenBucket& bucket_;
    BoundedQueue<Packet> queue_;
    Statistics& stats_;
    std::thread shaper_thread_;
    std::atomic<bool> running_;
};
