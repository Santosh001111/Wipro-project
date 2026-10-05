# Token-Bucket Shaper (`tbshaper`)

A Linux user-space **traffic policer and shaper** built with NFQUEUE and a token-bucket rate limiter. Intercepted outgoing IPv4 packets are either **dropped** (police mode) or **buffered and paced** (shape mode) to stay within a configured rate limit.

> Developed as a Wipro training project using C++17, CMake, and Linux Netfilter.

---

## Table of Contents

- [Architecture](#architecture)
- [Prerequisites](#prerequisites)
- [Build](#build)
- [CLI Reference](#cli-reference)
- [Usage](#usage)
- [Running Tests](#running-tests)
- [Code Quality](#code-quality)
- [Known Limitations](#known-limitations)
- [Project Structure](#project-structure)

---

## Architecture

```
 iptables OUTPUT --> NFQUEUE --> NFQueueCapture (capture thread, poll+recv)
                                       |
                    +------------------+-------------------+
                    | police mode                          | shape mode
                    v                                      v
              Policer::process                       Shaper::enqueue
                    |                                      | try_push (full/oversize -> DROP)
        TokenBucket::consume                               v
          ok -> NF_ACCEPT                           BoundedQueue<Packet>
          no -> NF_DROP                                    |  pop
                                                    shaper thread: wait for tokens
                                                           v
                                                       NF_ACCEPT

 Statistics: atomic counters, reporter thread prints every --stats-interval ms
```

| Module | Role |
|---|---|
| `TokenBucket` | Mutex-protected bucket; lazy refill from `steady_clock` |
| `Packet` | Move-only handle; guarantees exactly one verdict per packet |
| `BoundedQueue<T>` | Blocking/non-blocking bounded FIFO with `shutdown()` |
| `NFQueueCapture` | Owns the NFQUEUE binding and receive thread |
| `Policer` / `Shaper` | The two operating modes |
| `Statistics` | Lock-free counters + periodic report thread |
| `Config` | `getopt_long` CLI parsing and validation |

---

## Prerequisites

**OS:** Linux, kernel >= 4.14, root or `CAP_NET_ADMIN` to run.

**Ubuntu / Debian:**

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config \
    libnetfilter-queue-dev libnfnetlink-dev \
    iptables iperf3 valgrind clang-format clang-tidy
```

---

## Build

```bash
# Clone / enter the project directory
cd token-bucket-shaper

# Configure and build (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run unit tests (no root required)
ctest --test-dir build --output-on-failure
```

**Expected test output:**
```
100% tests passed, 0 tests failed out of 5
```

**Optional sanitizer builds:**

```bash
# Thread sanitizer
cmake -B build-tsan -DTBSHAPER_SANITIZE=thread
cmake --build build-tsan
ctest --test-dir build-tsan --output-on-failure

# Address + UB sanitizer
cmake -B build-asan -DTBSHAPER_SANITIZE=address,undefined
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

---

## CLI Reference

| Flag | Default | Meaning |
|---|---|---|
| `--rate <N[K/M/G]>` | required | bits/s — **decimal** suffixes (`1M` = 1,000,000) |
| `--burst <N[K/M/G]>` | required | bytes — **binary** suffixes (`64K` = 65,536); must be >= 1500 |
| `--mode police\|shape` | `police` | drop excess packets vs. delay them |
| `--queue <N>` | `0` | NFQUEUE number (0–65535) |
| `--queue-depth <N>` | `1024` | shaping queue size in packets (>= 1) |
| `--stats-interval <ms>` | `5000` | stats report period (>= 100 ms) |
| `--verbose` | off | print one line to stderr per packet |
| `--help` | | print usage and exit 0 |

---

## Usage

### Automated demo (recommended)

```bash
# Police mode (drops excess)
sudo ./RUN_COMMANDS.sh police

# Shape mode (paces excess)
sudo ./RUN_COMMANDS.sh shape

# Custom: shape at 10 Mbit/s, 64 KiB burst, 20-second test
sudo ./RUN_COMMANDS.sh shape 10M 64K 20
```

### Manual — three terminals

**Terminal 1 — start the shaper:**

```bash
sudo ./scripts/setup_iptables.sh 0
sudo ./build/tbshaper --mode police --rate 1M --burst 64K --queue 0
```

**Terminal 2 — start the iperf3 server:**

```bash
iperf3 -s
```

**Terminal 3 — generate traffic:**

```bash
iperf3 -c 127.0.0.1 -t 10 -b 100M
```

**Stop and teardown:**

```bash
# Press Ctrl+C in Terminal 1, then:
sudo ./scripts/teardown_iptables.sh 0
```

### Sample output

```
[tbshaper] Mode           : POLICING
[tbshaper] Rate           : 1.000 Mbit/s
[tbshaper] Burst          : 65536 bytes
[tbshaper] Queue Number   : 0
[tbshaper] Queue Depth    : 1024 packets
[tbshaper] Stats Interval : 5000 ms
[Stats   5s] pass=412  drop=3821  bytes_drop=5506074  throughput=0.998 Mbit/s
[tbshaper] Clean shutdown complete.
```

---

## Running Tests

### Unit tests (no root required)

```bash
ctest --test-dir build --output-on-failure
```

| Suite | Cases |
|---|---|
| `token_bucket` | TB-01..TB-09 — refill, consume, thread safety, `time_until`, `reset` |
| `bounded_queue` | BQ-01..BQ-07 — push, pop, shutdown, concurrency, move-only types |
| `statistics` | ST-01..ST-03 — atomic counters, prompt `stop()`, destructor safety |
| `config` | K/M/G parsing, bad-input rejection, all validation errors reported |
| `policer_shaper` | Verdict-once, accept/drop counts, pacing rate, queue overflow, flush on stop |

### Integration tests (Linux host + root)

```bash
# Police mode — throttle to 1 Mbit/s
sudo ./RUN_COMMANDS.sh police 1M 64K 10

# Shape mode — smooth at 10 Mbit/s
sudo ./RUN_COMMANDS.sh shape 10M 64K 10

# Rebuild after source changes
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Code Quality

```bash
# Format all source files (requires clang-format)
clang-format -i src/*.cpp include/*.hpp tests/*.cpp

# Static analysis (requires clang-tidy)
clang-tidy src/*.cpp -- -Iinclude $(pkg-config --cflags libnetfilter_queue)

# Memory check (requires valgrind + root)
sudo valgrind --leak-check=full --track-origins=yes \
    ./build/tbshaper --mode police --rate 1M --burst 64K --queue 0
```

Build flags enabled by default: `-Wall -Wextra -Wpedantic -Werror`.

---

## Known Limitations

- **IPv4 only** — bound to `AF_INET`; the iptables rule is on `OUTPUT` and matches all local traffic including loopback.
- **IP-layer byte counting** — rate is measured on IP-layer bytes (headers included), so `iperf3` payload throughput reads slightly lower than the configured rate.
- **Oversized packets in shape mode** — a packet larger than `--burst` can never obtain enough tokens and is dropped immediately. Set `--burst` >= largest packet size (loopback MTU is 65,536 bytes).
- **Flush-on-shutdown** — packets queued in shape mode on `SIGINT` are released immediately (NF_ACCEPT) rather than being paced.
- **Thread-safety note** — verdicts are issued from the shaper thread while the capture thread shares the same netlink handle; this is the standard NFQUEUE pattern but not formally documented as thread-safe by `libnetfilter_queue`.

---

## Project Structure

```
token-bucket-shaper/
├── CMakeLists.txt          # Build configuration
├── RUN_COMMANDS.sh         # Automated end-to-end demo script
├── README.md               # This file
├── .clang-format           # Code style configuration
├── .gitignore
│
├── include/                # Public C++ headers
│   ├── token_bucket.hpp
│   ├── packet.hpp
│   ├── bounded_queue.hpp
│   ├── nfqueue_capture.hpp
│   ├── policer.hpp
│   ├── shaper.hpp
│   ├── statistics.hpp
│   └── config.hpp
│
├── src/                    # C++ source files
│   ├── main.cpp
│   ├── token_bucket.cpp
│   ├── packet.cpp
│   ├── policer.cpp
│   ├── shaper.cpp
│   ├── nfqueue_capture.cpp
│   ├── statistics.cpp
│   └── config.cpp
│
├── tests/                  # Unit tests (no root required)
│   ├── CMakeLists.txt
│   ├── test_util.hpp
│   ├── test_token_bucket.cpp
│   ├── test_bounded_queue.cpp
│   ├── test_statistics.cpp
│   ├── test_config.cpp
│   └── test_policer_shaper.cpp
│
├── scripts/                # iptables helper scripts
│   ├── setup_iptables.sh
│   └── teardown_iptables.sh
│
└── docs/                   # Documentation and reports
    ├── AUDIT.md
    ├── project_steps.md
    ├── Wipro Project Commands.txt
    ├── to_run.txt
    └── report/
        ├── stage1.md
        ├── stage2.md
        ├── stage3.md
        ├── stage4.md
        ├── stage5.md
        └── stage6.md
```
