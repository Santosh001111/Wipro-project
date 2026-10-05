#!/bin/bash
# Usage: ./teardown_iptables.sh [queue-num]   (must match the setup call)
set -e
Q="${1:-0}"
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
$SUDO iptables -D OUTPUT -j NFQUEUE --queue-num "$Q" --queue-bypass
echo "[teardown] NFQUEUE rule removed for queue $Q"
