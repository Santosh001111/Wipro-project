# Token-Bucket Shaper — Complete Project Breakdown
## Steps, Substeps & Git Commits

> [!IMPORTANT]
> **Git & GitHub are integrated throughout every step.** Every meaningful piece of work ends with a proper commit. Follow the **Conventional Commits** format for all commit messages: `<type>(<scope>): <short description>`. Commit often — small, focused commits show professional development progress.

---

## Git Commit Message Format

```
<type>(<scope>): <short imperative description>

[Optional body — explain WHY, not WHAT]
```

| Type | When to use |
|------|------------|
| `feat` | New feature or capability added |
| `fix` | Bug fix |
| `refactor` | Code restructured, no behaviour change |
| `test` | Tests added or updated |
| `docs` | Documentation only |
| `chore` | Build system, CMake, dependencies |
| `style` | Formatting, whitespace only |
| `perf` | Performance improvement |

---

## Step 1 — Environment Setup & Git Initialisation

### 1.1 Install System Dependencies
- Install build tools: `build-essential`, `cmake`, `pkg-config`
- Install NFQUEUE libraries: `libnetfilter-queue-dev`, `libnfnetlink-dev`
- Install iptables: `iptables`, `iptables-persistent`
- Install testing tools: `iperf3`, `tcpdump`, `wireshark-cli`
- Install debug/quality tools: `valgrind`, `clang-tidy`, `clang-format`, `gdb`
- Verify each: `dpkg -l <package>` or `which <binary>`

### 1.2 Verify Kernel & Privileges
- Check kernel version: `uname -r` (must be ≥ 4.14)
- Confirm `nf_tables` / `iptables` modules loaded: `lsmod | grep nf`
- Confirm you can run with `sudo` or have `CAP_NET_ADMIN` + `CAP_NET_RAW`
- Test NFQUEUE rule: `sudo iptables -I OUTPUT -j NFQUEUE --queue-num 99` then remove it

### 1.3 Create Project Directory Structure
- Create root folder: `token-bucket-shaper/`
- Create subdirectories: `src/`, `include/`, `tests/`, `scripts/`, `docs/report/`
- Create empty placeholder `.cpp` and `.h` files for every planned module
- Create `CMakeLists.txt` (scaffold — targets, flags, link libraries)
- Create `README.md` (title + placeholder sections)
- Create `.clang-format` (Google style or custom)
- Create `.gitignore`:
  ```
  build/
  *.o
  *.out
  a.out
  .cache/
  compile_commands.json
  ```

### 1.4 Initialise Git Repository
- `git init` inside the project root
- `git add .`
- **Commit:** `chore: initial project scaffold with directory structure`
- Create GitHub repository (public, no README since we already have one)
- Add remote: `git remote add origin https://github.com/<username>/token-bucket-shaper.git`
- Push: `git push -u origin main`

### 1.5 Set Up Branching Strategy
- You will always work on a **feature branch**, never directly on `main`
- Create and switch to the first feature branch:
  ```bash
  git checkout -b feature/environment-setup
  ```
- Branch naming convention:
  | Branch | Purpose |
  |--------|---------|
  | `main` | Stable, working code only — protected |
  | `develop` | Integration branch — merge features here first |
  | `feature/<name>` | One branch per module/feature |
  | `test/<name>` | Integration test branches |
  | `hotfix/<name>` | Emergency bug fixes on `main` |

### 1.6 Validate Build System
- Write a minimal `CMakeLists.txt` that compiles a `main.cpp` printing "Hello, tbshaper"
- Run `cmake -B build && cmake --build build` — confirm it succeeds
- Add linker flags: `-lnetfilter_queue -lnfnetlink -lpthread` and rebuild
- **Commit:** `chore(cmake): add CMakeLists with netfilter_queue and pthread linking`
- Push branch: `git push -u origin feature/environment-setup`
- Open Pull Request on GitHub → merge `feature/environment-setup` → `main`

---

## Step 2 — Token Bucket Core (`TokenBucket`)

### 2.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/token-bucket
```

### 2.1 Design the Class Interface
- Write `include/token_bucket.hpp` with the full class declaration
- Fields: `tokens_`, `capacity_`, `rate_`, `last_refill_`, `mutex_`
- Public API: `consume(uint32_t bytes) → bool`, `refill()`, `reset()`
- Mark `consume()` with `[[nodiscard]]`
- Write Doxygen comments on every public method
- **Commit:** `feat(token-bucket): add TokenBucket class declaration with Doxygen`

### 2.2 Implement the Constructor
- Accept `double rate_bytes_per_sec` and `double burst_bytes`
- Initialise `tokens_ = capacity_ = burst_bytes`
- Initialise `last_refill_ = std::chrono::steady_clock::now()`
- Validate: `rate > 0`, `burst >= 1500` — throw `std::invalid_argument` if invalid
- **Commit:** `feat(token-bucket): implement constructor with input validation`

### 2.3 Implement Lazy Refill (`refill()`)
- Compute `Δt = steady_clock::now() - last_refill_` using `duration<double>`
- `tokens_ = std::min(tokens_ + Δt_seconds × rate_, capacity_)`
- Update `last_refill_ = now`
- Private method — called only from `consume()`
- **Commit:** `feat(token-bucket): implement lazy refill using steady_clock`

### 2.4 Implement `consume(bytes)`
- Lock `std::mutex` with `std::lock_guard`
- Call `refill()` first
- If `tokens_ >= bytes`: subtract, return `true`; else return `false`
- No negative token state ever possible
- **Commit:** `feat(token-bucket): implement consume() with mutex-protected token check`

### 2.5 Implement `reset()`
- Lock mutex, set `tokens_ = capacity_`, reset `last_refill_`
- **Commit:** `feat(token-bucket): add reset() method`

### 2.6 Write Unit Tests (`tests/test_token_bucket.cpp`)
- **TB-01** Fresh bucket → `consume(burst)` returns `true`
- **TB-02** Consume exactly burst → bucket at 0 tokens
- **TB-03** `consume(burst + 1)` immediately → returns `false`
- **TB-04** Drain; sleep 100 ms; rate=1 MB/s → `consume(100'000)` returns `true`
- **TB-05** 10 × 10 ms sleeps, 10 KB consume each → all return `true`
- **TB-06** `consume(0)` → returns `true`, tokens unchanged
- **TB-07** 4 threads × 1000 `consume(1500)` calls → tokens never go below 0
- **TB-08** `rate = 0` → `consume(any)` returns `false`, no division by zero
- **Commit:** `test(token-bucket): add 8 unit tests covering all edge cases`

### 2.7 Verify & Merge ✅ GATE
- Run all 8 tests — all must pass
- **Commit (if fixes needed):** `fix(token-bucket): <describe what was fixed>`
- Push: `git push origin feature/token-bucket`
- Open PR → merge `feature/token-bucket` → `main`

---

## Step 3 — Packet Data Model (`Packet`)

### 3.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/packet-model
```

### 3.1 Design the `Packet` Structure
- Write `include/packet.hpp`
- Fields: `const uint8_t* data_`, `uint32_t length_`, `struct nfq_q_handle* qh_`, `uint32_t id_`, `std::chrono::steady_clock::time_point arrival_`
- Delete copy constructor (data_ is not owned): `Packet(const Packet&) = delete`
- Implement move constructor
- **Commit:** `feat(packet): add Packet struct with move semantics and NFQUEUE fields`

### 3.2 Implement Verdict Methods
- `void accept()` → `nfq_set_verdict(qh_, id_, NF_ACCEPT, 0, nullptr)` — check return value
- `void drop()` → `nfq_set_verdict(qh_, id_, NF_DROP, 0, nullptr)` — check return value
- Log error to `stderr` if `nfq_set_verdict` fails
- Add getters: `uint32_t length()`, `uint32_t id()`, `time_point arrival()`
- **Commit:** `feat(packet): implement accept() and drop() verdict methods`

### 3.3 Merge
- Push: `git push origin feature/packet-model`
- Open PR → merge `feature/packet-model` → `main`

---

## Step 4 — Bounded Queue (`BoundedQueue<T>`)

### 4.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/bounded-queue
```

### 4.1 Design the Template Class
- Write `include/bounded_queue.hpp` (header-only template)
- Fields: `std::deque<T> queue_`, `size_t capacity_`, `std::mutex mutex_`, `std::condition_variable not_empty_`, `std::condition_variable not_full_`, `std::atomic<bool> shutdown_`
- **Commit:** `feat(bounded-queue): add BoundedQueue template class declaration`

### 4.2 Implement `push(T item)` — Blocking
- Lock mutex, wait on `not_full_` if full (check `shutdown_` flag in predicate)
- Push to back, notify `not_empty_`
- **Commit:** `feat(bounded-queue): implement blocking push()`

### 4.3 Implement `try_push(T item)` — Non-Blocking
- Lock mutex, return `false` immediately if at capacity
- Otherwise push and notify `not_empty_`, return `true`
- Mark with `[[nodiscard]]`
- **Commit:** `feat(bounded-queue): implement non-blocking try_push()`

### 4.4 Implement `pop()` — Blocking
- Lock mutex, wait on `not_empty_` until data available or `shutdown_`
- Pop front, notify `not_full_`, return item
- **Commit:** `feat(bounded-queue): implement blocking pop()`

### 4.5 Implement `shutdown()`
- Set `shutdown_ = true`
- Call `notify_all()` on both condition variables
- **Commit:** `feat(bounded-queue): add shutdown() for clean thread exit`

### 4.6 Helpers: `size()` and `empty()`
- Both under lock
- **Commit:** `feat(bounded-queue): add size() and empty() helpers`

### 4.7 Write Unit Tests (`tests/test_bounded_queue.cpp`)
- **BQ-01** Single push + pop → correct item returned
- **BQ-02** `pop()` on empty blocks; producer pushes after 50 ms → unblocks
- **BQ-03** `try_push()` beyond capacity returns `false`; size stays at capacity
- **BQ-04** FIFO ordering: push 1…10 → pop order is 1, 2, …, 10
- **BQ-05** `shutdown()` while thread blocked on `pop()` → thread wakes and exits
- **BQ-06** 2 producers + 2 consumers, 10 000 items → consumed exactly once
- **Commit:** `test(bounded-queue): add 6 unit tests including concurrency stress test`

### 4.8 Verify & Merge ✅ GATE
- All 6 tests must pass
- Push: `git push origin feature/bounded-queue`
- Open PR → merge `feature/bounded-queue` → `main`

---

## Step 5 — NFQUEUE Capture (`NFQueueCapture`)

### 5.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/nfqueue-capture
```

### 5.1 Write iptables Scripts
- Write `scripts/setup_iptables.sh`:
  ```bash
  #!/bin/bash
  sudo iptables -I OUTPUT -j NFQUEUE --queue-num ${1:-0}
  echo "[setup] NFQUEUE rule inserted for queue ${1:-0}"
  ```
- Write `scripts/teardown_iptables.sh`:
  ```bash
  #!/bin/bash
  sudo iptables -D OUTPUT -j NFQUEUE --queue-num ${1:-0}
  echo "[teardown] NFQUEUE rule removed for queue ${1:-0}"
  ```
- Make both executable: `chmod +x scripts/*.sh`
- **Commit:** `chore(scripts): add iptables setup and teardown shell scripts`

### 5.2 Design `NFQueueCapture` Interface
- Write `include/nfqueue_capture.hpp`
- Fields: `nfq_handle* nfq_h_`, `nfq_q_handle* nfq_qh_`, `std::thread capture_thread_`, `std::atomic<bool> running_`
- Methods: `start()`, `stop()`, static `callback()`
- **Commit:** `feat(nfqueue): add NFQueueCapture class declaration`

### 5.3 Open the NFQUEUE Handle
- `nfq_open()` → error check
- `nfq_unbind_pf(h, AF_INET)` then `nfq_bind_pf(h, AF_INET)`
- `nfq_create_queue(h, queue_num, &callback, user_data)`
- `nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff)`
- Throw on any failure
- **Commit:** `feat(nfqueue): implement NFQUEUE handle open and queue binding`

### 5.4 Set Socket Buffer Size
- Get fd: `nfq_fd(h)`
- `setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz))` with 4 MB buffer
- **Commit:** `fix(nfqueue): set large SO_RCVBUF to prevent packet drops under load`

### 5.5 Implement the Static Packet Callback
- Parse packet id from `nfq_get_msg_packet_hdr()`
- Get payload via `nfq_get_payload()`
- Record `arrival_ = steady_clock::now()`
- Construct `Packet` and invoke the registered handler via `data` pointer
- **Commit:** `feat(nfqueue): implement packet callback with Packet construction`

### 5.6 Implement the Receive Loop
- `fd = nfq_fd(h)` — tight loop: `recv(fd, buf, sizeof(buf), 0)` → `nfq_handle_packet(h, buf, rv)`
- Check `running_` flag each iteration
- Handle `ENOBUFS`: log warning and `continue` — do **not** crash
- **Commit:** `feat(nfqueue): implement recv loop with ENOBUFS handling`

### 5.7 Implement `start()` and `stop()`
- `start()` — spawn `capture_thread_` running the loop
- `stop()` — set `running_ = false`, join thread
- Destructor calls `stop()` (RAII)
- **Commit:** `feat(nfqueue): add start() and stop() with RAII destructor`

### 5.8 Smoke Test ✅ GATE
- Temporary test harness: print packet length, issue `NF_ACCEPT` for everything
- `./scripts/setup_iptables.sh`, start binary, run `ping 8.8.8.8`
- Confirm packets are printed in console, confirm ping still works
- Remove test harness code after confirmation
- **Commit:** `test(nfqueue): verify packet capture smoke test passes`
- Push: `git push origin feature/nfqueue-capture`
- Open PR → merge `feature/nfqueue-capture` → `main`

---

## Step 6 — Policing Mode (`Policer`)

### 6.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/policer
```

### 6.1 Design the `Policer` Class
- Write `include/policer.hpp`
- Constructor takes: `TokenBucket&`, `Statistics&`
- Method: `void process(Packet& pkt)`
- **Commit:** `feat(policer): add Policer class declaration`

### 6.2 Implement `process()`
- Call `bucket_.consume(pkt.length())`
- `true` → `pkt.accept()` + `stats_.record_accept(pkt.length())`
- `false` → `pkt.drop()` + `stats_.record_drop(pkt.length())`
- Must be **fast** — this runs on the hot capture path
- **Commit:** `feat(policer): implement process() with NF_ACCEPT and NF_DROP logic`

### 6.3 Wire Policer into `main.cpp`
- Create `TokenBucket` and `Policer`; pass `&policer` as `user_data` to `nfq_create_queue`
- In static callback: cast `data` to `Policer*`, call `->process(pkt)`
- **Commit:** `feat(main): wire Policer into NFQUEUE callback for police mode`

### 6.4 Integration Test ✅ GATE
- `./scripts/setup_iptables.sh`
- `sudo ./build/tbshaper --mode police --rate 1000000 --burst 65536 --queue 0`
- Run `iperf3 -s` in one terminal, `iperf3 -c 127.0.0.1 -t 10 -b 100M` in another
- Verify measured throughput ≈ 1 Mbit/s ± 5%
- `./scripts/teardown_iptables.sh`
- **Commit:** `test(policer): iperf3 confirms policing caps throughput to configured rate`
- Push: `git push origin feature/policer`
- Open PR → merge `feature/policer` → `main`

---

## Step 7 — Shaping Mode (`Shaper`)

### 7.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/shaper
```

### 7.1 Design the `Shaper` Class
- Write `include/shaper.hpp`
- Fields: `TokenBucket&`, `BoundedQueue<Packet>`, `Statistics&`, `std::thread shaper_thread_`, `std::atomic<bool> running_`
- Constructor: takes `TokenBucket&`, `Statistics&`, `size_t queue_depth`
- Methods: `enqueue(Packet)`, `start()`, `stop()`, `shaper_loop()`
- **Commit:** `feat(shaper): add Shaper class declaration`

### 7.2 Implement `enqueue(Packet pkt)` — Hot Path
- Use `try_push()` (non-blocking)
- If returns `false`: `pkt.drop()` + `stats_.record_drop()` (queue full)
- If returns `true`: packet buffered; verdict issued later by shaper thread
- **Commit:** `feat(shaper): implement enqueue() using non-blocking try_push`

### 7.3 Implement Shaper Thread (`shaper_loop()`)
- Loop while `running_`:
  - `pkt = queue_.pop()` — blocks until a packet arrives
  - Retry loop: call `bucket_.consume(pkt.length())`
    - `true` → `pkt.accept()` + `stats_.record_accept()` → break
    - `false` → compute `sleep = (bytes_needed - tokens) / rate`; `sleep_for()` then retry
- **Commit:** `feat(shaper): implement shaper_loop() with sleep-retry pacing`

### 7.4 Implement `start()` and `stop()`
- `start()`: set `running_ = true`, spawn `shaper_thread_`
- `stop()`: set `running_ = false`, call `queue_.shutdown()`, join thread
- Destructor calls `stop()` (RAII)
- **Commit:** `feat(shaper): add start() and stop() with RAII shutdown`

### 7.5 Wire Shaper into `main.cpp`
- In shape mode: create `Shaper`, call `shaper.start()`; pass `&shaper` as callback `user_data`
- In static callback: cast to `Shaper*`, call `->enqueue(std::move(pkt))`
- **Commit:** `feat(main): wire Shaper into NFQUEUE callback for shape mode`

### 7.6 Integration Test ✅ GATE
- Same iperf3 test as Step 6.4 but with `--mode shape`
- Verify: throughput ≈ 1 Mbit/s ± 5% **and** TCP retransmits = 0
- Run `ping -c 20 127.0.0.1` in parallel → RTT < 50 ms
- **Commit:** `test(shaper): iperf3 confirms shaping mode with 0 TCP retransmits`
- Push: `git push origin feature/shaper`
- Open PR → merge `feature/shaper` → `main`

---

## Step 8 — Statistics (`Statistics`)

### 8.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/statistics
```

### 8.1 Design the Class (Atomic Counters)
- Write `include/statistics.hpp`
- All counters: `std::atomic<uint64_t>` — `packets_accepted_`, `packets_dropped_`, `bytes_accepted_`, `bytes_dropped_`
- Non-atomic: `start_time_`, `stats_thread_`, `running_`, `interval_ms_`
- **Commit:** `feat(statistics): add Statistics class with atomic counter fields`

### 8.2 Implement `record_accept()` and `record_drop()`
- `packets_accepted_.fetch_add(1, std::memory_order_relaxed)`
- `bytes_accepted_.fetch_add(bytes, std::memory_order_relaxed)`
- No mutex — atomics are sufficient; these must be as fast as possible
- **Commit:** `feat(statistics): implement record_accept() and record_drop() with atomics`

### 8.3 Implement `report()`
- Read all atomic counters with `load(std::memory_order_relaxed)`
- Compute elapsed seconds from `start_time_`
- Compute throughput: `bytes_accepted / elapsed / 125000.0` → Mbit/s
- Print:
  ```
  [Stats  5s] pass=1234  drop=5678  throughput=0.998 Mbit/s
  ```
- **Commit:** `feat(statistics): implement report() with throughput calculation`

### 8.4 Implement Stats Thread and `start()` / `stop()`
- `run()`: loop while `running_`, `sleep_for(interval_ms_)`, call `report()`
- `start()`: record `start_time_`, set `running_ = true`, spawn `stats_thread_`
- `stop()`: set `running_ = false`, join thread, call `report()` one final time
- **Commit:** `feat(statistics): add stats thread with start() and stop()`

### 8.5 Concurrency Test
- 4 threads × 1000 calls each of `record_accept(1500)` → final count must be exactly 4000
- Run with TSan to confirm no data races
- **Commit:** `test(statistics): add concurrency test confirming atomic counter correctness`

### 8.6 Merge
- Push: `git push origin feature/statistics`
- Open PR → merge `feature/statistics` → `main`

---

## Step 9 — Configuration & CLI (`Config`)

### 9.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/config
```

### 9.1 Define the `Config` Struct
- Write `include/config.hpp`
- Fields: `uint64_t rate_bps`, `uint64_t burst_bytes`, `uint16_t queue_num`, `enum class Mode { POLICE, SHAPE } mode`, `size_t queue_depth` (default: 1024), `uint32_t stats_interval_ms` (default: 5000), `bool verbose`
- **Commit:** `feat(config): add Config struct with all tunable parameter fields`

### 9.2 Implement CLI Argument Parsing
- Use `getopt_long()` to support long flags:

| Flag | Default | Example |
|------|---------|---------|
| `--rate <N[K/M/G]>` | required | `--rate 1M` → 1,000,000 bps |
| `--burst <N[K/M/G]>` | required | `--burst 64K` → 65,536 bytes |
| `--mode police\|shape` | `police` | `--mode shape` |
| `--queue <N>` | `0` | `--queue 1` |
| `--queue-depth <N>` | `1024` | `--queue-depth 512` |
| `--stats-interval <ms>` | `5000` | `--stats-interval 1000` |
| `--verbose` | off | `--verbose` |
| `--help` | — | prints usage and exits 0 |

- Implement K/M/G suffix parsing for rate and burst
- **Commit:** `feat(config): implement CLI argument parsing with K/M/G suffix support`

### 9.3 Implement `Config::validate()`
- `rate_bps > 0` — error if zero
- `burst_bytes >= 1500` — error: "burst must be ≥ MTU (1500 bytes)"
- `stats_interval_ms >= 100`
- Print all errors before `exit(1)`
- **Commit:** `feat(config): add validate() with meaningful error messages`

### 9.4 Startup Banner
- Print after validation passes:
  ```
  [tbshaper] Mode           : SHAPING
  [tbshaper] Rate           : 1.000 Mbit/s
  [tbshaper] Burst          : 65536 bytes
  [tbshaper] Queue Number   : 0
  [tbshaper] Queue Depth    : 1024 packets
  [tbshaper] Stats Interval : 5000 ms
  ```
- **Commit:** `feat(config): add startup configuration banner`

### 9.5 Merge
- Push: `git push origin feature/config`
- Open PR → merge `feature/config` → `main`

---

## Step 10 — Signal Handling & Graceful Shutdown

### 10.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/signal-handling
```

### 10.1 Install Signal Handlers
- Use `sigaction()` (not `signal()`)
- Handle `SIGINT` (Ctrl+C) and `SIGTERM`
- Handler **only** sets `std::atomic<bool> g_running = false`
- Never call non-async-safe functions inside the handler
- **Commit:** `feat(signals): install SIGINT and SIGTERM handlers via sigaction()`

### 10.2 Implement Ordered Shutdown Sequence in `main()`
When `g_running` becomes false, execute in this exact order:
1. `capture.stop()` — no new packets received
2. `shaper.stop()` — drain queue, issue remaining verdicts (shape mode only)
3. `stats.stop()` — print final stats
4. `nfq_destroy_queue(qh)` + `nfq_close(h)` — release NFQUEUE handles
5. Print: `"[tbshaper] Clean shutdown complete."`
6. `return 0`
- **Commit:** `feat(main): implement ordered graceful shutdown sequence on SIGINT`

### 10.3 Test Graceful Shutdown
- Start tbshaper with iperf3 running
- `kill -INT <pid>` from another terminal
- Verify: final stats print, exit code 0, no stuck threads
- Run Valgrind — confirm zero leaks on shutdown
- **Commit:** `test(signals): verify graceful shutdown with Valgrind clean exit`

### 10.4 Merge
- Push: `git push origin feature/signal-handling`
- Open PR → merge `feature/signal-handling` → `main`

---

## Step 11 — `main.cpp` — Wiring Everything Together

### 11.0 Create Feature Branch
```bash
git checkout main && git pull
git checkout -b feature/main-integration
```

### 11.1 Implement Full `main()`
```
Parse Config → Validate Config → Print Banner →
Install signal handlers →
Construct TokenBucket →
Construct Statistics → stats.start() →
Construct NFQueueCapture →
If POLICE: construct Policer, register as callback
If SHAPE:  construct Shaper, shaper.start(), register as callback →
capture.start() →
Block on g_running flag →
On false: execute shutdown sequence
```
- Wrap all initialisation in `try/catch(std::exception& e)` → print + exit 1
- **Commit:** `feat(main): wire all subsystems together in main() with error handling`

### 11.2 Mode Dispatch
- Use `if/else` on `config.mode`
- The NFQUEUE `void* user_data` carries pointer to either `Policer` or `Shaper`
- **Commit:** `feat(main): implement police/shape mode dispatch via NFQUEUE user_data`

### 11.3 End-to-End Smoke Test
- Build Release mode: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`
- Run both modes back-to-back, confirm both work correctly
- **Commit:** `chore(build): add Release build target; verify end-to-end integration`

### 11.4 Merge
- Push: `git push origin feature/main-integration`
- Open PR → merge `feature/main-integration` → `main`

---

## Step 12 — Full Testing Pass

### 12.0 Create Test Branch
```bash
git checkout main && git pull
git checkout -b test/full-testing-pass
```

### 12.1 Run All Unit Tests
```bash
cmake --build build --target test
```
- All TB-01 to TB-08 must pass
- All BQ-01 to BQ-06 must pass
- **Commit (if fixes needed):** `fix(<scope>): <describe what was fixed>`

### 12.2 Run iperf3 Integration Tests

| Test ID | Mode | Rate | Pass Criterion |
|---------|------|------|----------------|
| IT-01 | Police | 1 Mbit/s | ~1.0 Mbit/s ± 5% |
| IT-02 | Police | 10 Mbit/s | ~10.0 Mbit/s ± 5% |
| IT-03 | Shape | 1 Mbit/s | ~1.0 Mbit/s, 0 retransmits |
| IT-04 | Shape | 10 Mbit/s | ~10.0 Mbit/s, smooth output |
| IT-05 | Police UDP | 512 Kbit/s | ~512 Kbit/s, drops correctly |
| IT-06 | Shape | 512 Kbit/s | Jitter < 5 ms, 0 retransmits |

- **Commit:** `test(integration): iperf3 results confirm rate accuracy within ±5%`

### 12.3 Valgrind Memory Check
```bash
sudo valgrind --leak-check=full --track-origins=yes \
  ./build/tbshaper --mode police --rate 1M --burst 64K --queue 0
```
- Expected: `"0 errors from 0 contexts"`
- **Commit:** `test(memory): Valgrind Memcheck confirms zero leaks on clean shutdown`

### 12.4 ThreadSanitizer (Data Race Check)
```bash
cmake -B build-tsan \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan
sudo ./build-tsan/tbshaper --mode shape --rate 1M --burst 64K --queue 0
# Run iperf3 in parallel for 10 seconds
```
- Expected: no `ThreadSanitizer: data race` output
- **Commit:** `test(concurrency): TSan confirms zero data races in shaping mode`

### 12.5 Edge Case Tests

| Scenario | Trigger | Expected |
|----------|---------|----------|
| `burst = MTU = 1500` | `--burst 1500` | Packets accepted/dropped whole |
| Queue overflow | `--queue-depth 4`, send burst | Drop counter increments, no crash |
| SIGINT during load | Ctrl+C while iperf3 running | Final stats, clean exit |
| Jumbo frame with small burst | 9000-byte frame, `burst=1500` | `consume()` → `false`, dropped |

- **Commit:** `test(edge-cases): verify all edge case scenarios behave correctly`

### 12.6 60-Second Stress Test
```bash
iperf3 -c 127.0.0.1 -u -b 100M -t 60
```
- CPU < 20%, RSS stable, counters monotonically increasing, no crash
- **Commit:** `test(stress): 60s UDP stress test confirms stability and no memory growth`

### 12.7 Merge
- Push: `git push origin test/full-testing-pass`
- Open PR → merge `test/full-testing-pass` → `main`

---

## Step 13 — Code Quality & Final Polish

### 13.0 Create Branch
```bash
git checkout main && git pull
git checkout -b chore/code-quality
```

### 13.1 Apply clang-format
```bash
clang-format -i src/*.cpp include/*.hpp tests/*.cpp
```
- **Commit:** `style: apply clang-format to all source and header files`

### 13.2 Run clang-tidy
- Fix all `modernize-*`, `performance-*`, `readability-*` warnings
- **Commit:** `refactor: fix all clang-tidy warnings across codebase`

### 13.3 Zero-Warning Build
```bash
cmake -B build-strict \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
cmake --build build-strict
```
- Must produce **zero** diagnostics
- **Commit:** `chore(build): enforce -Werror; all warnings resolved`

### 13.4 Code Review Checklist
- [ ] No raw `new`/`delete` anywhere — all heap via smart pointers
- [ ] `[[nodiscard]]` on `consume()` and `try_push()`
- [ ] `noexcept` on move constructors
- [ ] All magic numbers replaced with named `constexpr` constants
- [ ] All `TODO`/`FIXME` resolved or documented
- [ ] `nfq_set_verdict()` return value checked everywhere
- [ ] `ENOBUFS` handled in the receive loop
- [ ] Doxygen on every public class and method
- **Commit:** `refactor: final code review fixes — constexpr, noexcept, nodiscard`

### 13.5 Merge
- Push: `git push origin chore/code-quality`
- Open PR → merge `chore/code-quality` → `main`

---

## Step 14 — Documentation, GitHub & Presentation

### 14.0 Create Branch
```bash
git checkout main && git pull
git checkout -b docs/final-report-and-readme
```

### 14.1 Write `README.md`
- Project description (2–3 sentences)
- Architecture overview (ASCII diagram)
- Prerequisites and installation commands
- Build instructions (exact commands)
- CLI flag reference table
- Usage examples with expected output
- Teardown instructions
- Known limitations
- **Commit:** `docs(readme): write complete README with build, usage, and teardown instructions`

### 14.2 Write Stage Report Files
- `docs/report/stage1.md` through `docs/report/stage6.md`
- Each maps to the corresponding project stage deliverable
- **Commit:** `docs(report): add stage1–stage6 report documents`

### 14.3 Final GitHub Push & Tagging
- Ensure `main` is fully up to date
- Create an annotated release tag:
  ```bash
  git tag -a v1.0 -m "Final project submission — Token-Bucket Shaper v1.0"
  git push origin main --tags
  ```
- Verify on GitHub: all files visible, tag shows as a Release
- **Commit (if any last fixes):** `chore: final pre-release cleanup`

### 14.4 Prepare Presentation
- 12 slides following the Stage 6 structure
- Practice live demo sequence:
  1. Insert iptables rule (`setup_iptables.sh`)
  2. Police mode → iperf3 → read throughput
  3. Shape mode → iperf3 → compare RTT with parallel ping
  4. Send SIGINT → final stats → clean exit
  5. Run `teardown_iptables.sh`
- Time the demo to fit within 5 minutes
- **Commit:** `docs(presentation): add presentation slides to docs/`

### 14.5 Final Merge
- Push: `git push origin docs/final-report-and-readme`
- Open PR → merge `docs/final-report-and-readme` → `main`
- **Project complete ✅**

---

## Complete Step Map at a Glance

```
Step 1  — Environment Setup & Git Init
  1.1  Install dependencies
  1.2  Verify kernel & privileges
  1.3  Create directory structure
  1.4  git init + GitHub remote setup          ← COMMIT: chore: initial scaffold
  1.5  Set up branching strategy
  1.6  Validate build system                   ← COMMIT: chore(cmake): linker flags

Step 2  — TokenBucket (Core Algorithm)
  2.0  git checkout -b feature/token-bucket
  2.1  Class interface + Doxygen               ← COMMIT: feat: add declaration
  2.2  Constructor + validation                ← COMMIT: feat: constructor
  2.3  Lazy refill()                           ← COMMIT: feat: lazy refill
  2.4  consume()                               ← COMMIT: feat: consume()
  2.5  reset()                                 ← COMMIT: feat: reset()
  2.6  Unit tests TB-01 to TB-08               ← COMMIT: test: 8 unit tests
  2.7  PR → merge to main                      ← ✅ GATE

Step 3  — Packet Data Model
  3.0  git checkout -b feature/packet-model
  3.1  Packet struct + move semantics          ← COMMIT: feat: Packet struct
  3.2  accept() / drop()                       ← COMMIT: feat: verdict methods
  3.3  PR → merge to main

Step 4  — BoundedQueue<T>
  4.0  git checkout -b feature/bounded-queue
  4.1  Template class declaration              ← COMMIT: feat: BoundedQueue declaration
  4.2  push() blocking                         ← COMMIT: feat: blocking push
  4.3  try_push() non-blocking                 ← COMMIT: feat: non-blocking try_push
  4.4  pop() blocking                          ← COMMIT: feat: blocking pop
  4.5  shutdown()                              ← COMMIT: feat: shutdown()
  4.6  size() / empty()                        ← COMMIT: feat: helpers
  4.7  Unit tests BQ-01 to BQ-06              ← COMMIT: test: 6 unit tests
  4.8  PR → merge to main                      ← ✅ GATE

Step 5  — NFQueueCapture
  5.0  git checkout -b feature/nfqueue-capture
  5.1  iptables scripts                        ← COMMIT: chore: shell scripts
  5.2  Class declaration                       ← COMMIT: feat: declaration
  5.3  Open NFQUEUE handle                     ← COMMIT: feat: handle open
  5.4  Socket buffer size                      ← COMMIT: fix: SO_RCVBUF
  5.5  Packet callback                         ← COMMIT: feat: callback
  5.6  Receive loop + ENOBUFS                  ← COMMIT: feat: recv loop
  5.7  start() / stop() RAII                   ← COMMIT: feat: RAII lifecycle
  5.8  Smoke test                              ← COMMIT: test: smoke test
  PR → merge to main                           ← ✅ GATE

Step 6  — Policer Mode
  6.0  git checkout -b feature/policer
  6.1  Class declaration                       ← COMMIT: feat: Policer class
  6.2  process()                               ← COMMIT: feat: process()
  6.3  Wire into main.cpp                      ← COMMIT: feat: wire callback
  6.4  iperf3 integration test                 ← COMMIT: test: iperf3 policing
  PR → merge to main                           ← ✅ GATE

Step 7  — Shaper Mode
  7.0  git checkout -b feature/shaper
  7.1  Class declaration                       ← COMMIT: feat: Shaper class
  7.2  enqueue()                               ← COMMIT: feat: enqueue()
  7.3  shaper_loop()                           ← COMMIT: feat: shaper loop
  7.4  start() / stop()                        ← COMMIT: feat: RAII lifecycle
  7.5  Wire into main.cpp                      ← COMMIT: feat: wire callback
  7.6  iperf3 shaping test                     ← COMMIT: test: iperf3 shaping
  PR → merge to main                           ← ✅ GATE

Step 8  — Statistics
  8.0  git checkout -b feature/statistics
  8.1  Class + atomic fields                   ← COMMIT: feat: Statistics class
  8.2  record_accept() / record_drop()         ← COMMIT: feat: record methods
  8.3  report()                                ← COMMIT: feat: report()
  8.4  Stats thread + start() / stop()         ← COMMIT: feat: stats thread
  8.5  Concurrency test                        ← COMMIT: test: atomic counter test
  PR → merge to main

Step 9  — Config & CLI
  9.0  git checkout -b feature/config
  9.1  Config struct                           ← COMMIT: feat: Config struct
  9.2  CLI argument parsing                    ← COMMIT: feat: getopt_long parsing
  9.3  validate()                              ← COMMIT: feat: validate()
  9.4  Startup banner                          ← COMMIT: feat: startup banner
  PR → merge to main

Step 10 — Signal Handling & Shutdown
  10.0 git checkout -b feature/signal-handling
  10.1 sigaction() handlers                    ← COMMIT: feat: SIGINT/SIGTERM
  10.2 Shutdown sequence                       ← COMMIT: feat: ordered shutdown
  10.3 Test + Valgrind                         ← COMMIT: test: graceful shutdown
  PR → merge to main

Step 11 — main.cpp (Full Integration)
  11.0 git checkout -b feature/main-integration
  11.1 Full main()                             ← COMMIT: feat: complete main()
  11.2 Mode dispatch                           ← COMMIT: feat: mode dispatch
  11.3 End-to-end smoke + Release build        ← COMMIT: chore: Release build
  PR → merge to main

Step 12 — Full Testing Pass
  12.0 git checkout -b test/full-testing-pass
  12.1 All unit tests                          ← COMMIT: fix: any failures
  12.2 iperf3 integration (6 tests)            ← COMMIT: test: integration results
  12.3 Valgrind                                ← COMMIT: test: zero leaks
  12.4 ThreadSanitizer                         ← COMMIT: test: zero data races
  12.5 Edge cases                              ← COMMIT: test: edge cases
  12.6 60s stress test                         ← COMMIT: test: stress test
  PR → merge to main

Step 13 — Code Quality
  13.0 git checkout -b chore/code-quality
  13.1 clang-format                            ← COMMIT: style: clang-format
  13.2 clang-tidy                              ← COMMIT: refactor: tidy fixes
  13.3 Zero-warning -Werror build              ← COMMIT: chore: -Werror clean
  13.4 Final review checklist                  ← COMMIT: refactor: final fixes
  PR → merge to main

Step 14 — Docs, GitHub & Presentation
  14.0 git checkout -b docs/final-report-and-readme
  14.1 README.md                               ← COMMIT: docs: complete README
  14.2 Stage report docs                       ← COMMIT: docs: stage1–6 reports
  14.3 git tag v1.0 + push to GitHub           ← 🏷️ RELEASE TAG
  14.4 Presentation slides                     ← COMMIT: docs: presentation
  PR → merge to main

  ✅ PROJECT COMPLETE
```
