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

    lldp_copy_string_field(&ptr[1], tlv_length - 1, info->port_id, sizeof(info->port_id));
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

        case LLDP_TLV_SYSTEM_DESCRIPTION:
            lldp_parse_system_description(ptr, tlv_length, info);
            break;

        case LLDP_TLV_SYSTEM_CAPABILITIES:
            lldp_parse_system_capabilities(ptr, tlv_length, info);
            break;

        case LLDP_TLV_MANAGEMENT_ADDRESS:
            lldp_parse_management_address(ptr, tlv_length, info);
            break;

        default:
            break;
        }

        ptr += tlv_length;
    }

    return false;
}
