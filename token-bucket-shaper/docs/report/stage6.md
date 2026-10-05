# Stage 6 — Presentation & demo
Suggested 12 slides: 1 title · 2 problem (policing vs shaping) · 3 token-bucket algorithm · 4 architecture · 5 threading model · 6 policer · 7 shaper · 8 statistics & CLI · 9 graceful shutdown · 10 test strategy & results · 11 live demo · 12 limitations / future work.

Demo (<= 5 min): `scripts/setup_iptables.sh` -> police + iperf3 -> shape + iperf3 with parallel `ping` -> `Ctrl+C` (final stats, clean exit) -> `scripts/teardown_iptables.sh`. `RUN_COMMANDS.sh` automates it.
