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

bool is_lldp(uint8_t* buffer) {
    if(buffer == NULL) return false;

    ethernet_header_t header = ethernet_get_header(buffer);

    uint16_t type = header.type[0] << 8 | header.type[1];

    return type == LLDP_ETHERTYPE;
}

bool lldp_parse(const uint8_t* frame, uint16_t length, lldp_info_t* info) {
    const uint8_t* ptr = frame + ETHERNET_HEADER_LEN;
    const uint8_t* end = frame + length;

    if(!frame || !info) return false;

    memset(info, 0, sizeof(*info));

    while(ptr + 2 <= end) {
        uint16_t tlv_header = (ptr[0] << 8) | ptr[1];

        uint8_t tlv_type = (tlv_header >> 9) & 0x7F;

        uint16_t tlv_length = tlv_header & 0x1FF;

        ptr += 2;

        if(ptr + tlv_length > end) return false;

        if(tlv_type == LLDP_TLV_END) break;

        switch(tlv_type) {
        case LLDP_TLV_CHASSIS_ID:
            break;

        default:
            break;
        }

        ptr += tlv_length;
    }

    uint8_t dummy_mac[6] = {0};

    lldp_mac_to_string(dummy_mac, info->chassis_id, sizeof(info->chassis_id));

    if(length <= ETHERNET_HEADER_LEN) return false;

    uint16_t offset = ETHERNET_HEADER_LEN;

    while(offset + 2 <= length) {
        uint16_t tlv = ((uint16_t)frame[offset] << 8) | frame[offset + 1];

        uint8_t type = tlv >> 9;
        uint16_t tlv_length = tlv & 0x01FF;

        offset += 2;

        if(offset + tlv_length > length) break;

        switch(type) {
        case LLDP_TLV_TTL:

            if(tlv_length == 2) {
                info->ttl = ((uint16_t)frame[offset] << 8) | frame[offset + 1];
            }

            break;

        case LLDP_TLV_END:
            info->valid = true;
            return true;

        default:
            break;
        }

        offset += tlv_length;
    }

    return false;
}
