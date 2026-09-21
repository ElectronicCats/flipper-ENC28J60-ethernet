#include "lldp.h"
#include "ethernet_protocol.h"
#include "neighbor_db.h"

static void lldp_mac_to_string(const uint8_t* mac, char* output, size_t output_size) {
    if(!mac || !output || output_size < 18) return;

    snprintf(
        output,
        output_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

static void lldp_ipv4_to_string(const uint8_t* ip, char* output, size_t output_size) {
    if(!ip || !output) return;

    snprintf(output, output_size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void
    lldp_copy_string_field(const uint8_t* src, uint16_t length, char* dst, size_t dst_size) {
    if(!src || !dst || dst_size == 0) return;

    size_t copy_length = length;

    if(copy_length >= dst_size) copy_length = dst_size - 1;

    memcpy(dst, src, copy_length);

    dst[copy_length] = '\0';
}

static void lldp_parse_chassis_id(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 2) return;

    uint8_t subtype = ptr[0];
    info->chassis_id_subtype = subtype;

    switch(subtype) {
    case LLDP_CHASSIS_MAC_ADDRESS:
        if(tlv_length == 7) {
            lldp_mac_to_string(&ptr[1], info->chassis_id, sizeof(info->chassis_id));
        }
        break;

    case LLDP_CHASSIS_NETWORK_ADDRESS:
        if(tlv_length == 6 && ptr[1] == 1) {
            lldp_ipv4_to_string(&ptr[2], info->chassis_id, sizeof(info->chassis_id));
        }
        break;

    case LLDP_CHASSIS_COMPONENT:
    case LLDP_CHASSIS_INTERFACE_ALIAS:
    case LLDP_CHASSIS_PORT_COMPONENT:
    case LLDP_CHASSIS_INTERFACE_NAME:
    case LLDP_CHASSIS_LOCAL:
        lldp_copy_string_field(
            &ptr[1], tlv_length - 1, info->chassis_id, sizeof(info->chassis_id));
        break;

    default:
        break;
    }
}

static void lldp_parse_port_id(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 2) return;

    uint8_t subtype = ptr[0];
    info->port_id_subtype = subtype;

    switch(subtype) {
    case LLDP_PORT_MAC_ADDRESS:
        if(tlv_length == 7) {
            lldp_mac_to_string(&ptr[1], info->port_id, sizeof(info->port_id));
        }
        break;

    case LLDP_PORT_NETWORK_ADDRESS:
        if(tlv_length == 6 && ptr[1] == 1) {
            lldp_ipv4_to_string(&ptr[2], info->port_id, sizeof(info->port_id));
        }
        break;

    case LLDP_PORT_INTERFACE_ALIAS:
    case LLDP_PORT_COMPONENT:
    case LLDP_PORT_INTERFACE_NAME:
    case LLDP_PORT_AGENT_CIRCUIT_ID:
    case LLDP_PORT_LOCAL:
        lldp_copy_string_field(&ptr[1], tlv_length - 1, info->port_id, sizeof(info->port_id));
        break;

    default:
        break;
    }
}

static void lldp_parse_ttl(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length != 2) return;

    info->ttl = ((uint16_t)ptr[0] << 8) | ptr[1];
}

static void lldp_parse_system_name(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length == 0) return;

    lldp_copy_string_field(ptr, tlv_length, info->system_name, sizeof(info->system_name));
}

static void
    lldp_parse_system_description(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length == 0) return;

    lldp_copy_string_field(
        ptr, tlv_length, info->system_description, sizeof(info->system_description));
}

static void
    lldp_parse_system_capabilities(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length != 4) return;

    info->system_capabilities = ((uint16_t)ptr[0] << 8) | ptr[1];

    info->enabled_capabilities = ((uint16_t)ptr[2] << 8) | ptr[3];
}

static void
    lldp_parse_management_address(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 2) return;

    uint8_t address_length = ptr[0];

    if(address_length != 5) return;

    uint8_t subtype = ptr[1];

    if(subtype != 1) return;

    if(tlv_length < 6) return;

    lldp_ipv4_to_string(&ptr[2], info->management_address, sizeof(info->management_address));
}

static void lldp_parse_pvid(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length != 6) return;

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    if(oui != LLDP_OUI_IEEE_802_1) return;

    if(ptr[3] != LLDP_ORG_SUBTYPE_PVID) return;

    info->pvid = ((uint16_t)ptr[4] << 8) | ptr[5];
    info->has_pvid = true;
}

static void lldp_parse_vlan_name(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 7) return;

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    if(oui != LLDP_OUI_IEEE_802_1) return;

    if(ptr[3] != LLDP_ORG_SUBTYPE_VLAN_NAME) return;

    uint16_t vlan_id = ((uint16_t)ptr[4] << 8) | ptr[5];
    uint8_t vlan_name_length = ptr[6];

    if(vlan_name_length == 0) return;

    if((uint16_t)(7 + vlan_name_length) != tlv_length) return;

    lldp_copy_string_field(&ptr[7], vlan_name_length, info->vlan_name, sizeof(info->vlan_name));

    info->vlan_id = vlan_id;
    info->has_vlan_name = true;
}

static void lldp_parse_network_policy(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length != 8) return;

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    if(oui != LLDP_OUI_LLDP_MED) return;

    if(ptr[3] != LLDP_MED_SUBTYPE_NETWORK_POLICY) return;

    /*
     * ptr[4]:
     * Application Type
     *
     * ptr[5]:
     * Policy flags
     *
     * ptr[6..7]:
     * VLAN / priority / DSCP information
     */

    info->network_policy_vlan = ((uint16_t)(ptr[5] & 0x1FU) << 7) | (ptr[6] >> 1);
    info->has_network_policy = true;
}

static void lldp_parse_poe_8023(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(!ptr || !info) return;

    /*
     * IEEE 802.3 Power via MDI TLV:
     *
     * OUI       : 3 bytes
     * Subtype   : 1 byte
     * Power MDI : 1 byte
     * Power Pair: 1 byte
     * Power Class: 1 byte
     *
     * Total: 7 bytes
     */
    if(tlv_length < 7) return;

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    if(oui != LLDP_OUI_IEEE_802_3) return;

    if(ptr[3] != LLDP_8023_SUBTYPE_POWER_VIA_MDI) return;

    /*
     * MDI power support.
     *
     * Bit 0:
     * Device type (1 = PSE, 0 = PD)
     *
     * Bit 1:
     * MDI power supported
     *
     * Bit 2:
     * MDI power enabled
     *
     * Bit 3:
     * PSE pairs controllable
     */
    uint8_t power_mdi = ptr[4];

    info->poe_power_type = (power_mdi & 0x01U) ? 0U : 1U;
    info->poe_supported = (power_mdi & 0x02U) != 0;
    info->poe_type_source_priority = power_mdi;

    /*
     * PSE power pair.
     */
    info->poe_power_pair = ptr[5];

    /*
     * Power class.
     */
    info->poe_power_class = ptr[6];

    /*
     * A valid IEEE 802.3 PoE TLV was received.
     */
    info->has_poe_mdi = true;
    info->has_poe = true;
}

static void lldp_parse_poe_med(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length != 7) return;

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    if(oui != LLDP_OUI_LLDP_MED) return;

    if(ptr[3] != LLDP_MED_SUBTYPE_EXT_POWER) return;

    /*
     * MED Extended Power via MDI:
     *
     * ptr[4]:
     * power type / source / priority
     *
     * ptr[5..6]:
     * power value
     */

    uint8_t power_info = ptr[4];

    info->poe_type_source_priority = power_info;
    info->poe_power_type = (power_info >> 6) & 0x03;
    info->poe_power_source = (power_info >> 4) & 0x03;
    info->poe_power_priority = power_info & 0x0F;

    info->poe_power_watts = ((uint16_t)ptr[5] << 8) | ptr[6];

    info->has_poe = true;
    info->has_poe_power_values = true;
}

static void
    lldp_parse_organizational_tlv(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 4) return;

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    uint8_t subtype = ptr[3];

    switch(oui) {
    case LLDP_OUI_IEEE_802_1:

        if(subtype == LLDP_ORG_SUBTYPE_PVID) {
            lldp_parse_pvid(ptr, tlv_length, info);
        } else if(subtype == LLDP_ORG_SUBTYPE_VLAN_NAME) {
            lldp_parse_vlan_name(ptr, tlv_length, info);
        }

        break;

    case LLDP_OUI_IEEE_802_3:

        if(subtype == LLDP_8023_SUBTYPE_POWER_VIA_MDI) {
            lldp_parse_poe_8023(ptr, tlv_length, info);
        }

        break;

    case LLDP_OUI_LLDP_MED:

        if(subtype == LLDP_MED_SUBTYPE_NETWORK_POLICY) {
            lldp_parse_network_policy(ptr, tlv_length, info);
        } else if(subtype == LLDP_MED_SUBTYPE_EXT_POWER) {
            lldp_parse_poe_med(ptr, tlv_length, info);
        }

        break;

    default:
        break;
    }
}

bool lldp_fill_neighbor(const lldp_info_t* info, neighbor_t* neighbor) {
    if(!info || !neighbor || !info->valid) {
        return false;
    }

    memset(neighbor, 0, sizeof(neighbor_t));

    memcpy(neighbor->mac, info->source_mac, sizeof(neighbor->mac));

    neighbor->lldp_chassis_subtype = info->chassis_id_subtype;
    neighbor->lldp_port_subtype = info->port_id_subtype;

    strncpy(neighbor->name, info->system_name, sizeof(neighbor->name) - 1);

    strncpy(neighbor->port, info->port_id, sizeof(neighbor->port) - 1);

    strncpy(
        neighbor->management_address,
        info->management_address,
        sizeof(neighbor->management_address) - 1);

    strncpy(neighbor->chassis_id, info->chassis_id, sizeof(neighbor->chassis_id) - 1);

    strncpy(neighbor->description, info->system_description, sizeof(neighbor->description) - 1);

    /*
     * Standard LLDP information
     */
    neighbor->ttl = info->ttl;

    neighbor->capabilities = info->system_capabilities;

    neighbor->enabled_capabilities = info->enabled_capabilities;

    /* IEEE 802.1 VLAN information. Keep PVID and VLAN Name VID distinct. */
    neighbor->pvid = info->pvid;
    neighbor->has_pvid = info->has_pvid;
    neighbor->vlan_id = info->vlan_id;

    /*
 * IEEE 802.1 VLAN name
 */
    if(info->has_vlan_name) {
        strncpy(neighbor->vlan_name, info->vlan_name, sizeof(neighbor->vlan_name) - 1);

        neighbor->vlan_name[sizeof(neighbor->vlan_name) - 1] = '\0';
        neighbor->has_vlan_name = true;
    }

    /*
 * LLDP-MED Network Policy
 */
    if(info->has_network_policy) {
        neighbor->network_policy_vlan = info->network_policy_vlan;

        neighbor->has_network_policy = true;
    }

    /*
 * IEEE 802.3 / LLDP-MED PoE information
 */
    neighbor->poe_supported = info->poe_supported;

    neighbor->poe_power_pair = info->poe_power_pair;

    neighbor->poe_power_class = info->poe_power_class;

    neighbor->poe_type_source_priority = info->poe_type_source_priority;

    neighbor->poe_requested_power = info->poe_requested_power;

    neighbor->poe_allocated_power = info->poe_allocated_power;

    neighbor->poe_power_watts = info->poe_power_watts;

    neighbor->poe_requested_power_watts = info->poe_requested_power_watts;

    neighbor->poe_allocated_power_watts = info->poe_allocated_power_watts;

    neighbor->poe_power_type = info->poe_power_type;

    neighbor->poe_power_source = info->poe_power_source;

    neighbor->poe_power_priority = info->poe_power_priority;

    neighbor->has_poe_mdi = info->has_poe_mdi;
    neighbor->has_poe = info->has_poe;

    neighbor->has_poe_power_values = info->has_poe_power_values;

    /*
     * Discovery source
     */
    neighbor->discovery_sources = NEIGHBOR_SOURCE_LLDP;

    neighbor->occupied = true;

    return true;
}

bool is_lldp(uint8_t* buffer) {
    if(buffer == NULL) return false;

    ethernet_header_t header = ethernet_get_header(buffer);

    uint16_t type = header.type[0] << 8 | header.type[1];

    return type == LLDP_ETHERTYPE;
}

bool lldp_parse(const uint8_t* frame, uint16_t length, lldp_info_t* info) {
    if(!frame || !info) return false;

    if(length <= ETHERNET_HEADER_LEN) return false;

    const uint8_t* ptr = frame + ETHERNET_HEADER_LEN;
    const uint8_t* end = frame + length;

    memset(info, 0, sizeof(*info));

    ethernet_header_t header = ethernet_get_header((uint8_t*)frame);

    memcpy(info->source_mac, header.mac_source, sizeof(info->source_mac));

    while(ptr + 2 <= end) {
        uint16_t tlv_header = (ptr[0] << 8) | ptr[1];

        uint8_t tlv_type = (tlv_header >> 9) & 0x7F;

        uint16_t tlv_length = tlv_header & 0x1FF;

        ptr += 2;

        if(ptr + tlv_length > end) {
            FURI_LOG_E("LLDP", "TLV exceeds frame (type=%u len=%u)", tlv_type, tlv_length);

            return false;
        }

        if(tlv_type == LLDP_TLV_END) {
            info->valid = true;
            return true;
        }

        switch(tlv_type) {
        case LLDP_TLV_CHASSIS_ID:
            lldp_parse_chassis_id(ptr, tlv_length, info);
            break;

        case LLDP_TLV_PORT_ID:
            lldp_parse_port_id(ptr, tlv_length, info);
            break;

        case LLDP_TLV_TTL:
            lldp_parse_ttl(ptr, tlv_length, info);
            break;

        case LLDP_TLV_SYSTEM_NAME:
            lldp_parse_system_name(ptr, tlv_length, info);
            break;

        case LLDP_TLV_SYSTEM_DESCRIPTION:
            lldp_parse_system_description(ptr, tlv_length, info);
            break;

        case LLDP_TLV_SYSTEM_CAPABILITIES:
            lldp_parse_system_capabilities(ptr, tlv_length, info);
            break;

        case LLDP_TLV_MANAGEMENT_ADDRESS:
            lldp_parse_management_address(ptr, tlv_length, info);
            break;

        case LLDP_TLV_ORG_SPECIFIC:
            lldp_parse_organizational_tlv(ptr, tlv_length, info);
            break;

        default:
            break;
        }

        ptr += tlv_length;
    }

    return false;
}
