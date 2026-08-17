#!/usr/bin/env bash
# Tear down the eth test rig started by up.sh.
#
# Configurable via env vars (must match what up.sh saw, default is fine):
#   IFACE     — USB-Ethernet adapter. Default: enx000ec6be9018
#   LOG_DIR   — pid/log dir. Default: /tmp/eth-testrig
#   PCAP_FILE — capture path. Default: /tmp/eth-testrig.pcap
LOG_DIR="${LOG_DIR:-/tmp/eth-testrig}"
IFACE="${IFACE:-enx000ec6be9018}"
PCAP_FILE="${PCAP_FILE:-/tmp/eth-testrig.pcap}"

echo "Stopping test rig..."
sudo pkill -F "$LOG_DIR/dnsmasq.pid"   2>/dev/null || true
sudo pkill -F "$LOG_DIR/http.pid"      2>/dev/null || true
sudo pkill -F "$LOG_DIR/tcpdump.pid"   2>/dev/null || true

# Belt-and-suspenders: kill anything left.
sudo pkill -f "dnsmasq.*$IFACE"           2>/dev/null || true
sudo pkill -f "python3 -m http.server 80" 2>/dev/null || true
sudo pkill -f "tcpdump -i $IFACE"         2>/dev/null || true

echo "Bringing $IFACE down..."
sudo ip addr flush dev "$IFACE" 2>/dev/null || true
sudo ip link set "$IFACE" down 2>/dev/null || true

echo "Test rig DOWN. PCAP saved at $PCAP_FILE"
echo "Open with: wireshark $PCAP_FILE"
