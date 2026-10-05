#pragma once

#include <chrono>
#include <cstdint>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

/**
 * @brief One packet held by NFQUEUE awaiting a verdict.
 *
 * The payload pointer is NOT owned and is only valid inside the NFQUEUE
 * callback; do not dereference it after the callback returns. A Packet is
 * move-only; a moved-from Packet has no queue handle and its verdict calls
 * are no-ops, so a verdict can never be issued twice from stale copies.
 */
struct Packet {
    const uint8_t* data_{nullptr};
    uint32_t length_{0};
    struct nfq_q_handle* qh_{nullptr};
    uint32_t id_{0};
    std::chrono::steady_clock::time_point arrival_{};

    Packet() = default;
    Packet(const uint8_t* data, uint32_t length, struct nfq_q_handle* qh, uint32_t id,
           std::chrono::steady_clock::time_point arrival)
        : data_(data), length_(length), qh_(qh), id_(id), arrival_(arrival) {}

    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;
    Packet(Packet&& other) noexcept;
    Packet& operator=(Packet&& other) noexcept;

    [[nodiscard]] uint32_t length() const noexcept { return length_; }
    [[nodiscard]] uint32_t id() const noexcept { return id_; }
    [[nodiscard]] std::chrono::steady_clock::time_point arrival() const noexcept {
        return arrival_;
    }

    /// Issue NF_ACCEPT. Logs to stderr on failure. @return true on success.
    bool accept();
    /// Issue NF_DROP. Logs to stderr on failure. @return true on success.
    bool drop();

private:
    bool verdict(uint32_t verdict, const char* name);
};
