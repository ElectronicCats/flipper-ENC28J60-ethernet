#!/usr/bin/env python3
"""Generate deterministic EAPOL/EAP Ethernet frames for isolated lab testing.

Requires Scapy (``python3 -m pip install scapy``) and, when transmitting,
Linux privileges sufficient for raw Ethernet sends.
"""

import argparse
import struct
import sys
import time

try:
    from scapy.all import Ether, Raw, get_if_hwaddr, sendp, wrpcap
except ImportError as exc:
    print(
        "ERROR: Scapy is required. Install it with: python3 -m pip install scapy",
        file=sys.stderr,
    )
    raise SystemExit(2) from exc


EAPOL_DST = "01:80:c2:00:00:03"
EAPOL_ETHERTYPE = 0x888E

EAPOL_PACKET_EAP = 0
EAPOL_PACKET_START = 1
EAPOL_PACKET_LOGOFF = 2

EAP_REQUEST = 1
EAP_RESPONSE = 2
EAP_SUCCESS = 3
EAP_FAILURE = 4
EAP_TYPE_IDENTITY = 1

FRAME_NAMES = (
    "start",
    "logoff",
    "request-identity",
    "response-identity",
    "success",
    "failure",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send bounded EAPOL/EAP frames for Passive Discovery testing."
    )
    parser.add_argument("-i", "--interface", required=True, help="Linux transmit interface.")
    parser.add_argument(
        "frame",
        choices=FRAME_NAMES + ("all", "identity-preserve"),
        help="Frame or deterministic sequence to generate.",
    )
    parser.add_argument("--src-mac", help="Source MAC; defaults to the interface MAC.")
    parser.add_argument("--dst-mac", default=EAPOL_DST, help="Destination MAC.")
    parser.add_argument("--identity", default="test-user", help="Response/Identity value.")
    parser.add_argument("--identifier", type=int, default=1, help="EAP identifier (0-255).")
    parser.add_argument("--version", type=int, default=2, help="EAPOL version (1-3).")
    parser.add_argument("--interval", type=float, default=1.0, help="Seconds between frames.")
    parser.add_argument("--pcap", help="Also write generated frames to this PCAP file.")
    parser.add_argument("--no-send", action="store_true", help="Build only; do not transmit.")
    parser.add_argument("--show", action="store_true", help="Display decoded frames.")
    return parser.parse_args()


def validate(args: argparse.Namespace) -> None:
    if not 0 <= args.identifier <= 255:
        raise SystemExit("--identifier must be between 0 and 255.")
    if not 1 <= args.version <= 3:
        raise SystemExit("--version must be between 1 and 3.")
    if args.interval < 0:
        raise SystemExit("--interval must be >= 0.")
    try:
        args.identity.encode("ascii")
    except UnicodeEncodeError as exc:
        raise SystemExit("--identity must contain ASCII characters only.") from exc


def eap_packet(code: int, identifier: int, data: bytes = b"") -> bytes:
    return struct.pack("!BBH", code, identifier, 4 + len(data)) + data


def eapol_payload(frame_name: str, version: int, identifier: int, identity: str) -> bytes:
    if frame_name == "start":
        packet_type, body = EAPOL_PACKET_START, b""
    elif frame_name == "logoff":
        packet_type, body = EAPOL_PACKET_LOGOFF, b""
    elif frame_name == "request-identity":
        packet_type = EAPOL_PACKET_EAP
        body = eap_packet(EAP_REQUEST, identifier, bytes([EAP_TYPE_IDENTITY]))
    elif frame_name == "response-identity":
        packet_type = EAPOL_PACKET_EAP
        body = eap_packet(
            EAP_RESPONSE,
            identifier,
            bytes([EAP_TYPE_IDENTITY]) + identity.encode("ascii"),
        )
    elif frame_name == "success":
        packet_type, body = EAPOL_PACKET_EAP, eap_packet(EAP_SUCCESS, identifier)
    elif frame_name == "failure":
        packet_type, body = EAPOL_PACKET_EAP, eap_packet(EAP_FAILURE, identifier)
    else:
        raise ValueError(f"Unsupported frame name: {frame_name}")

    return struct.pack("!BBH", version, packet_type, len(body)) + body


def selected_frames(frame_name: str) -> list[str]:
    if frame_name == "all":
        return list(FRAME_NAMES)
    if frame_name == "identity-preserve":
        return ["response-identity", "start"]
    return [frame_name]


def main() -> int:
    args = parse_args()
    validate(args)

    source = args.src_mac or get_if_hwaddr(args.interface)
    names = selected_frames(args.frame)
    frames = []

    for name in names:
        payload = eapol_payload(name, args.version, args.identifier, args.identity)
        frame = Ether(dst=args.dst_mac, src=source, type=EAPOL_ETHERTYPE) / Raw(payload)
        frames.append(frame)
        print(
            f"{name:18s} src={source} dst={args.dst_mac} "
            f"version={args.version} frame_len={len(bytes(frame))}"
        )
        if args.show:
            frame.show2()

    if args.pcap:
        wrpcap(args.pcap, frames)

    if not args.no_send:
        for index, frame in enumerate(frames):
            sendp(frame, iface=args.interface, verbose=False)
            if index + 1 < len(frames):
                time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
