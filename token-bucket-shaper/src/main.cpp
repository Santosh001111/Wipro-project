#include <signal.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include "config.hpp"
#include "nfqueue_capture.hpp"
#include "packet.hpp"
#include "policer.hpp"
#include "shaper.hpp"
#include "statistics.hpp"
#include "token_bucket.hpp"

namespace {
std::atomic<bool> g_running{true};

// Async-signal-safe: only touches a lock-free atomic.
void signal_handler(int) { g_running = false; }

void setup_signals() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

constexpr std::chrono::milliseconds kMainPollInterval{100};
}  // namespace

int main(int argc, char* argv[]) {
    try {
        Config config = Config::parse(argc, argv);
        if (config.help) {
            Config::print_usage(argv[0]);
            return 0;
        }
        const auto errors = config.validate();
        if (!errors.empty()) {
            for (const auto& e : errors) std::cerr << "Error: " << e << std::endl;
            std::cerr << "Try '" << argv[0] << " --help'." << std::endl;
            return 1;
        }
        config.print_banner();
        setup_signals();

        TokenBucket bucket(static_cast<double>(config.rate_bps) / 8.0,
                           static_cast<double>(config.burst_bytes));
        Statistics stats;
        stats.start(config.stats_interval_ms);

        // Declared before `capture` so they outlive it.
        std::unique_ptr<Policer> policer;
        std::unique_ptr<Shaper> shaper;
        if (config.mode == Config::Mode::POLICE) {
            policer = std::make_unique<Policer>(bucket, stats);
        } else {
            shaper = std::make_unique<Shaper>(bucket, stats, config.queue_depth);
        }

        const bool verbose = config.verbose;
        const bool police = config.mode == Config::Mode::POLICE;
        NFQueueCapture capture(
            config.queue_num,
            [&](uint32_t id, const uint8_t* data, uint32_t len, struct nfq_q_handle* qh) {
                if (verbose) std::fprintf(stderr, "[pkt] id=%u len=%u\n", id, len);
                Packet pkt{data, len, qh, id, std::chrono::steady_clock::now()};
                if (police) {
                    policer->process(pkt);
                } else {
                    shaper->enqueue(std::move(pkt));
                }
            });

        if (shaper) shaper->start();
        capture.start();

        while (g_running) std::this_thread::sleep_for(kMainPollInterval);

        // Ordered shutdown: no new packets -> drain queue -> final stats -> release NFQUEUE.
        capture.stop();
        if (shaper) shaper->stop();
        stats.stop();
        capture.close();

        std::cout << "[tbshaper] Clean shutdown complete." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
