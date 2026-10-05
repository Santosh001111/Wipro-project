// Exercises Policer and Shaper without a kernel: nfq_set_verdict() is replaced
// by a recording stub, so these tests do not need libnetfilter_queue or root.
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "packet.hpp"
#include "policer.hpp"
#include "shaper.hpp"
#include "test_util.hpp"

using namespace std::chrono;

namespace {
struct Verdict { uint32_t id; uint32_t verdict; steady_clock::time_point t; };
std::mutex g_mu;
std::vector<Verdict> g_v;
void clear() { std::lock_guard<std::mutex> l(g_mu); g_v.clear(); }
std::vector<Verdict> snapshot() { std::lock_guard<std::mutex> l(g_mu); return g_v; }
size_t count(uint32_t verdict) {
    size_t n = 0;
    for (auto& v : snapshot()) n += (v.verdict == verdict);
    return n;
}
struct nfq_q_handle* fake_qh() { return reinterpret_cast<struct nfq_q_handle*>(0x1); }
Packet make(uint32_t id, uint32_t len) {
    return Packet{nullptr, len, fake_qh(), id, steady_clock::now()};
}
}  // namespace

extern "C" int nfq_set_verdict(struct nfq_q_handle*, u_int32_t id, u_int32_t verdict, u_int32_t,
                               const unsigned char*) {
    std::lock_guard<std::mutex> l(g_mu);
    g_v.push_back({id, verdict, steady_clock::now()});
    return 1;
}

static void pk01() {  // a packet gets exactly one verdict; moved-from packet is inert
    clear();
    Packet a = make(1, 100);
    Packet b = std::move(a);
    CHECK(!a.accept());
    CHECK(b.accept());
    CHECK(!b.drop());
    CHECK(snapshot().size() == 1);
    std::cout << "PK-01 PASS\n";
}
static void po01() {  // police: burst of 2 x 1500 passes, the rest is dropped; jumbo dropped
    clear();
    TokenBucket tb(1.0, 3000);
    Statistics st;
    Policer p(tb, st);
    for (uint32_t i = 0; i < 5; ++i) { Packet k = make(i, 1500); p.process(k); }
    CHECK(count(NF_ACCEPT) == 2 && count(NF_DROP) == 3);
    CHECK(st.packets_accepted() == 2 && st.packets_dropped() == 3);
    CHECK(st.bytes_dropped() == 4500);
    Packet jumbo = make(99, 9000);
    p.process(jumbo);
    CHECK(count(NF_DROP) == 4);
    std::cout << "PO-01 PASS\n";
}
static void sh01() {  // pacing: 20 x 1500 B @ 100 kB/s, burst 3000 -> (30000-3000)/100000 = 0.27 s
    clear();
    TokenBucket tb(100'000, 3000);
    Statistics st;
    Shaper sh(tb, st, 64);
    sh.start();
    const auto t0 = steady_clock::now();
    for (uint32_t i = 0; i < 20; ++i) sh.enqueue(make(i, 1500));
    while (count(NF_ACCEPT) < 20 && steady_clock::now() - t0 < seconds(5))
        std::this_thread::sleep_for(milliseconds(5));
    const double secs = duration<double>(steady_clock::now() - t0).count();
    sh.stop();
    CHECK(count(NF_ACCEPT) == 20 && count(NF_DROP) == 0);
    CHECK(secs > 0.24 && secs < 0.45);
    auto v = snapshot();
    for (uint32_t i = 0; i < v.size(); ++i) CHECK(v[i].id == i);  // FIFO preserved
    CHECK(st.packets_accepted() == 20 && st.bytes_accepted() == 30000);
    std::cout << "SH-01 PASS (" << secs << " s)\n";
}
static void sh02() {  // queue overflow: overflow packets get a DROP verdict + correct byte count
    clear();
    TokenBucket tb(1.0, 1500);
    Statistics st;
    Shaper sh(tb, st, 4);  // thread not started => nothing drains
    for (uint32_t i = 0; i < 10; ++i) sh.enqueue(make(i, 1000));
    CHECK(count(NF_DROP) == 6 && count(NF_ACCEPT) == 0);
    CHECK(st.packets_dropped() == 6 && st.bytes_dropped() == 6000);
    std::cout << "SH-02 PASS\n";
}
static void sh03() {  // oversized packet must not block the queue head
    clear();
    TokenBucket tb(1'000'000, 1500);
    Statistics st;
    Shaper sh(tb, st, 8);
    sh.start();
    sh.enqueue(make(1, 9000));
    sh.enqueue(make(2, 1000));
    const auto t0 = steady_clock::now();
    while (count(NF_ACCEPT) < 1 && steady_clock::now() - t0 < seconds(2))
        std::this_thread::sleep_for(milliseconds(5));
    sh.stop();
    CHECK(count(NF_DROP) == 1 && count(NF_ACCEPT) == 1);
    std::cout << "SH-03 PASS\n";
}
static void sh04() {  // stop() flushes queued packets: none is left without a verdict
    clear();
    TokenBucket tb(1.0, 1500);  // practically never refills => packets would wait forever
    Statistics st;
    Shaper sh(tb, st, 100);
    sh.start();
    for (uint32_t i = 0; i < 10; ++i) sh.enqueue(make(i, 1500));
    std::this_thread::sleep_for(milliseconds(50));
    const auto t0 = steady_clock::now();
    sh.stop();
    CHECK(steady_clock::now() - t0 < seconds(1));
    CHECK(snapshot().size() == 10);
    std::cout << "SH-04 PASS\n";
}

int main() {
    pk01(); po01(); sh01(); sh02(); sh03(); sh04();
    std::cout << "All Policer/Shaper tests PASSED\n";
}
