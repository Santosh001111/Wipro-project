#!/bin/bash
# Usage: ./setup_iptables.sh [queue-num]
# --queue-bypass: if tbshaper is NOT running, packets are accepted instead of
# being dropped, so a crash cannot cut off your network/SSH session.
set -e
Q="${1:-0}"
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
$SUDO iptables -I OUTPUT -j NFQUEUE --queue-num "$Q" --queue-bypass
echo "[setup] NFQUEUE rule inserted for queue $Q"
