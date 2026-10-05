# Audit of the original code against project_steps.md (fixed in this version)
See the summary in the hand-over message; every item below was fixed and is covered by a test where one is possible without root.

1. Shaper queue-full path never issued a DROP verdict (packet had been moved into `try_push`) and recorded 0 bytes.
2. Receive buffer was 4096 B: loopback packets up to 64 KiB were truncated, stalling iperf3 over 127.0.0.1.
3. `recv()` blocked forever on an idle queue, so Ctrl+C could hang `capture.stop()`.
4. `--burst 64K` produced 64,000 not 65,536 bytes (plan 9.2); invalid numbers were silently accepted.
5. Statistics: `stop()` waited a full interval; destroying a running Statistics (e.g. when `nfq_open` fails as non-root) called `std::terminate`.
6. Shaper: oversized packets blocked the queue head forever; fixed 100 us busy polling instead of computed sleep; queued packets got no verdict on shutdown.
7. `Packet::accept()/drop()` on a moved-from packet dereferenced a null handle; errors were not logged.
8. Tests: `assert` is compiled out in Release; TB-02/TB-04/TB-06/TB-07 were timing-flaky or did not test what the plan says; no tests for Statistics, Config, Policer, Shaper; `build/LastTestsFailed.log` shows a past failure.
9. `--verbose` was ignored; `--queue 65536` wrapped to 0; `--queue-depth 0` accepted; banner rate was not `1.000`.
10. iptables rule had no `--queue-bypass`; scripts were not executable; `RUN_COMMANDS.sh` ran unconditionally; `SETUP_COMMANDS.txt` had a hard-coded user path.
11. `build/` (with absolute paths from another machine) was shipped; `docs/report` was empty; README incomplete.
