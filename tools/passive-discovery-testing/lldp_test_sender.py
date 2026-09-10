#!/usr/bin/env python3
"""
lldp_test_sender.py

Generate realistic LLDP / LLDP-MED Ethernet advertisements for isolated
lab testing of passive-discovery implementations.

Requirements:
    python3 -m pip install scapy

Typical use:
    sudo python3 lldp_test_sender.py -i eth0 --show
    sudo python3 lldp_test_sender.py -i eth0 --count 0 --interval 5

Build only and save to PCAP:
    python3 lldp_test_sender.py \
        -i eth0 \
        --pcap lldp_test.pcap \
        --no-send

Example with custom device information:
    sudo python3 lldp_test_sender.py \
        -i enp0s31f6 \
        --system-name "LAB-SWITCH-01" \
        --management-ip 192.0.2.10 \
        --port-vlan-id 10 \
        --named-vlan-id 20 \
        --vlan-name "VOICE" \
        --network-policy-vlan 20 \
        --count 0 \
        --interval 5 \
        --show

Generated LLDPDU includes:

Mandatory LLDP TLVs:
    - Chassis ID
    - Port ID
    - TTL
    - End Of LLDPDU

Standard optional LLDP TLVs:
    - System Name
    - System Description
    - System Capabilities
    - Management IPv4 Address

IEEE 802.1 organizational TLVs:
    - Port VLAN ID
    - VLAN Name

IEEE 802.3 organizational TLVs:
    - Power Via MDI
        * PoE device type
        * Power support/status
        * Power pair
        * Power class

LLDP-MED TLVs:
    - LLDP-MED Capabilities
    - Network Policy
        * Application Type
        * VLAN
        * Tagged/Untagged
        * L2 Priority
        * DSCP
    - Extended Power Via MDI
        * Power Type
        * Power Source
        * Power Priority
        * MED Power Value

Notes:
    LLDP destination MAC:
        01:80:c2:00:00:0e

    LLDP Ethernet EtherType:
        0x88cc

    IEEE 802.1 OUI:
        00:80:c2

    IEEE 802.3 OUI:
        00:12:0f

    LLDP-MED / TIA OUI:
        00:12:bb
"""

import argparse
import struct
import sys
import time

from ipaddress import IPv4Address

try:
    from scapy.all import Ether, Raw, get_if_hwaddr, sendp, wrpcap
except ImportError as exc:
    print(
        "ERROR: Scapy is required.\n"
        "Install it with:\n"
        "    python3 -m pip install scapy\n",
        file=sys.stderr,
    )
    raise SystemExit(2) from exc


# ---------------------------------------------------------------------------
# Ethernet / LLDP constants
# ---------------------------------------------------------------------------

LLDP_DST = "01:80:c2:00:00:0e"
LLDP_ETHERTYPE = 0x88CC


# ---------------------------------------------------------------------------
# LLDP TLV types
# ---------------------------------------------------------------------------

TLV_END = 0
TLV_CHASSIS_ID = 1
TLV_PORT_ID = 2
TLV_TTL = 3
TLV_PORT_DESCRIPTION = 4
TLV_SYSTEM_NAME = 5
TLV_SYSTEM_DESCRIPTION = 6
TLV_SYSTEM_CAPABILITIES = 7
TLV_MANAGEMENT_ADDRESS = 8
TLV_ORG_SPECIFIC = 127


# ---------------------------------------------------------------------------
# Chassis ID subtypes
# ---------------------------------------------------------------------------

CHASSIS_SUBTYPE_MAC = 4


# ---------------------------------------------------------------------------
# Port ID subtypes
# ---------------------------------------------------------------------------

PORT_SUBTYPE_MAC = 3


# ---------------------------------------------------------------------------
# Management Address subtypes
# ---------------------------------------------------------------------------

MGMT_ADDR_SUBTYPE_IPV4 = 1

# Interface numbering subtype:
# 2 = ifIndex
MGMT_IFACE_SUBTYPE_IFINDEX = 2


# ---------------------------------------------------------------------------
# IEEE OUIs
# ---------------------------------------------------------------------------

IEEE_8021_OUI = bytes.fromhex("0080c2")
IEEE_8023_OUI = bytes.fromhex("00120f")
TIA_MED_OUI = bytes.fromhex("0012bb")


# ---------------------------------------------------------------------------
# IEEE 802.1 LLDP organizational subtypes
# ---------------------------------------------------------------------------

IEEE_8021_SUBTYPE_PORT_VLAN_ID = 1
IEEE_8021_SUBTYPE_VLAN_NAME = 3


# ---------------------------------------------------------------------------
# IEEE 802.3 LLDP organizational subtypes
# ---------------------------------------------------------------------------

IEEE_8023_SUBTYPE_POWER_VIA_MDI = 2


# ---------------------------------------------------------------------------
# LLDP-MED organizational subtypes
# ---------------------------------------------------------------------------

LLDP_MED_SUBTYPE_CAPABILITIES = 1
LLDP_MED_SUBTYPE_NETWORK_POLICY = 2
LLDP_MED_SUBTYPE_EXTENDED_POWER = 4


# ---------------------------------------------------------------------------
# LLDP system capability bitmap
# ---------------------------------------------------------------------------

CAPABILITY_BITS = {
    "other": 0x0001,
    "repeater": 0x0002,
    "bridge": 0x0004,
    "wlan": 0x0008,
    "router": 0x0010,
    "telephone": 0x0020,
    "docsis": 0x0040,
    "station": 0x0080,
    "cvlan": 0x0100,
    "svlan": 0x0200,
    "tpmr": 0x0400,
}


# ---------------------------------------------------------------------------
# LLDP-MED application types
# ---------------------------------------------------------------------------

MED_APPLICATION_TYPES = {
    "voice": 1,
    "voice-signaling": 2,
    "guest-voice": 3,
    "guest-voice-signaling": 4,
    "softphone-voice": 5,
    "video-conferencing": 6,
    "streaming-video": 7,
    "video-signaling": 8,
}


# ---------------------------------------------------------------------------
# LLDP-MED Power Type
#
# Two-bit field:
#   00 = PSE
#   01 = PD
# ---------------------------------------------------------------------------

MED_POWER_TYPE = {
    "pse": 0,
    "pd": 1,
}


# ---------------------------------------------------------------------------
# LLDP-MED Power Source
#
# Interpretation depends on Power Type.
#
# PSE:
#   0 = Unknown
#   1 = Primary
#   2 = Backup
#
# PD:
#   0 = Unknown
#   1 = PSE
#   2 = Local
#   3 = PSE + Local
# ---------------------------------------------------------------------------

MED_PSE_POWER_SOURCE = {
    "unknown": 0,
    "primary": 1,
    "backup": 2,
}

MED_PD_POWER_SOURCE = {
    "unknown": 0,
    "pse": 1,
    "local": 2,
    "pse-local": 3,
}


# ---------------------------------------------------------------------------
# LLDP-MED Power Priority
# ---------------------------------------------------------------------------

MED_POWER_PRIORITY = {
    "unknown": 0,
    "critical": 1,
    "high": 2,
    "low": 3,
}


# ---------------------------------------------------------------------------
# IEEE 802.3 PSE power pairs
# ---------------------------------------------------------------------------

POWER_PAIRS = {
    "signal": 1,
    "spare": 2,
}


# ---------------------------------------------------------------------------
# Utility functions
# ---------------------------------------------------------------------------

def parse_mac(mac: str) -> bytes:
    """Convert aa:bb:cc:dd:ee:ff into six bytes."""

    try:
        parts = mac.split(":")

        if len(parts) != 6:
            raise ValueError

        result = bytes(int(part, 16) for part in parts)

        if len(result) != 6:
            raise ValueError

        return result

    except (ValueError, TypeError):
        raise SystemExit(
            f"Invalid MAC address: {mac}"
        )


def build_tlv(tlv_type: int, value: bytes) -> bytes:
    """Build a generic LLDP TLV.

    LLDP TLV header:

        7 bits : Type
        9 bits : Length
    """

    if not 0 <= tlv_type <= 127:
        raise ValueError(
            f"Invalid LLDP TLV type: {tlv_type}"
        )

    if len(value) > 511:
        raise ValueError(
            "LLDP TLV payload exceeds 511 bytes."
        )

    header = (
        (tlv_type << 9)
        | len(value)
    )

    return struct.pack("!H", header) + value


def org_tlv(
    oui: bytes,
    subtype: int,
    data: bytes,
) -> bytes:
    """Build an organizationally-specific LLDP TLV."""

    if len(oui) != 3:
        raise ValueError("OUI must contain exactly 3 bytes.")

    if not 0 <= subtype <= 255:
        raise ValueError("Organizational subtype must be 0-255.")

    value = (
        oui
        + bytes([subtype])
        + data
    )

    return build_tlv(
        TLV_ORG_SPECIFIC,
        value,
    )


# ---------------------------------------------------------------------------
# Standard LLDP TLVs
# ---------------------------------------------------------------------------

def chassis_id_tlv(chassis_mac: str) -> bytes:
    return build_tlv(
        TLV_CHASSIS_ID,
        bytes([CHASSIS_SUBTYPE_MAC])
        + parse_mac(chassis_mac),
    )


def port_id_tlv(port_mac: str) -> bytes:
    return build_tlv(
        TLV_PORT_ID,
        bytes([PORT_SUBTYPE_MAC])
        + parse_mac(port_mac),
    )


def ttl_tlv(ttl: int) -> bytes:
    return build_tlv(
        TLV_TTL,
        struct.pack("!H", ttl),
    )


def system_name_tlv(name: str) -> bytes:
    return build_tlv(
        TLV_SYSTEM_NAME,
        name.encode("utf-8"),
    )


def system_description_tlv(description: str) -> bytes:
    return build_tlv(
        TLV_SYSTEM_DESCRIPTION,
        description.encode("utf-8"),
    )


def system_capabilities_tlv(
    supported: int,
    enabled: int,
) -> bytes:
    return build_tlv(
        TLV_SYSTEM_CAPABILITIES,
        struct.pack(
            "!HH",
            supported,
            enabled,
        ),
    )


def management_ipv4_tlv(
    ip: str,
    ifindex: int,
) -> bytes:
    """Build Management Address TLV for an IPv4 address.

    Value:

        Management Address String Length : 1 byte
        Address Subtype                  : 1 byte
        IPv4 Address                     : 4 bytes
        Interface Numbering Subtype      : 1 byte
        Interface Number                 : 4 bytes
        OID String Length                : 1 byte
    """

    ipv4 = IPv4Address(ip).packed

    management_address = (
        bytes([
            5,  # subtype + IPv4 = 5 bytes
            MGMT_ADDR_SUBTYPE_IPV4,
        ])
        + ipv4
        + bytes([
            MGMT_IFACE_SUBTYPE_IFINDEX
        ])
        + struct.pack("!I", ifindex)
        + bytes([
            0  # No OID supplied
        ])
    )

    return build_tlv(
        TLV_MANAGEMENT_ADDRESS,
        management_address,
    )


# ---------------------------------------------------------------------------
# IEEE 802.1 organizational TLVs
# ---------------------------------------------------------------------------

def port_vlan_id_tlv(vlan_id: int) -> bytes:
    """IEEE 802.1 Port VLAN ID TLV."""

    return org_tlv(
        IEEE_8021_OUI,
        IEEE_8021_SUBTYPE_PORT_VLAN_ID,
        struct.pack("!H", vlan_id),
    )


def vlan_name_tlv(
    vlan_id: int,
    vlan_name: str,
) -> bytes:
    """IEEE 802.1 VLAN Name TLV."""

    name_bytes = vlan_name.encode("utf-8")

    if len(name_bytes) > 32:
        raise SystemExit(
            "--vlan-name must be 32 bytes or fewer."
        )

    data = (
        struct.pack("!H", vlan_id)
        + bytes([len(name_bytes)])
        + name_bytes
    )

    return org_tlv(
        IEEE_8021_OUI,
        IEEE_8021_SUBTYPE_VLAN_NAME,
        data,
    )


# ---------------------------------------------------------------------------
# IEEE 802.3 Power Via MDI
# ---------------------------------------------------------------------------

def power_via_mdi_tlv(
    poe_device: str,
    power_pair: str,
    power_class: int,
) -> bytes:
    """Build legacy IEEE 802.3 Power Via MDI TLV.

    Information fields:

        MDI Power Support
        PSE Power Pair
        Power Class

    For the default PSE advertisement we indicate:

        MDI power supported
        MDI power enabled
        PSE pair control supported
    """

    if poe_device == "pse":
        # Bit 0: port class = PSE
        # Bit 1: MDI power support
        # Bit 2: MDI power enabled
        # Bit 3: PSE pair control ability
        mdi_power_support = 0x0F
    else:
        # PD advertisement.
        #
        # Bit 0 clear -> PD.
        # Advertise MDI power support/status.
        mdi_power_support = 0x06

    pair_value = POWER_PAIRS[power_pair]

    # IEEE representation:
    #
    #   1 = Class 0
    #   2 = Class 1
    #   3 = Class 2
    #   4 = Class 3
    #   5 = Class 4
    encoded_power_class = power_class + 1

    data = bytes([
        mdi_power_support,
        pair_value,
        encoded_power_class,
    ])

    return org_tlv(
        IEEE_8023_OUI,
        IEEE_8023_SUBTYPE_POWER_VIA_MDI,
        data,
    )


# ---------------------------------------------------------------------------
# LLDP-MED Capabilities
# ---------------------------------------------------------------------------

def med_capabilities_tlv(
    poe_device: str,
    med_device_class: int,
) -> bytes:
    """Build LLDP-MED Capabilities TLV.

    Capability bitmap:

        bit 0 - LLDP-MED Capabilities
        bit 1 - Network Policy
        bit 2 - Location Identification
        bit 3 - Extended Power PSE
        bit 4 - Extended Power PD
        bit 5 - Inventory
    """

    # Advertise:
    #   MED capabilities
    #   Network policy
    capabilities = (
        (1 << 0)
        | (1 << 1)
    )

    if poe_device == "pse":
        capabilities |= (1 << 3)
    else:
        capabilities |= (1 << 4)

    data = struct.pack(
        "!HB",
        capabilities,
        med_device_class,
    )

    return org_tlv(
        TIA_MED_OUI,
        LLDP_MED_SUBTYPE_CAPABILITIES,
        data,
    )


# ---------------------------------------------------------------------------
# LLDP-MED Network Policy
# ---------------------------------------------------------------------------

def med_network_policy_tlv(
    application_type: int,
    vlan_id: int,
    tagged: bool,
    l2_priority: int,
    dscp: int,
    unknown: bool = False,
) -> bytes:
    """Build LLDP-MED Network Policy TLV.

    Network Policy is four bytes:

        Application Type : 8 bits

        U                 : 1 bit
        T                 : 1 bit
        Reserved          : 1 bit
        VLAN ID           : 12 bits
        L2 Priority       : 3 bits
        DSCP              : 6 bits

    The final 24 bits are packed into a big-endian 3-byte value.
    """

    policy = 0

    if unknown:
        policy |= (1 << 23)

    if tagged:
        policy |= (1 << 22)

    policy |= (
        (vlan_id & 0x0FFF) << 9
    )

    policy |= (
        (l2_priority & 0x07) << 6
    )

    policy |= (
        dscp & 0x3F
    )

    policy_bytes = policy.to_bytes(
        3,
        byteorder="big",
    )

    data = (
        bytes([application_type])
        + policy_bytes
    )

    return org_tlv(
        TIA_MED_OUI,
        LLDP_MED_SUBTYPE_NETWORK_POLICY,
        data,
    )


# ---------------------------------------------------------------------------
# LLDP-MED Extended Power Via MDI
# ---------------------------------------------------------------------------

def med_extended_power_tlv(
    poe_device: str,
    source_name: str,
    priority_name: str,
    watts: float,
) -> bytes:
    """Build LLDP-MED Extended Power Via MDI TLV.

    First power octet:

        Power Type     : 2 bits
        Power Source   : 2 bits
        Power Priority : 4 bits

    Power value:
        16 bits, expressed in units of 0.1 W.
    """

    power_type = MED_POWER_TYPE[poe_device]

    if poe_device == "pse":
        power_source = MED_PSE_POWER_SOURCE[
            source_name
        ]
    else:
        power_source = MED_PD_POWER_SOURCE[
            source_name
        ]

    power_priority = MED_POWER_PRIORITY[
        priority_name
    ]

    first_octet = (
        ((power_type & 0x03) << 6)
        | ((power_source & 0x03) << 4)
        | (power_priority & 0x0F)
    )

    # LLDP-MED represents power in increments of 0.1 W.
    power_value = round(watts * 10)

    if not 0 <= power_value <= 1023:
        raise SystemExit(
            "--med-power must be between 0 and 102.3 watts."
        )

    data = (
        bytes([first_octet])
        + struct.pack("!H", power_value)
    )

    return org_tlv(
        TIA_MED_OUI,
        LLDP_MED_SUBTYPE_EXTENDED_POWER,
        data,
    )


def end_tlv() -> bytes:
    return build_tlv(
        TLV_END,
        b"",
    )


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=(
            "Send realistic LLDP/LLDP-MED advertisements "
            "for an isolated Ethernet lab."
        )
    )

    p.add_argument(
        "-i",
        "--interface",
        required=True,
        help="Linux interface used to transmit.",
    )

    p.add_argument(
        "--src-mac",
        help=(
            "Ethernet Source MAC. "
            "Defaults to the selected interface MAC."
        ),
    )

    p.add_argument(
        "--dst-mac",
        default=LLDP_DST,
        help=(
            "LLDP destination MAC. "
            f"Default: {LLDP_DST}"
        ),
    )

    # ------------------------------------------------------------------
    # Standard LLDP fields
    # ------------------------------------------------------------------

    p.add_argument(
        "--system-name",
        default="LAB-SWITCH-01",
        help="LLDP System Name.",
    )

    p.add_argument(
        "--system-description",
        default=(
            "Lab LLDP/LLDP-MED Ethernet Device; "
            "Firmware 1.0; Passive Discovery Test"
        ),
        help="LLDP System Description.",
    )

    p.add_argument(
        "--management-ip",
        default="192.0.2.10",
        help="LLDP Management IPv4 address.",
    )

    p.add_argument(
        "--chassis-mac",
        help=(
            "MAC used by Chassis ID TLV. "
            "Defaults to the Ethernet source MAC."
        ),
    )

    p.add_argument(
        "--port-mac",
        help=(
            "MAC used by Port ID TLV. "
            "Defaults to the Ethernet source MAC."
        ),
    )

    p.add_argument(
        "--ttl",
        type=int,
        default=120,
        help="LLDP TTL in seconds. Default: 120.",
    )

    p.add_argument(
        "--ifindex",
        type=int,
        default=1,
        help=(
            "Interface index advertised by the "
            "Management Address TLV."
        ),
    )

    p.add_argument(
        "--capabilities",
        default="bridge,router",
        help=(
            "Comma-separated LLDP capabilities. "
            "Examples: bridge,router,telephone,wlan,station"
        ),
    )

    # ------------------------------------------------------------------
    # IEEE 802.1 VLAN fields
    # ------------------------------------------------------------------

    p.add_argument(
        "--port-vlan-id",
        type=int,
        default=1,
        help="IEEE 802.1 Port VLAN ID.",
    )

    p.add_argument(
        "--named-vlan-id",
        type=int,
        default=100,
        help="VLAN ID carried by the VLAN Name TLV.",
    )

    p.add_argument(
        "--vlan-name",
        default="LAB-VLAN",
        help="IEEE 802.1 VLAN Name.",
    )

    # ------------------------------------------------------------------
    # LLDP-MED Network Policy
    # ------------------------------------------------------------------

    p.add_argument(
        "--network-policy-vlan",
        type=int,
        default=100,
        help="VLAN ID advertised by LLDP-MED Network Policy.",
    )

    p.add_argument(
        "--network-policy-app",
        choices=tuple(MED_APPLICATION_TYPES),
        default="voice",
        help="LLDP-MED Network Policy application type.",
    )

    p.add_argument(
        "--network-policy-tagged",
        action=argparse.BooleanOptionalAction,
        default=True,
        help=(
            "Advertise the Network Policy VLAN as tagged. "
            "Default: tagged."
        ),
    )

    p.add_argument(
        "--l2-priority",
        type=int,
        default=5,
        help=(
            "LLDP-MED Network Policy Layer-2 priority "
            "(0-7). Default: 5."
        ),
    )

    p.add_argument(
        "--dscp",
        type=int,
        default=46,
        help=(
            "LLDP-MED Network Policy DSCP "
            "(0-63). Default: 46."
        ),
    )

    # ------------------------------------------------------------------
    # IEEE 802.3 PoE
    # ------------------------------------------------------------------

    p.add_argument(
        "--poe-device",
        choices=("pse", "pd"),
        default="pse",
        help=(
            "Advertised PoE device role. "
            "pse = Power Sourcing Equipment; "
            "pd = Powered Device."
        ),
    )

    p.add_argument(
        "--power-pair",
        choices=("signal", "spare"),
        default="signal",
        help="IEEE 802.3 PSE power pair.",
    )

    p.add_argument(
        "--power-class",
        type=int,
        default=3,
        help="IEEE 802.3 PoE class (0-4).",
    )

    # ------------------------------------------------------------------
    # LLDP-MED Power
    # ------------------------------------------------------------------

    p.add_argument(
        "--med-power",
        type=float,
        default=15.4,
        help=(
            "LLDP-MED Extended Power value in watts "
            "(0-102.3). Default: 15.4."
        ),
    )

    p.add_argument(
        "--med-power-source",
        default=None,
        help=(
            "LLDP-MED power source. "
            "For PSE: unknown, primary, backup. "
            "For PD: unknown, pse, local, pse-local. "
            "Default: primary for PSE, pse for PD."
        ),
    )

    p.add_argument(
        "--med-power-priority",
        choices=tuple(MED_POWER_PRIORITY),
        default="high",
        help=(
            "LLDP-MED power priority: "
            "unknown, critical, high, low."
        ),
    )

    p.add_argument(
        "--med-device-class",
        type=int,
        default=4,
        help=(
            "LLDP-MED device class. "
            "Default: 4 (network connectivity device)."
        ),
    )

    # ------------------------------------------------------------------
    # Transmission
    # ------------------------------------------------------------------

    p.add_argument(
        "--count",
        type=int,
        default=1,
        help=(
            "Number of LLDP frames to send. "
            "0 means continuously until Ctrl-C."
        ),
    )

    p.add_argument(
        "--interval",
        type=float,
        default=5.0,
        help=(
            "Seconds between LLDP advertisements. "
            "Default: 5."
        ),
    )

    p.add_argument(
        "--pcap",
        help="Also write the generated frame to this PCAP file.",
    )

    p.add_argument(
        "--no-send",
        action="store_true",
        help=(
            "Build/inspect/write the frame but "
            "do not transmit it."
        ),
    )

    p.add_argument(
        "--show",
        action="store_true",
        help="Display the generated Ethernet/Raw frame.",
    )

    return p.parse_args()


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

def validate(args: argparse.Namespace) -> None:
    try:
        IPv4Address(args.management_ip)
    except ValueError as exc:
        raise SystemExit(
            f"Invalid Management IPv4 address: "
            f"{args.management_ip}"
        ) from exc

    parse_mac(
        args.src_mac
        or get_if_hwaddr(args.interface)
    )

    if args.chassis_mac:
        parse_mac(args.chassis_mac)

    if args.port_mac:
        parse_mac(args.port_mac)

    if not 0 <= args.ttl <= 65535:
        raise SystemExit(
            "--ttl must be between 0 and 65535."
        )

    if not 1 <= args.port_vlan_id <= 4094:
        raise SystemExit(
            "--port-vlan-id must be between 1 and 4094."
        )

    if not 1 <= args.named_vlan_id <= 4094:
        raise SystemExit(
            "--named-vlan-id must be between 1 and 4094."
        )

    if not 0 <= args.network_policy_vlan <= 4094:
        raise SystemExit(
            "--network-policy-vlan must be between 0 and 4094."
        )

    if not 0 <= args.l2_priority <= 7:
        raise SystemExit(
            "--l2-priority must be between 0 and 7."
        )

    if not 0 <= args.dscp <= 63:
        raise SystemExit(
            "--dscp must be between 0 and 63."
        )

    if not 0 <= args.power_class <= 4:
        raise SystemExit(
            "--power-class must be between 0 and 4."
        )

    if not 0.0 <= args.med_power <= 102.3:
        raise SystemExit(
            "--med-power must be between 0 and 102.3 watts."
        )

    if not 1 <= args.med_device_class <= 4:
        raise SystemExit(
            "--med-device-class must be between 1 and 4."
        )

    if args.count < 0:
        raise SystemExit(
            "--count must be >= 0."
        )

    if args.interval < 0:
        raise SystemExit(
            "--interval must be >= 0."
        )

    capabilities = [
        cap.strip().lower()
        for cap in args.capabilities.split(",")
        if cap.strip()
    ]

    for cap in capabilities:
        if cap not in CAPABILITY_BITS:
            raise SystemExit(
                f"Unknown LLDP capability: {cap}\n"
                "Available capabilities: "
                + ", ".join(CAPABILITY_BITS)
            )

    if args.med_power_source is None:
        if args.poe_device == "pse":
            args.med_power_source = "primary"
        else:
            args.med_power_source = "pse"

    if args.poe_device == "pse":
        if args.med_power_source not in MED_PSE_POWER_SOURCE:
            raise SystemExit(
                "For --poe-device pse, --med-power-source "
                "must be one of: "
                + ", ".join(MED_PSE_POWER_SOURCE)
            )
    else:
        if args.med_power_source not in MED_PD_POWER_SOURCE:
            raise SystemExit(
                "For --poe-device pd, --med-power-source "
                "must be one of: "
                + ", ".join(MED_PD_POWER_SOURCE)
            )


# ---------------------------------------------------------------------------
# Capabilities
# ---------------------------------------------------------------------------

def parse_capabilities(value: str) -> int:
    bitmap = 0

    names = [
        name.strip().lower()
        for name in value.split(",")
        if name.strip()
    ]

    for name in names:
        bitmap |= CAPABILITY_BITS[name]

    return bitmap


# ---------------------------------------------------------------------------
# Frame construction
# ---------------------------------------------------------------------------

def build_lldp_frame(
    args: argparse.Namespace,
):
    source_mac = (
        args.src_mac
        or get_if_hwaddr(args.interface)
    )

    chassis_mac = (
        args.chassis_mac
        or source_mac
    )

    port_mac = (
        args.port_mac
        or source_mac
    )

    capabilities = parse_capabilities(
        args.capabilities
    )

    med_application = MED_APPLICATION_TYPES[
        args.network_policy_app
    ]

    tlvs = [
        # ==============================================================
        # Mandatory LLDP TLVs
        # ==============================================================

        chassis_id_tlv(
            chassis_mac
        ),

        port_id_tlv(
            port_mac
        ),

        ttl_tlv(
            args.ttl
        ),

        # ==============================================================
        # Standard LLDP optional TLVs
        # ==============================================================

        system_name_tlv(
            args.system_name
        ),

        system_description_tlv(
            args.system_description
        ),

        system_capabilities_tlv(
            capabilities,
            capabilities,
        ),

        management_ipv4_tlv(
            args.management_ip,
            args.ifindex,
        ),

        # ==============================================================
        # IEEE 802.1 VLAN TLVs
        # ==============================================================

        port_vlan_id_tlv(
            args.port_vlan_id
        ),

        vlan_name_tlv(
            args.named_vlan_id,
            args.vlan_name,
        ),

        # ==============================================================
        # IEEE 802.3 PoE TLV
        # ==============================================================

        power_via_mdi_tlv(
            args.poe_device,
            args.power_pair,
            args.power_class,
        ),

        # ==============================================================
        # LLDP-MED Capabilities
        # ==============================================================

        med_capabilities_tlv(
            args.poe_device,
            args.med_device_class,
        ),

        # ==============================================================
        # LLDP-MED Network Policy
        # ==============================================================

        med_network_policy_tlv(
            application_type=med_application,
            vlan_id=args.network_policy_vlan,
            tagged=args.network_policy_tagged,
            l2_priority=args.l2_priority,
            dscp=args.dscp,
        ),

        # ==============================================================
        # LLDP-MED Extended Power Via MDI
        # ==============================================================

        med_extended_power_tlv(
            poe_device=args.poe_device,
            source_name=args.med_power_source,
            priority_name=args.med_power_priority,
            watts=args.med_power,
        ),

        # ==============================================================
        # End Of LLDPDU
        # ==============================================================

        end_tlv(),
    ]

    lldpdu = b"".join(tlvs)

    frame = (
        Ether(
            dst=args.dst_mac,
            src=source_mac,
            type=LLDP_ETHERTYPE,
        )
        / Raw(lldpdu)
    )

    # Force complete serialization.
    bytes(frame)

    return (
        frame,
        source_mac,
        chassis_mac,
        port_mac,
        capabilities,
    )


# ---------------------------------------------------------------------------
# Information display
# ---------------------------------------------------------------------------

def print_configuration(
    args: argparse.Namespace,
    frame,
    source_mac: str,
    chassis_mac: str,
    port_mac: str,
) -> None:

    print()
    print("=" * 72)
    print("LLDP / LLDP-MED TEST ADVERTISEMENT")
    print("=" * 72)

    print()
    print("[ Ethernet ]")
    print(f"Interface              : {args.interface}")
    print(f"Destination MAC        : {args.dst_mac}")
    print(f"SOURCE MAC             : {source_mac}")
    print(f"EtherType              : 0x{LLDP_ETHERTYPE:04x}")

    print()
    print("[ Mandatory LLDP ]")
    print(f"CHASSIS ID             : {chassis_mac}")
    print(f"PORT MAC / Port ID     : {port_mac}")
    print(f"TTL                    : {args.ttl} seconds")

    print()
    print("[ System Information ]")
    print(f"SYSTEM NAME            : {args.system_name}")
    print(f"SYSTEM DESCRIPTION     : {args.system_description}")
    print(f"CAPABILITIES           : {args.capabilities}")
    print(f"MANAGEMENT IPV4        : {args.management_ip}")
    print(f"Management IfIndex     : {args.ifindex}")

    print()
    print("[ IEEE 802.1 VLAN ]")
    print(f"PORT VLAN ID           : {args.port_vlan_id}")
    print(f"NAMED VLAN ID          : {args.named_vlan_id}")
    print(f"VLAN NAME              : {args.vlan_name}")

    print()
    print("[ LLDP-MED Network Policy ]")
    print(
        f"Application Type       : "
        f"{args.network_policy_app}"
    )
    print(
        f"NETWORK POLICY VLAN    : "
        f"{args.network_policy_vlan}"
    )
    print(
        f"Tagged                 : "
        f"{'yes' if args.network_policy_tagged else 'no'}"
    )
    print(f"L2 Priority            : {args.l2_priority}")
    print(f"DSCP                   : {args.dscp}")

    print()
    print("[ IEEE 802.3 Power Via MDI ]")
    print(
        f"POE / DEVICE           : "
        f"{args.poe_device.upper()}"
    )
    print(
        f"POWER PAIR             : "
        f"{args.power_pair}"
    )
    print(
        f"POWER CLASS            : "
        f"Class {args.power_class}"
    )

    print()
    print("[ LLDP-MED Extended Power ]")
    print(
        f"MED POWER              : "
        f"{args.med_power:.1f} W"
    )
    print(
        f"TYPE                   : "
        f"{args.poe_device.upper()}"
    )
    print(
        f"SOURCE                 : "
        f"{args.med_power_source}"
    )
    print(
        f"PRIORITY               : "
        f"{args.med_power_priority}"
    )

    print()
    print("[ Frame ]")
    print(
        f"Frame length           : "
        f"{len(bytes(frame))} bytes "
        f"(without Ethernet FCS)"
    )

    if args.count == 0:
        print("Transmission count     : unlimited")
    else:
        print(f"Transmission count     : {args.count}")

    print(
        f"Transmission interval  : "
        f"{args.interval} seconds"
    )

    print("=" * 72)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    args = parse_args()

    validate(args)

    (
        frame,
        source_mac,
        chassis_mac,
        port_mac,
        capabilities,
    ) = build_lldp_frame(args)

    print_configuration(
        args,
        frame,
        source_mac,
        chassis_mac,
        port_mac,
    )

    if args.show:
        print()
        print("Scapy Ethernet frame:")
        print("---------------------")
        frame.show2()

        print()
        print("Raw LLDPDU:")
        print("-----------")
        print(bytes(frame.payload).hex(" "))

    if args.pcap:
        wrpcap(
            args.pcap,
            [frame],
        )

        print()
        print(
            f"PCAP written to: "
            f"{args.pcap}"
        )

    if args.no_send:
        print()
        print(
            "No frame transmitted (--no-send)."
        )
        return 0

    print()
    print(
        "Starting LLDP transmission. "
        "Press Ctrl+C to stop."
    )

    sent = 0

    try:
        while (
            args.count == 0
            or sent < args.count
        ):
            sendp(
                frame,
                iface=args.interface,
                verbose=False,
            )

            sent += 1

            print(
                f"[TX #{sent}] "
                f"LLDP "
                f"src={source_mac} "
                f"dst={args.dst_mac} "
                f"system={args.system_name!r} "
                f"mgmt={args.management_ip} "
                f"vlan={args.port_vlan_id} "
                f"med_vlan={args.network_policy_vlan}"
            )

            if (
                args.count != 0
                and sent >= args.count
            ):
                break

            time.sleep(
                args.interval
            )

    except PermissionError:
        print(
            "\nPermission denied while opening "
            "the raw Ethernet socket.\n"
            "Run as root or grant the required "
            "raw-socket capability.",
            file=sys.stderr,
        )
        return 1

    except KeyboardInterrupt:
        print()
        print(
            f"Stopped by user after "
            f"{sent} LLDP frame(s)."
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
