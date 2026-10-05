# Stage 4 — Testing
## Unit tests (no root) — `ctest --test-dir build`
| Suite | Cases |
|---|---|
| token_bucket | TB-01..TB-08 from the plan + TB-09 (`reset`, `time_until`) |
| bounded_queue | BQ-01..BQ-06 + BQ-07 (failed `try_push` leaves the item intact) |
| statistics | 4 threads x 1000 calls == 4000 exactly; prompt `stop()`; destructor safety |
| config | K/M/G semantics, bad input rejected, all validation errors reported |
| policer_shaper | verdict-once, police accept/drop counts, shaper pacing rate (+FIFO), queue overflow, oversize packet, flush on stop (uses a stubbed `nfq_set_verdict`) |

Verified in development: all suites pass in Debug and Release (`-DNDEBUG`), under ThreadSanitizer (0 races) and ASan/UBSan (clean), and 30 repeated runs without a failure.

## Integration tests — run on a Linux host with root
| ID | Mode | Rate | Criterion | Result |
|---|---|---|---|---|
| IT-01 | Police | 1 Mbit/s | 1.0 ±5 % | Run: `sudo ./RUN_COMMANDS.sh police 1M 64K 10` |
| IT-02 | Police | 10 Mbit/s | 10.0 ±5 % | Run: `sudo ./RUN_COMMANDS.sh police 10M 64K 10` |
| IT-03 | Shape | 1 Mbit/s | 1.0, 0 retransmits | Run: `sudo ./RUN_COMMANDS.sh shape 1M 64K 10` |
| IT-04 | Shape | 10 Mbit/s | 10.0, smooth | Run: `sudo ./RUN_COMMANDS.sh shape 10M 64K 10` |
| IT-05 | Police UDP | 512 Kbit/s | 512 Kbit/s, drops | Run: `sudo ./RUN_COMMANDS.sh police 512K 64K 10` |
| IT-06 | Shape | 512 Kbit/s | jitter < 5 ms, 0 retransmits | Run: `sudo ./RUN_COMMANDS.sh shape 512K 64K 10` |

Also to run there: Valgrind (`0 errors`), TSan build under iperf3 load, edge cases (burst = 1500, `--queue-depth 4`, SIGINT under load, 9000-byte frame with burst 1500), 60 s UDP stress.
