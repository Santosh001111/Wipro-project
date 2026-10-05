#include "config.hpp"

#include <getopt.h>

#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "token_bucket.hpp"

namespace {
const struct option kLongOptions[] = {{"rate", required_argument, nullptr, 'r'},
                                      {"burst", required_argument, nullptr, 'b'},
                                      {"mode", required_argument, nullptr, 'm'},
                                      {"queue", required_argument, nullptr, 'q'},
                                      {"queue-depth", required_argument, nullptr, 'd'},
                                      {"stats-interval", required_argument, nullptr, 'i'},
                                      {"verbose", no_argument, nullptr, 'v'},
                                      {"help", no_argument, nullptr, 'h'},
                                      {nullptr, 0, nullptr, 0}};

uint64_t parse_uint(const std::string& s, const char* what, uint64_t max) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(std::string("invalid value for ") + what + ": '" + s + "'");
    }
    errno = 0;
    const unsigned long long v = std::strtoull(s.c_str(), nullptr, 10);
    if (errno == ERANGE || v > max) {
        throw std::invalid_argument(std::string("value out of range for ") + what + ": '" + s +
                                    "'");
    }
    return v;
}
}  // namespace

uint64_t Config::parse_size(const std::string& str, uint64_t base) {
    uint64_t mult = 1;
    std::string digits = str;
    if (!str.empty()) {
        switch (str.back()) {
            case 'K': case 'k': mult = base; break;
            case 'M': case 'm': mult = base * base; break;
            case 'G': case 'g': mult = base * base * base; break;
            default: break;
        }
        if (mult != 1) digits.pop_back();
    }
    const uint64_t n = parse_uint(digits, "size", std::numeric_limits<uint64_t>::max());
    if (n != 0 && mult > std::numeric_limits<uint64_t>::max() / n) {
        throw std::invalid_argument("size overflows: '" + str + "'");
    }
    return n * mult;
}

Config Config::parse(int argc, char* argv[]) {
    Config cfg;
    optind = 1;  // allow parse() to be called more than once (tests)
    int opt;
    while ((opt = getopt_long(argc, argv, "r:b:m:q:d:i:vh", kLongOptions, nullptr)) != -1) {
        switch (opt) {
            case 'r': cfg.rate_bps = parse_size(optarg, 1000); break;
            case 'b': cfg.burst_bytes = parse_size(optarg, 1024); break;
            case 'm': {
                const std::string m(optarg);
                if (m == "police") cfg.mode = Mode::POLICE;
                else if (m == "shape") cfg.mode = Mode::SHAPE;
                else throw std::invalid_argument("invalid mode '" + m + "' (use police or shape)");
                break;
            }
            case 'q': cfg.queue_num = static_cast<uint16_t>(parse_uint(optarg, "--queue", 65535)); break;
            case 'd': cfg.queue_depth = parse_uint(optarg, "--queue-depth", 100'000'000); break;
            case 'i': cfg.stats_interval_ms = static_cast<uint32_t>(parse_uint(optarg, "--stats-interval", 3'600'000)); break;
            case 'v': cfg.verbose = true; break;
            case 'h': cfg.help = true; return cfg;
            default: throw std::invalid_argument("unknown or malformed option");
        }
    }
    if (optind < argc) {
        throw std::invalid_argument(std::string("unexpected argument: ") + argv[optind]);
    }
    return cfg;
}

std::vector<std::string> Config::validate() const {
    std::vector<std::string> errs;
    if (rate_bps == 0) errs.emplace_back("--rate is required and must be > 0");
    if (static_cast<double>(burst_bytes) < TokenBucket::kMinBurstBytes) {
        errs.emplace_back("--burst must be >= 1500 bytes (burst must be >= MTU)");
    }
    if (stats_interval_ms < 100) errs.emplace_back("--stats-interval must be >= 100 ms");
    if (queue_depth == 0) errs.emplace_back("--queue-depth must be >= 1");
    return errs;
}

void Config::print_banner() const {
    std::ostringstream os;
    os << "[tbshaper] Mode           : " << (mode == Mode::SHAPE ? "SHAPING" : "POLICING") << '\n'
       << "[tbshaper] Rate           : " << std::fixed << std::setprecision(3)
       << static_cast<double>(rate_bps) / 1'000'000.0 << " Mbit/s\n"
       << "[tbshaper] Burst          : " << burst_bytes << " bytes\n"
       << "[tbshaper] Queue Number   : " << queue_num << '\n'
       << "[tbshaper] Queue Depth    : " << queue_depth << " packets\n"
       << "[tbshaper] Stats Interval : " << stats_interval_ms << " ms\n";
    std::cout << os.str() << std::flush;
}

void Config::print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " --rate <N[K/M/G]> --burst <N[K/M/G]> [options]\n"
              << "Options:\n"
              << "  -r, --rate <N[K/M/G]>      Rate in bits/sec, decimal suffixes (1M = 1,000,000)\n"
              << "  -b, --burst <N[K/M/G]>     Burst in bytes, binary suffixes (64K = 65,536), >= 1500\n"
              << "  -m, --mode police|shape    police (default) drops excess, shape delays it\n"
              << "  -q, --queue <N>            NFQUEUE number (default: 0)\n"
              << "  -d, --queue-depth <N>      Shaping queue depth in packets (default: 1024)\n"
              << "  -i, --stats-interval <ms>  Stats interval, >= 100 (default: 5000)\n"
              << "  -v, --verbose              Print one line per packet to stderr\n"
              << "  -h, --help                 Show this help\n";
}
