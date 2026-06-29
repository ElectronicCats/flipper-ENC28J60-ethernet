#ifndef LLDP_H_
#define LLDP_H_

#include <furi.h>
#include <furi_hal.h>

#define LLDP_ETHERTYPE 0x88CC

typedef enum {
    LLDP_TLV_END = 0,
    LLDP_TLV_CHASSIS_ID = 1,
    LLDP_TLV_PORT_ID = 2,
    LLDP_TLV_TTL = 3,
    LLDP_TLV_PORT_DESCRIPTION = 4,
    LLDP_TLV_SYSTEM_NAME = 5,
    LLDP_TLV_SYSTEM_DESCRIPTION = 6,
    LLDP_TLV_SYSTEM_CAPABILITIES = 7,
    LLDP_TLV_MANAGEMENT_ADDRESS = 8,
} lldp_tlv_type_t;

typedef enum {
    LLDP_CHASSIS_COMPONENT = 1,
    LLDP_CHASSIS_INTERFACE_ALIAS = 2,
    LLDP_CHASSIS_PORT_COMPONENT = 3,
    LLDP_CHASSIS_MAC_ADDRESS = 4,
    LLDP_CHASSIS_NETWORK_ADDRESS = 5,
    LLDP_CHASSIS_INTERFACE_NAME = 6,
    LLDP_CHASSIS_LOCAL = 7,
} lldp_chassis_subtype_t;

typedef struct {
    char chassis_id[64];
    char port_id[64];
    char system_name[64];
    char system_description[128];

    uint16_t ttl;

    bool valid;
} lldp_info_t;

/**
 * @brief Checks whether an Ethernet frame is an LLDP packet.
 *
 * Examines the EtherType field of the Ethernet header and returns
 * true when it corresponds to LLDP (0x88CC).
 *
 * @param buffer Pointer to the beginning of the Ethernet frame.
 * @return true if the frame is LLDP.
 * @return false otherwise.
 */
bool is_lldp(uint8_t* buffer);

/**
 * @brief Parses an LLDP frame.
 *
 * @param frame Pointer to the complete Ethernet frame.
 * @param length Frame length.
 * @param info Parsed LLDP information.
 *
 * @return true if the frame contained a valid LLDPDU.
 * @return false otherwise.
 */
bool lldp_parse(const uint8_t* frame, uint16_t length, lldp_info_t* info);

#endif
