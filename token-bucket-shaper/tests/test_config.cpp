#include <iostream>
#include <string>
#include <vector>

#include "config.hpp"
#include "test_util.hpp"

static Config parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    static std::string prog = "tbshaper";
    argv.push_back(prog.data());
    for (auto& a : args) argv.push_back(a.data());
    return Config::parse(static_cast<int>(argv.size()), argv.data());
}
static bool throws(std::vector<std::string> args) {
    try { parse(std::move(args)); } catch (const std::invalid_argument&) { return true; }
    return false;
}

int main() {
    auto c = parse({"--rate", "1M", "--burst", "64K"});
    CHECK(c.rate_bps == 1'000'000);      // spec 9.2: --rate 1M  -> 1,000,000 bps
    CHECK(c.burst_bytes == 65'536);      // spec 9.2: --burst 64K -> 65,536 bytes
    CHECK(c.mode == Config::Mode::POLICE && c.queue_num == 0 && c.queue_depth == 1024);
    CHECK(c.stats_interval_ms == 5000 && c.validate().empty());

    c = parse({"--mode", "shape", "--rate", "512k", "--burst", "1500", "--queue", "3",
               "--queue-depth", "512", "--stats-interval", "1000", "--verbose"});
    CHECK(c.mode == Config::Mode::SHAPE && c.rate_bps == 512'000 && c.burst_bytes == 1500);
    CHECK(c.queue_num == 3 && c.queue_depth == 512 && c.stats_interval_ms == 1000 && c.verbose);
    CHECK(Config::parse_size("2G", 1000) == 2'000'000'000ULL);

    CHECK(parse({"--help"}).help);
    CHECK(throws({"--mode", "bogus"}));
    CHECK(throws({"--rate", "abc"}));
    CHECK(throws({"--rate", "-5"}));
    CHECK(throws({"--rate", "10X"}));
    CHECK(throws({"--queue", "70000"}));
    CHECK(throws({"--nonsense"}));
    CHECK(throws({"stray"}));
    CHECK(throws({"--rate", "99999999999999999999G"}));

    // validate() reports every problem at once
    auto bad = parse({"--burst", "1000", "--stats-interval", "10"}).validate();
    CHECK(bad.size() == 3);  // rate missing, burst < MTU, interval < 100
    CHECK(parse({"--rate", "1M", "--burst", "1500", "--queue-depth", "0"}).validate().size() == 1);

    std::cout << "All Config tests PASSED\n";
}
