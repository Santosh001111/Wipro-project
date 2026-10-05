#!/bin/bash
# End-to-end demo: iptables rule -> tbshaper -> iperf3 -> SIGINT -> teardown.
# Usage: sudo ./RUN_COMMANDS.sh [police|shape] [rate=1M] [burst=64K] [seconds=10]
set -u
cd "$(dirname "$0")"
MODE="${1:-police}"; RATE="${2:-1M}"; BURST="${3:-64K}"; SECS="${4:-10}"; Q=0

[ "$(id -u)" -eq 0 ] || { echo "Run as root: sudo $0 $*"; exit 1; }
[ -x build/tbshaper ] || { echo "Build first: cmake -B build && cmake --build build"; exit 1; }
command -v iperf3 >/dev/null || { echo "iperf3 not installed"; exit 1; }

PID=""; SRV=""
cleanup() {
    [ -n "$PID" ] && kill -INT "$PID" 2>/dev/null && wait "$PID" 2>/dev/null
    [ -n "$SRV" ] && kill "$SRV" 2>/dev/null
    ./scripts/teardown_iptables.sh "$Q" 2>/dev/null || true
}
trap cleanup EXIT

./scripts/setup_iptables.sh "$Q"
./build/tbshaper --mode "$MODE" --rate "$RATE" --burst "$BURST" --queue "$Q" --stats-interval 2000 &
PID=$!
sleep 1
kill -0 "$PID" 2>/dev/null || { echo "tbshaper failed to start"; PID=""; exit 1; }

iperf3 -s -1 >/dev/null 2>&1 & SRV=$!
sleep 1
[ "$MODE" = "shape" ] && ping -c 10 -i 0.5 127.0.0.1 | tail -2 &
iperf3 -c 127.0.0.1 -t "$SECS" -b 100M | tail -8
wait
echo "--- sending SIGINT to tbshaper ---"
kill -INT "$PID"; wait "$PID"; echo "exit code: $?"; PID=""
