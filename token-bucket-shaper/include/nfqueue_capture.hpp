#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

#include <libnetfilter_queue/libnetfilter_queue.h>

/**
 * @brief Owns an NFQUEUE binding and the thread that receives its packets.
 * RAII: the destructor stops the thread and releases the queue handles.
 */
class NFQueueCapture {
public:
    using PacketHandler = std::function<void(uint32_t id, const uint8_t* data, uint32_t len,
                                             struct nfq_q_handle* qh)>;

    /// @throws std::runtime_error if the queue cannot be opened (e.g. not root).
    NFQueueCapture(uint16_t queue_num, PacketHandler handler);
    ~NFQueueCapture();
    NFQueueCapture(const NFQueueCapture&) = delete;
    NFQueueCapture& operator=(const NFQueueCapture&) = delete;

    void start();
    /// Stop receiving. Returns within ~one poll interval even if the link is idle.
    void stop();
    /// stop() + nfq_destroy_queue() + nfq_close(). Idempotent.
    void close();

private:
    static int callback(struct nfq_q_handle* qh, struct nfgenmsg* nfmsg, struct nfq_data* nfa,
                        void* data);
    void recv_loop();

    struct nfq_handle* nfq_h_{nullptr};
    struct nfq_q_handle* nfq_qh_{nullptr};
    std::thread capture_thread_;
    std::atomic<bool> running_{false};
    uint16_t queue_num_;
    PacketHandler handler_;
};
