# Hardware test rig

Local rig that turns a laptop into a controllable LAN target for the
Flipper ENC28J60 Ethernet app. Used to validate every phase of the
F0 refactor on real hardware; F1 development should keep using it.

## Topology

```
  Laptop                                        Flipper Zero
  ┌────────────────────┐                        ┌──────────────┐
  │ enx*  10.10.10.1   │  ← USB-Ethernet ──→    │ ENC28J60     │
  │  + dnsmasq DHCP    │     dongle             │  (DHCP client)│
  │  + python http.svr │                        │              │
  │  + tcpdump capture │                        │              │
  └────────────────────┘                        └──────────────┘
```

Subnet `10.10.10.0/24`. Laptop is the gateway, DHCP server, and the
HTTP target for port-scan validation. tcpdump records every frame
to a pcap so you can replay traffic in Wireshark when something
behaves strangely on-device.

## Prerequisites

- A USB-Ethernet adapter on the laptop. Find its kernel name with
  `ip link show` — it's usually `enx<MAC>` (e.g. `enx000ec6be9018`).
- `sudo` (for `ip`, `dnsmasq`, `tcpdump`).
- `dnsmasq`, `tcpdump`, `python3`. On Ubuntu/Debian:
  `sudo apt install dnsmasq tcpdump python3`.
- For `flipper-*.py`: `python3-serial` (`sudo apt install python3-serial`).

## Bring up

```bash
sudo bash tools/test-rig/up.sh
```

Override defaults via env vars:

```bash
sudo IFACE=enx112233445566 LAB_NET_GW=192.168.55.1 \
     bash tools/test-rig/up.sh
```

`up.sh` runs in the foreground; press `Ctrl-C` to tear everything
down (it traps `INT/TERM` and runs `down.sh`). Or close the terminal
and run `down.sh` separately.

| Env var          | Default                  | Notes                              |
|------------------|--------------------------|------------------------------------|
| `IFACE`          | `enx000ec6be9018`        | Output of `ip link show`           |
| `LAB_NET_GW`     | `10.10.10.1`             | Laptop IP on the rig subnet        |
| `DHCP_RANGE_FROM`| `10.10.10.50`            | DHCP lease range start             |
| `DHCP_RANGE_TO`  | `10.10.10.150`           | DHCP lease range end               |
| `PCAP_FILE`      | `/tmp/eth-testrig.pcap`  | tcpdump output                     |
| `LOG_DIR`        | `/tmp/eth-testrig`       | dnsmasq, http.server, tcpdump logs |

## What to test on the Flipper

`up.sh` prints this on launch, but for reference:

1. **Get IP** → DORA gets a lease in the configured range.
2. **Scan Hosts** → `LAB_NET_GW` (laptop) appears in the list.
3. **Ping** → `LAB_NET_GW` replies arrive at ~1 pps.
4. **Ports Scanner** → TCP, target `LAB_NET_GW`, port 80, range 5 → port 80 reports OPEN.
5. **OS Detector** → `LAB_NET_GW` → should detect Linux. Labelled
   EXPERIMENTAL in v2.0; results may be heuristic.
6. **Sniffer** → captures broadcast traffic; pcap on the Flipper SD.

## Tear down

```bash
bash tools/test-rig/down.sh
```

Kills dnsmasq / http.server / tcpdump (by pid file first, then by
pattern as fallback) and brings the interface back down. The pcap
stays at `$PCAP_FILE`.

## Live monitors

In separate terminals while `up.sh` runs:

```bash
tail -f /tmp/eth-testrig/dnsmasq.log         # DHCP transactions
tail -f /tmp/eth-testrig/http.log            # HTTP requests
tcpdump -r /tmp/eth-testrig.pcap -nn -tttt   # replay frames
wireshark /tmp/eth-testrig.pcap              # GUI replay
```

## Flipper CLI helpers

`flipper-log.py` and `flipper-cmd.py` talk to the Flipper's CLI over
the second USB CDC-ACM endpoint (`/dev/ttyACM1` typically) at
230400 baud. Useful for grabbing `top`, `free`, `help`, or streaming
`log debug` output while the app runs.

```bash
python3 tools/test-rig/flipper-log.py 5         # capture 5 s of log
python3 tools/test-rig/flipper-cmd.py "top" 2   # process snapshot
python3 tools/test-rig/flipper-cmd.py "free"    # memory stats
```

If the device hops endpoints across reboots, pass `--port`:

```bash
python3 tools/test-rig/flipper-cmd.py "help" --port /dev/ttyACM2
```

These scripts were instrumental during F0.4a debugging when the
device froze on entering some scenes — log streaming was the only
way to see the chip's state without SWD (PA14 is now consumed by the
INT line, see `docs/HARDWARE.md`).

## Known gotchas

- **dnsmasq `--no-ping`** is set on purpose. Without it, dnsmasq
  ARP/ICMP-probes a candidate IP before sending OFFER, which used to
  push past the Flipper's old 3 s DORA timeout (B-1, fixed in F0.7
  by raising the timeout to 10 s). The flag keeps timing predictable
  even with the larger window.
- **tcpdump permission denied on stale pcap**: kernel
  `fs.protected_regular` blocks an unprivileged user `tcpdump` from
  overwriting a pcap dropped by a previous run owned by another
  user. `up.sh` does `rm -f $PCAP_FILE` before starting.
- **Flipper not at `/dev/ttyACM*`**: USB can be flaky through the
  ENC28J60 dongle's hub. If the device disappears, replug; the OS
  reassigns the same `enx*` name on the rig but the Flipper may
  end up on a different `ttyACM`.
