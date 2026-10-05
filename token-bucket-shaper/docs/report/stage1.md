# Stage 1 — Requirements & Environment
**Goal:** a Linux user-space rate limiter using NFQUEUE and a token bucket, with two modes (police = drop excess, shape = delay excess).

**Environment:** Ubuntu/Debian, kernel >= 4.14, `build-essential cmake pkg-config libnetfilter-queue-dev libnfnetlink-dev iptables iperf3 valgrind clang-format clang-tidy`; root / `CAP_NET_ADMIN` to run.

**Acceptance criteria:** measured throughput within +-5 % of the configured rate; 0 TCP retransmits and RTT < 50 ms when shaping; clean shutdown on SIGINT/SIGTERM; no leaks (Valgrind) and no data races (TSan).
