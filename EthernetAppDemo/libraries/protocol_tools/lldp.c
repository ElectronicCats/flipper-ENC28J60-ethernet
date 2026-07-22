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
    if(tlv_length < 2) {
        return;
    }

    uint8_t subtype = ptr[0];

    info->chassis_subtype = subtype;

    switch(subtype) {
    case LLDP_CHASSIS_MAC_ADDRESS:

        if(tlv_length == 7) {
            lldp_mac_to_string(&ptr[1], info->chassis_id, sizeof(info->chassis_id));
        }

        break;

    case LLDP_CHASSIS_NETWORK_ADDRESS:

        if(tlv_length >= 6 && ptr[1] == 1) {
            lldp_ipv4_to_string(&ptr[2], info->chassis_id, sizeof(info->chassis_id));
        }

        break;

    case LLDP_CHASSIS_INTERFACE_NAME:

    case LLDP_CHASSIS_INTERFACE_ALIAS:

    case LLDP_CHASSIS_LOCAL:

    case LLDP_CHASSIS_COMPONENT:

    case LLDP_CHASSIS_PORT_COMPONENT:

        lldp_copy_string_field(
            &ptr[1], tlv_length - 1, info->chassis_id, sizeof(info->chassis_id));

        break;

    default:

        lldp_copy_string_field(
            &ptr[1], tlv_length - 1, info->chassis_id, sizeof(info->chassis_id));

        break;
    }
}

static void lldp_parse_port_id(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 2) return;

    info->port_subtype = ptr[0];

    lldp_copy_string_field(&ptr[1], tlv_length - 1, info->port_id, sizeof(info->port_id));
}

static void
    lldp_parse_port_description(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length == 0) {
        return;
    }

    lldp_copy_string_field(
        ptr, tlv_length, info->port_description, sizeof(info->port_description));
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

static void lldp_parse_organizational(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(!ptr || !info || tlv_length < 4) {
        return;
    }

    uint32_t oui = ((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2];

    uint8_t subtype = ptr[3];

    FURI_LOG_I(
        "LLDP", "ORG TLV OUI=%06lX subtype=%u len=%u", (unsigned long)oui, subtype, tlv_length);

    /*
     * IEEE 802.1 VLAN
     */
    if(oui == LLDP_OUI_IEEE_8021) {
        FURI_LOG_I("LLDP", "802.1 TLV subtype=%u len=%u", subtype, tlv_length);

        if(subtype == LLDP_8021_PORT_VLAN_ID && tlv_length >= 6) {
            info->vlan_id = ((uint16_t)ptr[4] << 8) | ptr[5];

            info->has_vlan = true;

            FURI_LOG_I("LLDP", "VLAN ID detected: %u", info->vlan_id);
        }

    }

    /*
     * IEEE 802.3 Power via MDI
     */
    else if(oui == LLDP_OUI_IEEE_8023) {
        FURI_LOG_I("LLDP", "802.3 TLV subtype=%u len=%u", subtype, tlv_length);

        if(subtype == LLDP_8023_POWER_VIA_MDI && tlv_length >= 7) {
            uint8_t mdi_power_support = ptr[4];
            uint8_t power_class = ptr[6];

            bool is_pse = mdi_power_support & 0x01;
            bool is_pd = mdi_power_support & 0x02;
            bool power_enabled = mdi_power_support & 0x04;

            FURI_LOG_I(
                "LLDP",
                "PoE support=0x%02X class=%u PSE=%u PD=%u enabled=%u",
                mdi_power_support,
                power_class,
                is_pse,
                is_pd,
                power_enabled);

            /*
         * Mapear clase PoE a potencia típica (mW)
         * IEEE 802.3af/at
         */
            static const uint16_t poe_class_mw[] = {
                15400, /* Clase 0 */
                4000, /* Clase 1 */
                7000, /* Clase 2 */
                15400, /* Clase 3 */
                30000, /* Clase 4 (PoE+) */
                45000, /* Clase 5 */
                60000, /* Clase 6 */
                65000, /* Clase 7 (saturado) */
                65000 /* Clase 8 (saturado) */
            };

            if(power_class <= 8) {
                info->poe_power_mw = poe_class_mw[power_class];
                info->has_poe = true;

                FURI_LOG_I(
                    "LLDP", "PoE detected: class=%u power=%u mW", power_class, info->poe_power_mw);
            }
        }
    }
}

bool lldp_fill_neighbor(const lldp_info_t* info, neighbor_t* neighbor) {
    if(!info || !neighbor || !info->valid) {
        return false;
    }

    memset(neighbor, 0, sizeof(neighbor_t));

    memcpy(neighbor->mac, info->source_mac, 6);

    strncpy(neighbor->chassis_id, info->chassis_id, sizeof(neighbor->chassis_id) - 1);

    neighbor->chassis_subtype = info->chassis_subtype;

    strncpy(neighbor->name, info->system_name, sizeof(neighbor->name) - 1);
    strncpy(neighbor->port, info->port_id, sizeof(neighbor->port) - 1);
    strncpy(
        neighbor->port_description,
        info->port_description,
        sizeof(neighbor->port_description) - 1);
    strncpy(
        neighbor->management_address,
        info->management_address,
        sizeof(neighbor->management_address) - 1);

    strncpy(neighbor->description, info->system_description, sizeof(neighbor->description) - 1);

    neighbor->ttl = info->ttl;
    neighbor->capabilities = info->system_capabilities;

    if(info->has_vlan) {
        neighbor->vlan_id = info->vlan_id;
        neighbor->has_vlan = true;
    }

    if(info->has_poe) {
        neighbor->poe_power_mw = info->poe_power_mw;
        neighbor->has_poe = true;
    }

    neighbor->discovery_sources = NEIGHBOR_SOURCE_LLDP;
    neighbor->last_seen_source = NEIGHBOR_SOURCE_LLDP;
    neighbor->occupied = true;

    return true;
}

bool is_lldp(const uint8_t* buffer) {
    if(buffer == NULL) return false;

    ethernet_header_t header = ethernet_get_header((uint8_t*)buffer);

    uint16_t type = header.type[0] << 8 | header.type[1];

    return type == LLDP_ETHERTYPE;
}

bool lldp_parse(const uint8_t* frame, uint16_t length, lldp_info_t* info) {
    if(!frame || !info) return false;

    if(!is_lldp(frame)) {
        return false;
    }

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

        FURI_LOG_I("LLDP", "TLV type=%u len=%u", tlv_type, tlv_length);

        ptr += 2;

        if(ptr + tlv_length > end) {
            FURI_LOG_E("LLDP", "Malformed TLV type=%u len=%u", tlv_type, tlv_length);
            return false;
        }

        if(tlv_type == LLDP_TLV_END) {
            if(info->has_chassis_id && info->has_port_id && info->has_ttl) {
                info->valid = true;
                return true;
            }

            return false;
        }

        switch(tlv_type) {
        case LLDP_TLV_CHASSIS_ID:
            lldp_parse_chassis_id(ptr, tlv_length, info);
            info->has_chassis_id = true;
            break;

        case LLDP_TLV_PORT_ID:
            lldp_parse_port_id(ptr, tlv_length, info);
            info->has_port_id = true;
            break;

        case LLDP_TLV_TTL:
            lldp_parse_ttl(ptr, tlv_length, info);
            info->has_ttl = true;
            break;

        case LLDP_TLV_PORT_DESCRIPTION:
            lldp_parse_port_description(ptr, tlv_length, info);
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

        case LLDP_TLV_ORGANIZATIONAL:
            lldp_parse_organizational(ptr, tlv_length, info);
            break;

        default:
            break;
        }

        ptr += tlv_length;
    }

    return false;
}
