#!/usr/bin/env bash
# Hardware test rig for the Flipper ENC28J60 Ethernet app.
#
# Brings up:
#   - laptop USB-Ethernet interface ($IFACE) at 10.10.10.1/24
#   - dnsmasq DHCP on that interface (no DNS, lease range 10.10.10.50–150)
#   - Python http.server on :80 (target for port-scan validation)
#   - tcpdump capture to $PCAP_FILE
#
# Use: sudo bash tools/test-rig/up.sh
# Stop: bash tools/test-rig/down.sh   (or Ctrl-C; up.sh traps it)
#
# Configurable via env vars:
#   IFACE        — USB-Ethernet adapter name. Default: enx000ec6be9018
#                  Find yours with `ip link show` (look for the enx*
#                  device that appears when you plug the dongle).
#   LAB_NET_GW   — laptop IP on the rig subnet. Default: 10.10.10.1
#   PCAP_FILE    — capture output. Default: /tmp/eth-testrig.pcap
#   LOG_DIR      — where dnsmasq/http/tcpdump logs land. Default: /tmp/eth-testrig
#
# Prerequisites: sudo, dnsmasq, tcpdump, python3.
set -euo pipefail

IFACE="${IFACE:-enx000ec6be9018}"
LAB_NET_GW="${LAB_NET_GW:-10.10.10.1}"
LAB_IP="${LAB_NET_GW}/24"
DHCP_RANGE_FROM="${DHCP_RANGE_FROM:-10.10.10.50}"
DHCP_RANGE_TO="${DHCP_RANGE_TO:-10.10.10.150}"
PCAP_FILE="${PCAP_FILE:-/tmp/eth-testrig.pcap}"
LOG_DIR="${LOG_DIR:-/tmp/eth-testrig}"
mkdir -p "$LOG_DIR"

echo "[1/4] Bringing up $IFACE with $LAB_IP"
ip addr add "$LAB_IP" dev "$IFACE" 2>/dev/null || true
ip link set "$IFACE" up

echo "[2/4] Starting dnsmasq DHCP on $IFACE (range $DHCP_RANGE_FROM..$DHCP_RANGE_TO)"
# --no-ping: dnsmasq's default ICMP probe before OFFER conflicts with
# the Flipper's DORA timing (B-1 history). The 10s app-side timeout
# in F0.7 also helps; leaving --no-ping in for predictable timing.
dnsmasq \
  --interface="$IFACE" \
  --bind-interfaces \
  --no-daemon \
  --port=0 \
  --dhcp-range="$DHCP_RANGE_FROM,$DHCP_RANGE_TO,12h" \
  --dhcp-option=3,"$LAB_NET_GW" \
  --dhcp-option=6,"$LAB_NET_GW" \
  --no-ping \
  --log-dhcp \
  --log-facility="$LOG_DIR/dnsmasq.log" \
  --pid-file="$LOG_DIR/dnsmasq.pid" \
  &
DNSMASQ_PID=$!

echo "[3/4] Starting HTTP server on $LAB_NET_GW:80"
( cd "$LOG_DIR" && python3 -m http.server 80 --bind "$LAB_NET_GW" \
    > "$LOG_DIR/http.log" 2>&1 ) &
HTTP_PID=$!
echo "$HTTP_PID" > "$LOG_DIR/http.pid"

echo "[4/4] Starting tcpdump capture to $PCAP_FILE"
# Remove stale pcap (kernel fs.protected_regular blocks user `tcpdump`
# from overwriting a pcap dropped by a previous run owned by another
# user).
rm -f "$PCAP_FILE"
tcpdump -i "$IFACE" -w "$PCAP_FILE" -U \
  > "$LOG_DIR/tcpdump.log" 2>&1 &
TCPDUMP_PID=$!
echo "$TCPDUMP_PID" > "$LOG_DIR/tcpdump.pid"

echo "$DNSMASQ_PID" > "$LOG_DIR/dnsmasq.shellpid"

echo ""
echo "=========================================="
echo "Test rig UP. Connect Flipper cable now."
echo ""
echo "Live monitors (in separate terminals):"
echo "  tail -f $LOG_DIR/dnsmasq.log    # see DHCP transactions"
echo "  tail -f $LOG_DIR/http.log       # see HTTP requests"
echo "  tcpdump -r $PCAP_FILE -nn -tttt # replay captured frames"
echo ""
echo "On the Flipper:"
echo "  1. Get IP            → DORA gets a lease in $DHCP_RANGE_FROM..$DHCP_RANGE_TO"
echo "  2. Scan Hosts        → Start IP $LAB_NET_GW, range 5 → laptop appears"
echo "  3. Ping              → $LAB_NET_GW → replies arrive"
echo "  4. Ports Scanner     → TCP, target $LAB_NET_GW, port 80, range 5 → 80 OPEN"
echo "  5. OS Detector       → $LAB_NET_GW → should detect Linux (EXPERIMENTAL)"
echo ""
echo "Stop the rig: bash tools/test-rig/down.sh   (or Ctrl-C here)"
echo "=========================================="

# Stay in the foreground so Ctrl-C tears everything down via the trap.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
trap "bash '$SCRIPT_DIR/down.sh'; exit 0" INT TERM
wait
