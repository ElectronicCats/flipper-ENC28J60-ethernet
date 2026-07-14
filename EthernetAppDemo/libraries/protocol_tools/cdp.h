#ifndef CDP_H_
#define CDP_H_

#include <furi.h>
#include <furi_hal.h>

#include "neighbor_db.h"

/**
 * @brief CDP uses LLC/SNAP instead of Ethernet II EtherType.
 */

#define CDP_SNAP_DSAP    0xAA
#define CDP_SNAP_SSAP    0xAA
#define CDP_SNAP_CONTROL 0x03

/**
 * @brief Cisco Organizational Unique Identifier.
 */
#define CDP_CISCO_OUI_0 0x00
#define CDP_CISCO_OUI_1 0x00
#define CDP_CISCO_OUI_2 0x0C

/**
 * @brief CDP TLV types.
 */
typedef enum {
    CDP_TLV_DEVICE_ID = 0x0001,

    CDP_TLV_ADDRESS = 0x0002,

    CDP_TLV_PORT_ID = 0x0003,

    CDP_TLV_CAPABILITIES = 0x0004,

    CDP_TLV_SOFTWARE_VERSION = 0x0005,

    CDP_TLV_PLATFORM = 0x0006,

    CDP_TLV_NATIVE_VLAN = 0x000A,

    CDP_TLV_DUPLEX = 0x000B,

} cdp_tlv_type_t;

/**
 * @brief Parsed CDP information.
 *
 * Contains the relevant information advertised by Cisco
 * devices through CDP.
 */
typedef struct {
    uint8_t source_mac[6];

    char device_id[64];

    char port_id[64];

    char software_version[128];

    char platform[64];

    char management_address[48];

    uint16_t capabilities;

    bool valid;

} cdp_info_t;

/**
 * @brief Check if an Ethernet frame contains CDP.
 */
bool is_cdp(const uint8_t* buffer, uint16_t length);

/**
 * @brief Parse CDP frame.
 */
bool cdp_parse(const uint8_t* frame, uint16_t length, cdp_info_t* info);

/**
 * @brief Convert CDP information into common neighbor format.
 */
bool cdp_fill_neighbor(const cdp_info_t* info, neighbor_t* neighbor);

#endif
