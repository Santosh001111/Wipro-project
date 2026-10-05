# Stage 3 — Implementation
Modules implemented in the order of `project_steps.md`: TokenBucket, Packet, BoundedQueue, NFQueueCapture, Policer, Shaper, Statistics, Config, signal handling, `main()`.

Shutdown order (SIGINT/SIGTERM only set an atomic flag): `capture.stop()` -> `shaper.stop()` (queued packets flushed with NF_ACCEPT) -> `stats.stop()` (final report) -> destroy NFQUEUE handles -> "Clean shutdown complete." -> exit 0.

Robustness points: `poll()` with a 100 ms timeout so stop never hangs on an idle link; 64 KiB+ receive buffer so loopback-sized packets are not truncated; ENOBUFS is logged, not fatal; 4 MiB `SO_RCVBUF`.
