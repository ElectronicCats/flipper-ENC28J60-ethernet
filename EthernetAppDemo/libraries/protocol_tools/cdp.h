#pragma once

#include "neighbor_db.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CDP_SNAP_DSAP    0xAA
#define CDP_SNAP_SSAP    0xAA
#define CDP_SNAP_CONTROL 0x03

#define CDP_CISCO_OUI_0 0x00
#define CDP_CISCO_OUI_1 0x00
#define CDP_CISCO_OUI_2 0x0C
#define CDP_SNAP_PID    0x2000

typedef enum {
    CDP_TLV_DEVICE_ID = 0x0001,
    CDP_TLV_ADDRESS = 0x0002,
    CDP_TLV_PORT_ID = 0x0003,
    CDP_TLV_CAPABILITIES = 0x0004,
    CDP_TLV_SOFTWARE_VERSION = 0x0005,
    CDP_TLV_PLATFORM = 0x0006,
    CDP_TLV_NATIVE_VLAN = 0x000A,
    CDP_TLV_DUPLEX = 0x000B,
    CDP_TLV_MANAGEMENT_ADDRESS = 0x0016,
} cdp_tlv_type_t;

typedef struct {
    uint8_t source_mac[6];
    char device_id[64];
    char port_id[64];
    char software_version[128];
    char platform[64];
    char management_address[16];
    uint32_t capabilities;
    uint8_t ttl;
    uint8_t version;
    bool valid;
} cdp_info_t;

/** Return true when an untagged IEEE 802.3 frame has the CDP LLC/SNAP envelope. */
bool is_cdp(const uint8_t* frame, uint16_t length);

/** Parse and validate an untagged CDP frame, including its checksum and TLVs. */
bool cdp_parse(const uint8_t* frame, uint16_t length, cdp_info_t* info);

/** Convert parsed CDP information into the shared neighbor representation. */
bool cdp_fill_neighbor(const cdp_info_t* info, neighbor_t* neighbor);
