#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "statistics.hpp"
#include "test_util.hpp"

using namespace std::chrono;

static void st01() {  // 4 threads x 1000 record_accept(1500) -> exactly 4000
    Statistics s;
    std::vector<std::thread> th;
    for (int t = 0; t < 4; ++t)
        th.emplace_back([&] {
            for (int i = 0; i < 1000; ++i) {
                s.record_accept(1500);
                s.record_drop(100);
            }
        });
    for (auto& t : th) t.join();
    CHECK(s.packets_accepted() == 4000);
    CHECK(s.bytes_accepted() == 4000u * 1500u);
    CHECK(s.packets_dropped() == 4000);
    CHECK(s.bytes_dropped() == 400000u);
    std::cout << "ST-01 PASS\n";
}
static void st02() {  // stop() returns promptly even with a long interval
    Statistics s;
    s.start(60'000);
    std::this_thread::sleep_for(milliseconds(20));
    const auto t0 = steady_clock::now();
    s.stop();
    CHECK(steady_clock::now() - t0 < seconds(2));
    std::cout << "ST-02 PASS\n";
}
static void st03() {  // destroying a started-but-not-stopped Statistics must not terminate
    Statistics s;
    s.start(1000);
    std::cout << "ST-03 PASS\n";
}

int main() {
    st01(); st02(); st03();
    std::cout << "All Statistics tests PASSED\n";
}
