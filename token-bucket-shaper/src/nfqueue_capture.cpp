#include "nfqueue_capture.hpp"

#include <arpa/inet.h>
#include <linux/netfilter.h>
#include <poll.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr int kRcvBufBytes = 4 * 1024 * 1024;   // SO_RCVBUF: absorb bursts
constexpr uint32_t kCopyRange = 0xffff;         // full packet (real length is needed)
// Loopback MTU is 65536, so a netlink message can be ~64 KiB + headers.
// A smaller recv buffer would truncate such packets and stall the flow.
constexpr size_t kRecvBufBytes = 65536 + 4096;
constexpr int kPollTimeoutMs = 100;             // how often the loop checks running_

[[noreturn]] void fail(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno) +
                             " (are you root / do you have CAP_NET_ADMIN?)");
}
}  // namespace

NFQueueCapture::NFQueueCapture(uint16_t queue_num, PacketHandler handler)
    : queue_num_(queue_num), handler_(std::move(handler)) {
    nfq_h_ = nfq_open();
    if (!nfq_h_) fail("nfq_open failed");

    try {
        if (nfq_unbind_pf(nfq_h_, AF_INET) < 0) fail("nfq_unbind_pf failed");
        if (nfq_bind_pf(nfq_h_, AF_INET) < 0) fail("nfq_bind_pf failed");

        nfq_qh_ = nfq_create_queue(nfq_h_, queue_num_, &callback, this);
        if (!nfq_qh_) fail("nfq_create_queue failed (is another process using this queue?)");

        if (nfq_set_mode(nfq_qh_, NFQNL_COPY_PACKET, kCopyRange) < 0) fail("nfq_set_mode failed");

        int rcvbuf = kRcvBufBytes;
        if (setsockopt(nfq_fd(nfq_h_), SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
            std::cerr << "[warn] could not set SO_RCVBUF: " << std::strerror(errno) << std::endl;
        }
    } catch (...) {
        close();
        throw;
    }
}

NFQueueCapture::~NFQueueCapture() { close(); }

void NFQueueCapture::start() {
    if (running_.exchange(true)) return;
    capture_thread_ = std::thread(&NFQueueCapture::recv_loop, this);
}

void NFQueueCapture::stop() {
    running_ = false;
    if (capture_thread_.joinable()) capture_thread_.join();
}

void NFQueueCapture::close() {
    stop();
    if (nfq_qh_) {
        nfq_destroy_queue(nfq_qh_);
        nfq_qh_ = nullptr;
    }
    if (nfq_h_) {
        nfq_close(nfq_h_);
        nfq_h_ = nullptr;
    }
}

int NFQueueCapture::callback(struct nfq_q_handle* qh, struct nfgenmsg*, struct nfq_data* nfa,
                             void* data) {
    auto* self = static_cast<NFQueueCapture*>(data);

    struct nfqnl_msg_packet_hdr* ph = nfq_get_msg_packet_hdr(nfa);
    if (!ph) return -1;
    const uint32_t id = ntohl(ph->packet_id);

    unsigned char* payload = nullptr;
    const int len = nfq_get_payload(nfa, &payload);
    if (len < 0) {
        // Never leave a packet without a verdict.
        nfq_set_verdict(qh, id, NF_ACCEPT, 0, nullptr);
        return -1;
    }
    self->handler_(id, payload, static_cast<uint32_t>(len), qh);
    return 0;
}

void NFQueueCapture::recv_loop() {
    std::vector<char> buf(kRecvBufBytes);
    const int fd = nfq_fd(nfq_h_);
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (running_) {
        // poll() with a timeout so stop() works even when no packets arrive.
        const int pr = poll(&pfd, 1, kPollTimeoutMs);
        if (pr == 0) continue;
        if (pr < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[error] poll: " << std::strerror(errno) << std::endl;
            break;
        }
        const ssize_t rv = recv(fd, buf.data(), buf.size(), 0);
        if (rv < 0) {
            if (errno == ENOBUFS) {
                std::cerr << "[warn] ENOBUFS: packets were dropped by the kernel queue" << std::endl;
                continue;
            }
            if (errno == EINTR || errno == EAGAIN) continue;
            std::cerr << "[error] recv: " << std::strerror(errno) << std::endl;
            break;
        }
        nfq_handle_packet(nfq_h_, buf.data(), static_cast<int>(rv));
    }
}
