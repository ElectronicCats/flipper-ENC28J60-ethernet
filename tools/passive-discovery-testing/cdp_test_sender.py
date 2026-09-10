#!/usr/bin/env python3
"""
cdp_test_sender.py

Generate realistic Cisco Discovery Protocol (CDPv2) Ethernet frames for
lab testing of passive-discovery implementations.

Requirements:
    python3 -m pip install scapy

Typical use:
    sudo python3 cdp_test_sender.py -i eth0 --show
    sudo python3 cdp_test_sender.py -i eth0 --count 0 --interval 5
    python3 cdp_test_sender.py -i eth0 --pcap cdp_test.pcap --no-send

Notes:
- CDP destination MAC: 01:00:0c:cc:cc:cc
- IEEE 802.3 + LLC/SNAP
- Cisco OUI: 00:00:0c
- SNAP protocol ID: 0x2000
- CDPv2 default hold time: 180 seconds
"""

import argparse
import sys
import time
from ipaddress import IPv4Address

try:
    from scapy.all import Ether, LLC, SNAP, get_if_hwaddr, sendp, wrpcap
    from scapy.contrib.cdp import (
        CDPAddrRecordIPv4,
        CDPMsgAddr,
        CDPMsgCapabilities,
        CDPMsgDeviceID,
        CDPMsgDuplex,
        CDPMsgMgmtAddr,
        CDPMsgNativeVLAN,
        CDPMsgPlatform,
        CDPMsgPortID,
        CDPMsgSoftwareVersion,
        CDPv2_HDR,
    )
except ImportError as exc:
    print(
        "ERROR: Scapy with the CDP contrib module is required.\n"
        "Install it with:\n"
        "    python3 -m pip install scapy\n",
        file=sys.stderr,
    )
    raise SystemExit(2) from exc


CDP_DST = "01:00:0c:cc:cc:cc"
CISCO_OUI = 0x00000C
CDP_SNAP_PID = 0x2000


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Send realistic CDPv2 advertisements for an isolated Ethernet lab."
    )
    p.add_argument("-i", "--interface", required=True, help="Linux interface used to transmit.")
    p.add_argument("--src-mac", help="Source MAC. Defaults to the selected interface MAC.")
    p.add_argument("--dst-mac", default=CDP_DST, help="Destination MAC. Default is the Cisco CDP multicast address.")
    p.add_argument(
        "--snap-pid",
        type=lambda x: int(x, 0),
        default=CDP_SNAP_PID,
        help="SNAP protocol ID. Default 0x2000 (CDP); useful for negative tests.",
    )

    p.add_argument("--device-id", default="LAB-C2960X-01", help="CDP Device-ID.")
    p.add_argument("--port-id", default="GigabitEthernet1/0/24", help="Advertised CDP Port-ID.")
    p.add_argument("--ip", default="192.0.2.10", help="Advertised IPv4 management address.")
    p.add_argument(
        "--platform",
        default="cisco WS-C2960X-48FPS-L",
        help="CDP Platform TLV.",
    )
    p.add_argument(
        "--software",
        default="Cisco IOS Software, C2960X Software (C2960X-UNIVERSALK9-M), Version 15.2(7)E",
        help="CDP Software Version TLV.",
    )
    p.add_argument("--native-vlan", type=int, default=1, help="Native VLAN TLV (1-4094).")
    p.add_argument(
        "--capabilities",
        type=lambda x: int(x, 0),
        default=0x28,
        help="CDP capability bitmap. Default 0x28 = Switch + IGMP-capable.",
    )
    p.add_argument(
        "--duplex",
        choices=("full", "half"),
        default="full",
        help="Advertised duplex mode.",
    )
    p.add_argument("--ttl", type=int, default=180, help="CDP hold time in seconds (1-255).")
    p.add_argument(
        "--bad-checksum",
        action="store_true",
        help="Force the CDP checksum to 0x0000 for a negative parser test.",
    )

    p.add_argument(
        "--count",
        type=int,
        default=1,
        help="Number of frames to send. 0 means continuously until Ctrl-C.",
    )
    p.add_argument(
        "--interval",
        type=float,
        default=60.0,
        help="Seconds between frames when count is greater than 1 or 0.",
    )
    p.add_argument(
        "--pcap",
        help="Also write the generated frame to this PCAP file.",
    )
    p.add_argument(
        "--no-send",
        action="store_true",
        help="Build/inspect/write the frame but do not transmit it.",
    )
    p.add_argument(
        "--show",
        action="store_true",
        help="Display the fully built frame and decoded CDP TLVs before sending.",
    )
    return p.parse_args()


def validate(args: argparse.Namespace) -> None:
    try:
        IPv4Address(args.ip)
    except ValueError as exc:
        raise SystemExit(f"Invalid IPv4 address: {args.ip}") from exc

    if not 1 <= args.native_vlan <= 4094:
        raise SystemExit("--native-vlan must be between 1 and 4094.")
    if not 1 <= args.ttl <= 255:
        raise SystemExit("--ttl must be between 1 and 255.")
    if args.count < 0:
        raise SystemExit("--count must be >= 0.")
    if args.interval < 0:
        raise SystemExit("--interval must be >= 0.")


def build_cdp_frame(args: argparse.Namespace):
    src_mac = args.src_mac or get_if_hwaddr(args.interface)

    address_record = CDPAddrRecordIPv4(addr=args.ip)

    tlvs = [
        CDPMsgDeviceID(val=args.device_id.encode()),
        CDPMsgAddr(addr=[address_record]),
        CDPMsgPortID(iface=args.port_id.encode()),
        CDPMsgCapabilities(cap=args.capabilities),
        CDPMsgSoftwareVersion(val=args.software.encode()),
        CDPMsgPlatform(val=args.platform.encode()),
        CDPMsgNativeVLAN(vlan=args.native_vlan),
        CDPMsgDuplex(duplex=1 if args.duplex == "full" else 0),
        # CDPv2 commonly carries a separate management-address TLV as well.
        CDPMsgMgmtAddr(addr=[CDPAddrRecordIPv4(addr=args.ip)]),
    ]

    cdp = CDPv2_HDR(
        vers=2,
        ttl=args.ttl,
        cksum=0x0000 if args.bad_checksum else None,
        msg=tlvs,
    )

    # CDP on Ethernet uses IEEE 802.3 length framing, not Ethernet-II EtherType.
    # LLC: AA-AA-03, SNAP OUI: 00-00-0C, protocol ID: 0x2000.
    l2_payload = (
        LLC(dsap=0xAA, ssap=0xAA, ctrl=0x03)
        / SNAP(OUI=CISCO_OUI, code=args.snap_pid)
        / cdp
    )

    # Ether.type values <= 1500 represent the IEEE 802.3 payload length.
    frame = Ether(
        dst=args.dst_mac,
        src=src_mac,
        type=len(bytes(l2_payload)),
    ) / l2_payload

    # Force final serialization once so Scapy calculates dependent fields,
    # including the CDP checksum.
    bytes(frame)
    return frame


def main() -> int:
    args = parse_args()
    validate(args)

    frame = build_cdp_frame(args)

    print(f"Interface : {args.interface}")
    print(f"Source MAC: {frame.src}")
    print(f"Dst MAC   : {args.dst_mac}")
    print(f"SNAP PID  : 0x{args.snap_pid:04x}")
    print(f"Checksum  : {'forced bad (0x0000)' if args.bad_checksum else 'auto/valid'}")
    print(f"Device-ID : {args.device_id}")
    print(f"Port-ID   : {args.port_id}")
    print(f"Mgmt IPv4 : {args.ip}")
    print(f"VLAN      : {args.native_vlan}")
    print(f"TTL       : {args.ttl}s")
    print(f"Frame len : {len(bytes(frame))} bytes (without Ethernet FCS)")

    if args.show:
        print("\nDecoded frame:")
        frame.show2()

    if args.pcap:
        wrpcap(args.pcap, [frame])
        print(f"\nPCAP written to: {args.pcap}")

    if args.no_send:
        print("\nNo frame transmitted (--no-send).")
        return 0

    sent = 0
    try:
        while args.count == 0 or sent < args.count:
            sendp(frame, iface=args.interface, verbose=False)
            sent += 1
            print(f"Sent CDPv2 frame #{sent}")

            if args.count != 0 and sent >= args.count:
                break
            time.sleep(args.interval)
    except PermissionError:
        print(
            "\nPermission denied while opening the raw Ethernet socket.\n"
            "Run as root or grant the required raw-socket capability.",
            file=sys.stderr,
        )
        return 1
    except KeyboardInterrupt:
        print("\nStopped by user.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
