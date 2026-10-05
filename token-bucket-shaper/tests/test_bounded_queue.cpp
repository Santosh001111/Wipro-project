#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "bounded_queue.hpp"
#include "test_util.hpp"

using namespace std::chrono;

static void bq01() {
    BoundedQueue<int> q(10);
    q.push(42);
    auto o = q.pop();
    CHECK(o && *o == 42);
    std::cout << "BQ-01 PASS\n";
}
static void bq02() {  // pop() blocks until a producer pushes ~50 ms later
    BoundedQueue<int> q(10);
    const auto t0 = steady_clock::now();
    std::thread p([&] {
        std::this_thread::sleep_for(milliseconds(50));
        q.push(123);
    });
    auto o = q.pop();
    const auto waited = steady_clock::now() - t0;
    p.join();
    CHECK(o && *o == 123);
    CHECK(waited >= milliseconds(40));
    std::cout << "BQ-02 PASS\n";
}
static void bq03() {
    BoundedQueue<int> q(2);
    CHECK(q.try_push(1));
    CHECK(q.try_push(2));
    CHECK(!q.try_push(3));
    CHECK(q.size() == 2);
    std::cout << "BQ-03 PASS\n";
}
static void bq04() {
    BoundedQueue<int> q(10);
    for (int i = 1; i <= 10; ++i) q.push(i);
    for (int i = 1; i <= 10; ++i) {
        auto o = q.pop();
        CHECK(o && *o == i);
    }
    CHECK(q.empty());
    std::cout << "BQ-04 PASS\n";
}
static void bq05() {  // shutdown() wakes a blocked pop(), which returns nullopt
    BoundedQueue<int> q(10);
    std::atomic<bool> woke{false}, got_value{true};
    std::thread c([&] {
        got_value = q.pop().has_value();
        woke = true;
    });
    std::this_thread::sleep_for(milliseconds(50));
    CHECK(!woke);
    q.shutdown();
    c.join();
    CHECK(woke && !got_value);
    CHECK(!q.try_push(1));  // rejected after shutdown
    std::cout << "BQ-05 PASS\n";
}
static void bq06() {  // 2 producers + 2 consumers, 10 000 items, each consumed exactly once
    BoundedQueue<int> q(64);  // small => producers really block on a full queue
    constexpr int kPer = 5000;
    std::vector<std::atomic<int>> seen(2 * kPer);
    for (auto& s : seen) s = 0;
    std::vector<std::thread> prod, cons;
    for (int p = 0; p < 2; ++p)
        prod.emplace_back([&, p] {
            for (int i = 0; i < kPer; ++i) q.push(p * kPer + i);
        });
    for (int c = 0; c < 2; ++c)
        cons.emplace_back([&] {
            while (auto o = q.pop()) ++seen[*o];
        });
    for (auto& t : prod) t.join();
    q.shutdown();  // consumers still drain what is left, then exit
    for (auto& t : cons) t.join();
    for (auto& s : seen) CHECK(s == 1);
    std::cout << "BQ-06 PASS\n";
}
static void bq07() {  // try_push(T&&) must leave the item intact when it fails
    BoundedQueue<std::unique_ptr<int>> q(1);
    CHECK(q.try_push(std::make_unique<int>(1)));
    auto item = std::make_unique<int>(2);
    CHECK(!q.try_push(std::move(item)));
    CHECK(item && *item == 2);
    std::cout << "BQ-07 PASS\n";
}

int main() {
    bq01(); bq02(); bq03(); bq04(); bq05(); bq06(); bq07();
    std::cout << "All BoundedQueue tests PASSED\n";
}
