# Stage 2 — Design
Pipeline: `iptables OUTPUT -> NFQUEUE -> NFQueueCapture -> {Policer | Shaper} -> verdict`.

* **TokenBucket** — capacity = burst, fill rate = rate/8 B/s, lazy refill on every call, one mutex. `time_until()` lets the shaper sleep for exactly as long as needed.
* **Packet** — move-only; exactly one verdict per packet (moved-from packets are inert).
* **BoundedQueue** — blocking `push/pop`, non-blocking `try_push`, `shutdown()` that still lets consumers drain.
* **Shaper** — capture thread only enqueues (never blocks); one pacing thread releases packets as tokens allow.
* **Statistics** — relaxed atomics on the hot path; a reporter thread off the hot path.
* **Design decisions:** K/M/G are decimal for `--rate` and binary for `--burst`; packets larger than the burst are dropped (they can never conform); `--queue-bypass` in the iptables rule so a crash cannot black-hole traffic.
