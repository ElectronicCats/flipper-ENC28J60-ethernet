# Bug backlog (deferred fixes)

Bugs and rough edges discovered during F0 hardware testing that are NOT
in the scope of the sub-phase that found them. Each entry says where it
will be fixed.

---

## B-1 — DHCP DORA timeout too short (3s)

**Found:** F0.3 hardware testing against a `dnsmasq` server doing standard
ARP/ICMP collision check before sending DHCPOFFER.

**Location:** `EthernetAppDemo/modules/dhcp_protocol.c` — 3 s hardcoded
timeout while waiting for OFFER. Confirmed in F0.0 audit.

**Symptom:** Flipper sends `DHCPDISCOVER`, server takes ~3 s to send
`DHCPOFFER` (because of its own collision-check delay), Flipper has
already given up by the time OFFER arrives, retries DISCOVER 40 s later.
DORA never completes; user sees no IP. Workaround on the test rig:
`dnsmasq --no-ping` skips the server's collision check so OFFER comes
back in <10 ms.

**Severity:** High — affects compatibility with most production DHCP
servers in the wild (almost all do collision-check by default).

**Fix in:** F0.7 bug fixes consolidation. Either bump the timeout to
~10 s or rework into an async wait that doesn't constrain server
latency.

---

## B-2 — SnifferScene busy-loop on link-up wait blocks BACK

**Found:** F0.3 hardware testing — pressed OK to start sniffing,
ENC28J60 PHY momentarily reported link-down (probably a glitch from a
recent state transition), Flipper got stuck in
`while(!is_link_up(ethernet)) { if (BACK) break; }` busy-loop. The
BACK check is inside the loop but the SPI read of `PHSTAT2` (via
`read_Phy_byte`) had hung, so the BACK comparison was never reached.
User had to hard-reset the Flipper (BACK + LEFT 10 s).

**Location:** `EthernetAppDemo/scenes/SnifferScene.c:191-196`.

**Severity:** Medium — only triggers when the PHY register read hangs,
which is rare. But when it happens, the user is fully locked out
without a hard reset. Pre-existing; not a regression.

**Fix in:** F0.4 (RX dispatch decoupling) replaces the entire
"suspend-worker-and-poll" pattern with a single shared dispatcher and
event-driven scenes — the busy-loop disappears as a byproduct.
Alternatively a one-line `furi_delay_ms(1)` inside the loop in F0.6
would give the kernel a chance to interrupt. Choosing F0.4 since the
arch fix subsumes it.

---

## B-3 — "Dead window" between sniffer entry and capture start

**Found:** F0.3 hardware testing — generated traffic from laptop while
SnifferScene was in its OK-wait + link-up-wait phase. Worker thread
already suspended at scene entry; promiscuous mode + capture only
enabled after the user confirms with OK and link-up succeeds. During
that window the chip drops everything. ICMP from the laptop saw 100 %
packet loss because the worker (the auto-replier) was suspended and
the sniffer was not yet capturing.

**Location:** `EthernetAppDemo/scenes/SnifferScene.c` (entry sequence).

**Severity:** Low — only meaningful in the test rig where you need
captures of arbitrary external traffic that arrives *during* setup.
Real users don't notice because they start the sniffer first and then
go cause traffic.

**Fix in:** F0.4. With rx_dispatch, there's no "suspend worker" step;
the dispatcher is always running. Sniffer just registers a
capture-everything handler.

---

## B-4 — UDP scanner doesn't distinguish open vs closed

**Found:** F0.3 hardware testing — UDP scan of closed ports gets
ICMP-port-unreachable replies but the predicate ignores them (only
matches actual UDP responses). Result: `nmap -sU --reason` would say
`open|filtered` for every port, which is what the Flipper effectively
reports (no port appears open even when the host is responsive).
Pre-existing behavior, not a regression. To distinguish, the predicate
needs to also match ICMP type 3 code 3 with the embedded UDP header
referring to the scan port.

**Location:** `EthernetAppDemo/modules/udp_module.c` —
`udp_scan_match()` predicate (added in F0.3d).

**Severity:** Low — UDP scanning is inherently unreliable. The
overall feature works enough to report responses from active services.

**Fix in:** F1 UDP service scan rewrite (per roadmap §4 F1.14
"Payload-aware UDP service scan"). Don't bolt on ICMP-unreachable
detection to the current generic UDP probe; replace with per-protocol
payloads (DNS, NTP, SNMP, NetBIOS, mDNS, SSDP) that get real replies.

---

## B-5 — LoadingView clock animation freezes during ARP scan

**Found:** F0.3a hardware testing — during a 30-IP ARP scan the
spinning clock animation in the LoadingView stops updating. Scan still
completes correctly. Plausible cause: the extra GPIO read in
`scanner_cancel_requested` shifts the worker thread's yielding pattern
just enough for the GUI thread's render timer to lag.

**Location:** Cosmetic; the offending code is the busy-poll inside
`scanner_wait_for_packet` (`libraries/scanner/scanner_session.c`).

**Severity:** Cosmetic.

**Fix in:** F0.4 (rx_dispatch eliminates the polling pattern).

---

(Add new entries below as they're found.)
