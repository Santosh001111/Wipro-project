#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// Runtime configuration, filled from the command line.
struct Config {
    enum class Mode { POLICE, SHAPE };

    uint64_t rate_bps{0};      ///< bits per second (K/M/G are decimal: 1M = 1,000,000)
    uint64_t burst_bytes{0};   ///< bytes (K/M/G are binary: 64K = 65,536)
    uint16_t queue_num{0};
    Mode mode{Mode::POLICE};
    size_t queue_depth{1024};
    uint32_t stats_interval_ms{5000};
    bool verbose{false};
    bool help{false};          ///< --help was given; caller should print usage and exit 0

    /// @throws std::invalid_argument on malformed arguments.
    static Config parse(int argc, char* argv[]);

    /// @return human-readable problems; empty if the configuration is valid.
    [[nodiscard]] std::vector<std::string> validate() const;

    void print_banner() const;
    static void print_usage(const char* prog);

    /// Parse "N", "NK", "NM", "NG" (case-insensitive). @p base is 1000 or 1024.
    /// @throws std::invalid_argument on garbage, negatives, or overflow.
    static uint64_t parse_size(const std::string& str, uint64_t base);
};
