#!/usr/bin/env python3
"""Generate deterministic EAPOL/EAP Ethernet frames for isolated lab testing.

Requires Scapy (``python3 -m pip install scapy``) and, when transmitting,
Linux privileges sufficient for raw Ethernet sends.

Examples:

    # Send EAP-Response/Identity forever, every 5 seconds:
    sudo python3 eapol_test_sender.py response-identity \
        -i enp0s31f6 \
        --count 0 \
        --interval 5 \
        --identity "test-user"

    # Send exactly 10 frames:
    sudo python3 eapol_test_sender.py response-identity \
        -i enp0s31f6 \
        --count 10 \
        --interval 5
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

# IEEE 802.1X / EAPOL packet types
EAPOL_PACKET_EAP = 0
EAPOL_PACKET_START = 1
EAPOL_PACKET_LOGOFF = 2

# RFC 3748 EAP codes
EAP_REQUEST = 1
EAP_RESPONSE = 2
EAP_SUCCESS = 3
EAP_FAILURE = 4

# RFC 3748 EAP Type
EAP_TYPE_IDENTITY = 1

FRAME_NAMES = (
    "start",
    "logoff",
    "request-identity",
    "response-identity",
    "success",
    "failure",
)


EAPOL_PACKET_TYPE_NAMES = {
    EAPOL_PACKET_EAP: "EAP Packet",
    EAPOL_PACKET_START: "Start",
    EAPOL_PACKET_LOGOFF: "Logoff",
}

EAP_CODE_NAMES = {
    EAP_REQUEST: "Request",
    EAP_RESPONSE: "Response",
    EAP_SUCCESS: "Success",
    EAP_FAILURE: "Failure",
}

EAP_TYPE_NAMES = {
    EAP_TYPE_IDENTITY: "Identity",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate and repeatedly send EAPOL/EAP frames "
            "for isolated Passive Discovery testing."
        )
    )

    parser.add_argument(
        "-i",
        "--interface",
        required=True,
        help="Linux transmit interface.",
    )

    parser.add_argument(
        "frame",
        choices=FRAME_NAMES + ("all", "identity-preserve"),
        help="Frame or deterministic sequence to generate.",
    )

    parser.add_argument(
        "--src-mac",
        help="Source MAC; defaults to the interface MAC.",
    )

    parser.add_argument(
        "--dst-mac",
        default=EAPOL_DST,
        help=f"Destination MAC. Default: {EAPOL_DST}",
    )

    parser.add_argument(
        "--identity",
        default="test-user",
        help="EAP Response/Identity value.",
    )

    parser.add_argument(
        "--identifier",
        type=int,
        default=1,
        help="EAP identifier (0-255).",
    )

    parser.add_argument(
        "--version",
        type=int,
        default=2,
        help="EAPOL version (1-3).",
    )

    parser.add_argument(
        "--interval",
        type=float,
        default=5.0,
        help="Seconds between transmissions. Default: 5.",
    )

    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help=(
            "Number of transmission cycles. "
            "Use 0 to transmit continuously until Ctrl+C. "
            "Default: 1."
        ),
    )

    parser.add_argument(
        "--pcap",
        help="Also write generated frames to this PCAP file.",
    )

    parser.add_argument(
        "--no-send",
        action="store_true",
        help="Build only; do not transmit.",
    )

    parser.add_argument(
        "--show",
        action="store_true",
        help="Display decoded Scapy frames.",
    )

    return parser.parse_args()


def validate(args: argparse.Namespace) -> None:
    if not 0 <= args.identifier <= 255:
        raise SystemExit("--identifier must be between 0 and 255.")

    if not 1 <= args.version <= 3:
        raise SystemExit("--version must be between 1 and 3.")

    if args.interval < 0:
        raise SystemExit("--interval must be >= 0.")

    if args.count < 0:
        raise SystemExit("--count must be >= 0.")

    try:
        args.identity.encode("ascii")
    except UnicodeEncodeError as exc:
        raise SystemExit(
            "--identity must contain ASCII characters only."
        ) from exc


def eap_packet(
    code: int,
    identifier: int,
    data: bytes = b"",
) -> bytes:
    """Build an EAP packet.

    EAP header:
        Code        1 byte
        Identifier  1 byte
        Length      2 bytes
        Data        variable
    """

    length = 4 + len(data)

    return struct.pack(
        "!BBH",
        code,
        identifier,
        length,
    ) + data


def eapol_payload(
    frame_name: str,
    version: int,
    identifier: int,
    identity: str,
) -> bytes:
    """Build the EAPOL payload."""

    if frame_name == "start":
        packet_type = EAPOL_PACKET_START
        body = b""

    elif frame_name == "logoff":
        packet_type = EAPOL_PACKET_LOGOFF
        body = b""

    elif frame_name == "request-identity":
        packet_type = EAPOL_PACKET_EAP

        body = eap_packet(
            EAP_REQUEST,
            identifier,
            bytes([EAP_TYPE_IDENTITY]),
        )

    elif frame_name == "response-identity":
        packet_type = EAPOL_PACKET_EAP

        identity_bytes = identity.encode("ascii")

        body = eap_packet(
            EAP_RESPONSE,
            identifier,
            bytes([EAP_TYPE_IDENTITY]) + identity_bytes,
        )

    elif frame_name == "success":
        packet_type = EAPOL_PACKET_EAP

        body = eap_packet(
            EAP_SUCCESS,
            identifier,
        )

    elif frame_name == "failure":
        packet_type = EAPOL_PACKET_EAP

        body = eap_packet(
            EAP_FAILURE,
            identifier,
        )

    else:
        raise ValueError(
            f"Unsupported frame name: {frame_name}"
        )

    # EAPOL header:
    #
    # Version      1 byte
    # Packet Type  1 byte
    # Body Length  2 bytes

    return struct.pack(
        "!BBH",
        version,
        packet_type,
        len(body),
    ) + body


def selected_frames(frame_name: str) -> list[str]:
    if frame_name == "all":
        return list(FRAME_NAMES)

    if frame_name == "identity-preserve":
        return [
            "response-identity",
            "start",
        ]

    return [frame_name]


def frame_metadata(
    frame_name: str,
    source: str,
    version: int,
    identifier: int,
    identity: str,
) -> dict:
    """Return the protocol fields represented by a generated frame."""

    metadata = {
        "source_mac": source,
        "eapol_version": version,
        "packet_type": None,
        "packet_type_name": None,
        "eap_code": None,
        "eap_code_name": None,
        "eap_type": None,
        "eap_type_name": None,
        "identity": None,
        "identifier": None,
    }

    if frame_name == "start":
        metadata["packet_type"] = EAPOL_PACKET_START
        metadata["packet_type_name"] = EAPOL_PACKET_TYPE_NAMES[
            EAPOL_PACKET_START
        ]

    elif frame_name == "logoff":
        metadata["packet_type"] = EAPOL_PACKET_LOGOFF
        metadata["packet_type_name"] = EAPOL_PACKET_TYPE_NAMES[
            EAPOL_PACKET_LOGOFF
        ]

    elif frame_name == "request-identity":
        metadata["packet_type"] = EAPOL_PACKET_EAP
        metadata["packet_type_name"] = EAPOL_PACKET_TYPE_NAMES[
            EAPOL_PACKET_EAP
        ]
        metadata["eap_code"] = EAP_REQUEST
        metadata["eap_code_name"] = EAP_CODE_NAMES[EAP_REQUEST]
        metadata["eap_type"] = EAP_TYPE_IDENTITY
        metadata["eap_type_name"] = EAP_TYPE_NAMES[EAP_TYPE_IDENTITY]
        metadata["identifier"] = identifier

    elif frame_name == "response-identity":
        metadata["packet_type"] = EAPOL_PACKET_EAP
        metadata["packet_type_name"] = EAPOL_PACKET_TYPE_NAMES[
            EAPOL_PACKET_EAP
        ]
        metadata["eap_code"] = EAP_RESPONSE
        metadata["eap_code_name"] = EAP_CODE_NAMES[EAP_RESPONSE]
        metadata["eap_type"] = EAP_TYPE_IDENTITY
        metadata["eap_type_name"] = EAP_TYPE_NAMES[EAP_TYPE_IDENTITY]
        metadata["identity"] = identity
        metadata["identifier"] = identifier

    elif frame_name == "success":
        metadata["packet_type"] = EAPOL_PACKET_EAP
        metadata["packet_type_name"] = EAPOL_PACKET_TYPE_NAMES[
            EAPOL_PACKET_EAP
        ]
        metadata["eap_code"] = EAP_SUCCESS
        metadata["eap_code_name"] = EAP_CODE_NAMES[EAP_SUCCESS]
        metadata["identifier"] = identifier

    elif frame_name == "failure":
        metadata["packet_type"] = EAPOL_PACKET_EAP
        metadata["packet_type_name"] = EAPOL_PACKET_TYPE_NAMES[
            EAPOL_PACKET_EAP
        ]
        metadata["eap_code"] = EAP_FAILURE
        metadata["eap_code_name"] = EAP_CODE_NAMES[EAP_FAILURE]
        metadata["identifier"] = identifier

    return metadata


def print_metadata(
    frame_name: str,
    metadata: dict,
) -> None:
    """Print important Ethernet/EAPOL/EAP fields."""

    print()
    print("=" * 64)
    print(f"FRAME              : {frame_name}")
    print(f"SOURCE MAC         : {metadata['source_mac']}")
    print(f"EAPOL VERSION      : {metadata['eapol_version']}")

    if metadata["packet_type"] is not None:
        print(
            "PACKET TYPE        : "
            f"{metadata['packet_type']} "
            f"({metadata['packet_type_name']})"
        )
    else:
        print("PACKET TYPE        : N/A")

    if metadata["eap_code"] is not None:
        print(
            "EAP CODE           : "
            f"{metadata['eap_code']} "
            f"({metadata['eap_code_name']})"
        )
    else:
        print("EAP CODE           : N/A")

    if metadata["eap_type"] is not None:
        print(
            "EAP TYPE           : "
            f"{metadata['eap_type']} "
            f"({metadata['eap_type_name']})"
        )
    else:
        print("EAP TYPE           : N/A")

    if metadata["identity"] is not None:
        print(
            f"IDENTITY           : {metadata['identity']}"
        )
    else:
        print("IDENTITY           : N/A")

    if metadata["identifier"] is not None:
        print(
            f"EAP IDENTIFIER     : {metadata['identifier']}"
        )

    print("=" * 64)


def verify_identity_frame(metadata: dict) -> None:
    """Verify that an EAP Response/Identity has all requested fields."""

    required_fields = {
        "SOURCE MAC": metadata["source_mac"],
        "PACKET TYPE": metadata["packet_type"],
        "EAPOL VERSION": metadata["eapol_version"],
        "EAP CODE": metadata["eap_code"],
        "EAP TYPE": metadata["eap_type"],
        "IDENTITY": metadata["identity"],
    }

    missing = [
        name
        for name, value in required_fields.items()
        if value is None
    ]

    if missing:
        raise RuntimeError(
            "Generated EAP Response/Identity is missing: "
            + ", ".join(missing)
        )


def build_frames(args, source):
    """Build all frames requested on the command line."""

    frames = []

    for name in selected_frames(args.frame):
        payload = eapol_payload(
            name,
            args.version,
            args.identifier,
            args.identity,
        )

        frame = (
            Ether(
                dst=args.dst_mac,
                src=source,
                type=EAPOL_ETHERTYPE,
            )
            / Raw(payload)
        )

        metadata = frame_metadata(
            name,
            source,
            args.version,
            args.identifier,
            args.identity,
        )

        # Response/Identity is the packet that contains all six
        # requested fields.
        if name == "response-identity":
            verify_identity_frame(metadata)

        frames.append(
            (name, frame, metadata)
        )

    return frames


def main() -> int:
    args = parse_args()
    validate(args)

    source = (
        args.src_mac
        or get_if_hwaddr(args.interface)
    )

    generated_frames = build_frames(
        args,
        source,
    )

    print()
    print("EAPOL sender configuration")
    print("--------------------------")
    print(f"Interface       : {args.interface}")
    print(f"Source MAC      : {source}")
    print(f"Destination MAC : {args.dst_mac}")
    print(f"Interval        : {args.interval} seconds")

    if args.count == 0:
        print("Count           : unlimited")
    else:
        print(f"Count           : {args.count}")

    for name, frame, metadata in generated_frames:
        print_metadata(
            name,
            metadata,
        )

        print(
            f"Ethernet frame length: "
            f"{len(bytes(frame))} bytes"
        )

        if args.show:
            print()
            frame.show2()

    if args.pcap:
        wrpcap(
            args.pcap,
            [frame for _, frame, _ in generated_frames],
        )

        print()
        print(
            f"Generated frame(s) written to: "
            f"{args.pcap}"
        )

    if args.no_send:
        print()
        print("--no-send specified; nothing transmitted.")
        return 0

    print()
    print(
        "Starting transmission. "
        "Press Ctrl+C to stop."
    )

    transmission_number = 0

    try:
        while True:
            # count=0 means unlimited transmission.
            if (
                args.count != 0
                and transmission_number >= args.count
            ):
                break

            transmission_number += 1

            for name, frame, _ in generated_frames:
                print(
                    f"[TX #{transmission_number}] "
                    f"{name:18s} "
                    f"src={source} "
                    f"dst={args.dst_mac} "
                    f"version={args.version} "
                    f"len={len(bytes(frame))}"
                )

                sendp(
                    frame,
                    iface=args.interface,
                    verbose=False,
                )

            # Do not unnecessarily sleep after the final frame.
            if (
                args.count != 0
                and transmission_number >= args.count
            ):
                break

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print()
        print(
            f"Transmission stopped by user "
            f"after {transmission_number} cycle(s)."
        )
        return 0

    print()
    print(
        f"Transmission complete: "
        f"{transmission_number} cycle(s)."
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
