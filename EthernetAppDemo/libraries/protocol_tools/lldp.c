#include "lldp.h"
#include "ethernet_protocol.h"

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

static void lldp_parse_chassis_id(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 2) return;

    uint8_t subtype = ptr[0];

    switch(subtype) {
    case LLDP_CHASSIS_MAC_ADDRESS:

        if(tlv_length == 7) {
            lldp_mac_to_string(&ptr[1], info->chassis_id, sizeof(info->chassis_id));
        }

        break;

    default:
        break;
    }
}

static void lldp_parse_port_id(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length < 2) return;

    size_t port_length = tlv_length - 1;

    if(port_length >= sizeof(info->port_id)) port_length = sizeof(info->port_id) - 1;

    memcpy(info->port_id, &ptr[1], port_length);

    info->port_id[port_length] = '\0';
}

static void lldp_parse_ttl(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length != 2) return;

    info->ttl = ((uint16_t)ptr[0] << 8) | ptr[1];
}

static void lldp_parse_system_name(const uint8_t* ptr, uint16_t tlv_length, lldp_info_t* info) {
    if(tlv_length == 0) return;

    size_t length = tlv_length;

    if(length >= sizeof(info->system_name)) length = sizeof(info->system_name) - 1;

    memcpy(info->system_name, ptr, length);

    info->system_name[length] = '\0';
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

    while(ptr + 2 <= end) {
        uint16_t tlv_header = (ptr[0] << 8) | ptr[1];

        uint8_t tlv_type = (tlv_header >> 9) & 0x7F;

        uint16_t tlv_length = tlv_header & 0x1FF;

        ptr += 2;

        if(ptr + tlv_length > end) return false;

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

        default:
            break;
        }

        ptr += tlv_length;
    }

    return false;
}
