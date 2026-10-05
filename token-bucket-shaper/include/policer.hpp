#pragma once

#include "packet.hpp"
#include "statistics.hpp"
#include "token_bucket.hpp"

/**
 * @brief Policing mode: conforming packets are accepted, excess is dropped.
 * Runs on the capture thread, so it never blocks.
 */
class Policer {
public:
    Policer(TokenBucket& bucket, Statistics& stats) : bucket_(bucket), stats_(stats) {}

    /// Decide and issue the verdict for @p pkt.
    void process(Packet& pkt);

private:
    TokenBucket& bucket_;
    Statistics& stats_;
};
