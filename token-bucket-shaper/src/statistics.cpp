#include "statistics.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

Statistics::~Statistics() {
    // Never let a joinable thread be destroyed (would call std::terminate),
    // e.g. when main() unwinds because of an exception. No final report here.
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        running_ = false;
    }
    wake_cv_.notify_all();
    if (stats_thread_.joinable()) stats_thread_.join();
}

void Statistics::start(uint32_t interval_ms) {
    if (started_) return;
    started_ = true;
    interval_ms_ = interval_ms;
    start_time_ = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        running_ = true;
    }
    stats_thread_ = std::thread(&Statistics::run, this);
}

void Statistics::stop() {
    if (!started_) return;
    started_ = false;
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        running_ = false;
    }
    wake_cv_.notify_all();
    if (stats_thread_.joinable()) stats_thread_.join();
    report();
}

void Statistics::report() const {
    const double elapsed =
        std::max(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_)
                     .count(),
                 0.001);
    const uint64_t bytes_acc = bytes_accepted();
    const double mbps = (static_cast<double>(bytes_acc) * 8.0) / (elapsed * 1'000'000.0);

    std::ostringstream os;  // single write => lines never interleave
    os << "[Stats " << std::setw(3) << static_cast<long>(elapsed) << "s] pass=" << packets_accepted()
       << "  drop=" << packets_dropped() << "  bytes_drop=" << bytes_dropped()
       << "  throughput=" << std::fixed << std::setprecision(3) << mbps << " Mbit/s\n";
    std::cout << os.str() << std::flush;
}

void Statistics::run() {
    std::unique_lock<std::mutex> lock(wake_mutex_);
    while (running_) {
        // Wakes immediately on stop() instead of sleeping out the interval.
        if (wake_cv_.wait_for(lock, std::chrono::milliseconds(interval_ms_),
                              [this] { return !running_; })) {
            break;
        }
        lock.unlock();
        report();
        lock.lock();
    }
}
