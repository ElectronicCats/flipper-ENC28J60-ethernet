#ifndef LLDP_H_
#define LLDP_H_

#include <furi.h>
#include <furi_hal.h>

#define LLDP_ETHERTYPE      0x88CC
#define LLDP_OUI_IEEE_802_1 0x0080C2
#define LLDP_OUI_IEEE_802_3 0x00120F
#define LLDP_OUI_LLDP_MED   0x0012BB

#define LLDP_ORG_SUBTYPE_PVID      1
#define LLDP_ORG_SUBTYPE_PPVID     2
#define LLDP_ORG_SUBTYPE_VLAN_NAME 3

#define LLDP_8023_SUBTYPE_POWER_VIA_MDI 2

#define LLDP_MED_SUBTYPE_NETWORK_POLICY 2
#define LLDP_MED_SUBTYPE_EXT_POWER      4

#include "neighbor_db.h"

/**
 * @brief LLDP TLV types defined by IEEE 802.1AB.
 */
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

#define LLDP_TLV_ORG_SPECIFIC 127

// IEEE 802.1 organizationally specific TLV
#define LLDP_OUI_8021_0 0x00
#define LLDP_OUI_8021_1 0x80
#define LLDP_OUI_8021_2 0xC2

#define LLDP_8021_SUBTYPE_PORT_VLAN_ID 1

// IEEE 802.3 organizationally specific TLV
#define LLDP_OUI_8023_0 0x00
#define LLDP_OUI_8023_1 0x12
#define LLDP_OUI_8023_2 0x0F

#define LLDP_8023_SUBTYPE_POWER_VIA_MDI 2

/**
 * @brief LLDP Chassis ID subtypes defined by IEEE 802.1AB.
 */
typedef enum {
    LLDP_CHASSIS_COMPONENT = 1,
    LLDP_CHASSIS_INTERFACE_ALIAS = 2,
    LLDP_CHASSIS_PORT_COMPONENT = 3,
    LLDP_CHASSIS_MAC_ADDRESS = 4,
    LLDP_CHASSIS_NETWORK_ADDRESS = 5,
    LLDP_CHASSIS_INTERFACE_NAME = 6,
    LLDP_CHASSIS_LOCAL = 7,
} lldp_chassis_subtype_t;

/**
 * @brief Parsed information extracted from an LLDP Data Unit (LLDPDU).
 *
 * This structure stores the most relevant information advertised by a
 * neighboring device according to IEEE 802.1AB. Fields that are not
 * present in the received LLDP frame remain empty or set to zero.
 */
typedef struct {
    /* Standard LLDP information */
    char chassis_id[64];
    char port_id[64];
    char system_name[64];
    char system_description[128];
    char management_address[48];

    uint8_t source_mac[6];

    uint16_t ttl;
    uint16_t system_capabilities;
    uint16_t enabled_capabilities;

    /* IEEE 802.1 VLAN information */
    uint16_t vlan_id;
    uint16_t pvid;
    char vlan_name[64];

    bool has_pvid;
    bool has_vlan_name;

    /* LLDP-MED Network Policy */
    uint16_t network_policy_vlan;
    bool has_network_policy;

    /* IEEE 802.3 / LLDP-MED PoE information */
    bool poe_supported;

    uint8_t poe_power_pair;
    uint8_t poe_power_class;

    uint8_t poe_type_source_priority;

    uint16_t poe_requested_power;
    uint16_t poe_allocated_power;

    uint16_t poe_power_watts;
    uint16_t poe_requested_power_watts;
    uint16_t poe_allocated_power_watts;

    uint8_t poe_power_type;
    uint8_t poe_power_source;
    uint8_t poe_power_priority;

    bool has_poe;
    bool has_poe_power_values;

    /* Parser status */
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

bool lldp_fill_neighbor(const lldp_info_t* info, neighbor_t* neighbor);

#endif
