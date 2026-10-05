#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "test_util.hpp"
#include "token_bucket.hpp"

using namespace std::chrono;

// Very slow refill (1 B/s) so "empty" stays empty for the whole test.
static constexpr double kSlow = 1.0;

static void tb01() {  // fresh bucket -> consume(burst) succeeds
    TokenBucket tb(1'000'000, 65536);
    CHECK(tb.consume(65536));
    std::cout << "TB-01 PASS\n";
}
static void tb02() {  // consume exactly burst -> bucket empty
    TokenBucket tb(kSlow, 65536);
    CHECK(tb.consume(65536));
    CHECK(tb.tokens() < 1.0);
    CHECK(!tb.consume(2));
    std::cout << "TB-02 PASS\n";
}
static void tb03() {  // burst+1 immediately fails
    TokenBucket tb(1'000'000, 65536);
    CHECK(!tb.consume(65537));
    std::cout << "TB-03 PASS\n";
}
static void tb04() {  // drain, sleep 100 ms @ 1 MB/s -> 100 KB available
    TokenBucket tb(1'000'000, 200'000);
    CHECK(tb.consume(200'000));
    CHECK(!tb.consume(100'000));
    std::this_thread::sleep_for(milliseconds(100));
    CHECK(tb.consume(100'000));
    std::cout << "TB-04 PASS\n";
}
static void tb05() {  // 10 x (10 ms sleep, consume 10 KB) @ 1 MB/s
    TokenBucket tb(1'000'000, 100'000);
    CHECK(tb.consume(100'000));  // start from empty so refill really is what pays
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(milliseconds(10));
        CHECK(tb.consume(10'000));
    }
    std::cout << "TB-05 PASS\n";
}
static void tb06() {  // consume(0) is true and changes nothing
    TokenBucket tb(kSlow, 65536);
    CHECK(tb.consume(60000));
    const double before = tb.tokens();
    CHECK(tb.consume(0));
    CHECK(tb.tokens() >= before);       // only refill may add
    CHECK(tb.tokens() < before + 1.0);  // ...and at 1 B/s that is < 1 byte
    std::cout << "TB-06 PASS\n";
}
static void tb07() {  // 4 threads x 1000 consume(1500): never over-spend, never negative
    const double rate = 15'000, burst = 150'000;
    TokenBucket tb(rate, burst);
    std::atomic<long> ok{0};
    const auto t0 = steady_clock::now();
    std::vector<std::thread> th;
    for (int t = 0; t < 4; ++t)
        th.emplace_back([&] {
            for (int i = 0; i < 1000; ++i)
                if (tb.consume(1500)) ++ok;
        });
    for (auto& t : th) t.join();
    const double elapsed = duration<double>(steady_clock::now() - t0).count();
    CHECK(static_cast<double>(ok) * 1500.0 <= burst + rate * elapsed + 1500.0);
    CHECK(tb.tokens() >= 0.0);
    CHECK(ok >= 100);  // at least the initial burst was usable
    std::cout << "TB-07 PASS (granted=" << ok << ")\n";
}
static void tb08() {  // invalid construction is rejected (no division by zero later)
    bool t0 = false, t1 = false, t2 = false;
    try { TokenBucket tb(0, 65536); } catch (const std::invalid_argument&) { t0 = true; }
    try { TokenBucket tb(-5, 65536); } catch (const std::invalid_argument&) { t1 = true; }
    try { TokenBucket tb(1000, 1499); } catch (const std::invalid_argument&) { t2 = true; }
    CHECK(t0 && t1 && t2);
    std::cout << "TB-08 PASS\n";
}
static void tb09() {  // reset() and time_until()
    TokenBucket tb(1000, 3000);
    CHECK(tb.consume(3000));
    CHECK(tb.time_until(1000) > milliseconds(500));
    CHECK(tb.time_until(1000) <= milliseconds(1001));
    CHECK(tb.time_until(3001) == nanoseconds::max());
    tb.reset();
    CHECK(tb.consume(3000));
    CHECK(tb.time_until(0) == nanoseconds::zero());
    std::cout << "TB-09 PASS\n";
}

int main() {
    tb01(); tb02(); tb03(); tb04(); tb05(); tb06(); tb07(); tb08(); tb09();
    std::cout << "All TokenBucket tests PASSED\n";
}
