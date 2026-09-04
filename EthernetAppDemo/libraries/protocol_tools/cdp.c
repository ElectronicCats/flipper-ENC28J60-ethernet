#include "cdp.h"

#include <stdio.h>
#include <string.h>

#define CDP_ETHERNET_HEADER_LENGTH 14U
#define CDP_LLC_SNAP_LENGTH        8U
#define CDP_HEADER_LENGTH          4U
#define CDP_MAX_8023_PAYLOAD       1500U
#define CDP_ADDRESS_PROTOCOL_NLPID 0x01U
#define CDP_NLPID_IPV4             0xCCU

static uint16_t cdp_read_u16(const uint8_t* data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static uint32_t cdp_read_u32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) |
           data[3];
}

static bool cdp_get_pdu(
    const uint8_t* frame,
    uint16_t captured_length,
    const uint8_t** pdu,
    uint16_t* pdu_length) {
    static const uint8_t destination[6] = {0x01, 0x00, 0x0C, 0xCC, 0xCC, 0xCC};

    if(!frame ||
       captured_length < CDP_ETHERNET_HEADER_LENGTH + CDP_LLC_SNAP_LENGTH + CDP_HEADER_LENGTH) {
        return false;
    }

    if(memcmp(frame, destination, sizeof(destination)) != 0) {
        return false;
    }

    uint16_t declared_payload_length = cdp_read_u16(frame + 12);
    if(declared_payload_length < CDP_LLC_SNAP_LENGTH + CDP_HEADER_LENGTH ||
       declared_payload_length > CDP_MAX_8023_PAYLOAD ||
       declared_payload_length > captured_length - CDP_ETHERNET_HEADER_LENGTH) {
        return false;
    }

    const uint8_t* llc = frame + CDP_ETHERNET_HEADER_LENGTH;
    if(llc[0] != CDP_SNAP_DSAP || llc[1] != CDP_SNAP_SSAP || llc[2] != CDP_SNAP_CONTROL ||
       llc[3] != CDP_CISCO_OUI_0 || llc[4] != CDP_CISCO_OUI_1 || llc[5] != CDP_CISCO_OUI_2 ||
       cdp_read_u16(llc + 6) != CDP_SNAP_PID) {
        return false;
    }

    if(pdu) {
        *pdu = llc + CDP_LLC_SNAP_LENGTH;
    }
    if(pdu_length) {
        *pdu_length = declared_payload_length - CDP_LLC_SNAP_LENGTH;
    }

    return true;
}

static bool cdp_checksum_is_valid(const uint8_t* data, uint16_t length) {
    uint32_t sum = 0;

    while(length >= 2) {
        sum += cdp_read_u16(data);
        data += 2;
        length -= 2;
    }

    if(length == 1) {
        sum += (uint16_t)data[0] << 8;
    }

    while(sum >> 16) {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)sum == 0xFFFFU;
}

static void cdp_copy_text(const uint8_t* source, uint16_t length, char* destination, size_t size) {
    if(!destination || size == 0) {
        return;
    }

    size_t copy_length = length;
    if(copy_length >= size) {
        copy_length = size - 1;
    }

    for(size_t index = 0; index < copy_length; index++) {
        uint8_t value = source[index];
        destination[index] = (value >= 0x20U && value <= 0x7EU) ? (char)value : ' ';
    }

    while(copy_length > 0 && destination[copy_length - 1] == ' ') {
        copy_length--;
    }
    destination[copy_length] = '\0';
}

static bool cdp_parse_addresses(
    const uint8_t* data,
    uint16_t length,
    char* management_address,
    size_t management_address_size) {
    if(length < 4) {
        return false;
    }

    uint32_t address_count = cdp_read_u32(data);
    size_t offset = 4;

    for(uint32_t address_index = 0; address_index < address_count; address_index++) {
        if(length - offset < 2) {
            return false;
        }

        uint8_t protocol_type = data[offset++];
        uint8_t protocol_length = data[offset++];
        if(protocol_length > length - offset) {
            return false;
        }

        const uint8_t* protocol = data + offset;
        offset += protocol_length;

        if(length - offset < 2) {
            return false;
        }

        uint16_t address_length = cdp_read_u16(data + offset);
        offset += 2;
        if(address_length > length - offset) {
            return false;
        }

        bool is_ipv4 = protocol_type == CDP_ADDRESS_PROTOCOL_NLPID && protocol_length == 1 &&
                       protocol[0] == CDP_NLPID_IPV4;
        if(is_ipv4) {
            if(address_length != 4) {
                return false;
            }

            if(management_address[0] == '\0') {
                snprintf(
                    management_address,
                    management_address_size,
                    "%u.%u.%u.%u",
                    data[offset],
                    data[offset + 1],
                    data[offset + 2],
                    data[offset + 3]);
            }
        }

        offset += address_length;
    }

    return offset == length;
}

bool is_cdp(const uint8_t* frame, uint16_t length) {
    return cdp_get_pdu(frame, length, NULL, NULL);
}

bool cdp_parse(const uint8_t* frame, uint16_t length, cdp_info_t* info) {
    if(!frame || !info) {
        return false;
    }

    const uint8_t* pdu = NULL;
    uint16_t pdu_length = 0;
    if(!cdp_get_pdu(frame, length, &pdu, &pdu_length) || !cdp_checksum_is_valid(pdu, pdu_length)) {
        return false;
    }

    memset(info, 0, sizeof(cdp_info_t));

    info->version = pdu[0];
    info->ttl = pdu[1];
    if(info->version != 1 && info->version != 2) {
        return false;
    }

    memcpy(info->source_mac, frame + 6, sizeof(info->source_mac));

    size_t offset = CDP_HEADER_LENGTH;
    bool saw_tlv = false;
    bool saw_device_id = false;

    while(offset < pdu_length) {
        if(pdu_length - offset < 4) {
            return false;
        }

        const uint8_t* tlv = pdu + offset;
        uint16_t type = cdp_read_u16(tlv);
        uint16_t tlv_length = cdp_read_u16(tlv + 2);
        if(tlv_length < 4 || tlv_length > pdu_length - offset) {
            return false;
        }

        const uint8_t* value = tlv + 4;
        uint16_t value_length = tlv_length - 4;
        saw_tlv = true;

        switch(type) {
        case CDP_TLV_DEVICE_ID:
            cdp_copy_text(value, value_length, info->device_id, sizeof(info->device_id));
            saw_device_id = info->device_id[0] != '\0';
            break;

        case CDP_TLV_ADDRESS:
        case CDP_TLV_MANAGEMENT_ADDRESS:
            if(!cdp_parse_addresses(
                   value,
                   value_length,
                   info->management_address,
                   sizeof(info->management_address))) {
                return false;
            }
            break;

        case CDP_TLV_PORT_ID:
            cdp_copy_text(value, value_length, info->port_id, sizeof(info->port_id));
            break;

        case CDP_TLV_CAPABILITIES:
            if(value_length != 4) {
                return false;
            }
            info->capabilities = cdp_read_u32(value);
            break;

        case CDP_TLV_SOFTWARE_VERSION:
            cdp_copy_text(
                value, value_length, info->software_version, sizeof(info->software_version));
            break;

        case CDP_TLV_PLATFORM:
            cdp_copy_text(value, value_length, info->platform, sizeof(info->platform));
            break;

        default:
            break;
        }

        offset += tlv_length;
    }

    if(!saw_tlv || !saw_device_id) {
        return false;
    }

    info->valid = true;
    return true;
}

static void cdp_store_description(const cdp_info_t* info, char* output, size_t output_size) {
    if(output && output_size > 0) {
        output[0] = '\0';
    }
    if(output_size < 4 || (!info->platform[0] && !info->software_version[0])) {
        return;
    }

    const size_t separator_length = 3;
    const size_t content_capacity = output_size - separator_length - 1;
    const size_t platform_length = strlen(info->platform);
    const size_t software_length = strlen(info->software_version);

    size_t platform_copy = platform_length < 47U ? platform_length : 47U;
    size_t software_copy = software_length < 77U ? software_length : 77U;
    size_t used = platform_copy + software_copy;

    if(used < content_capacity && software_copy < software_length) {
        size_t extra = software_length - software_copy;
        if(extra > content_capacity - used) {
            extra = content_capacity - used;
        }
        software_copy += extra;
        used += extra;
    }

    if(used < content_capacity && platform_copy < platform_length) {
        size_t extra = platform_length - platform_copy;
        if(extra > content_capacity - used) {
            extra = content_capacity - used;
        }
        platform_copy += extra;
    }

    memcpy(output, info->platform, platform_copy);
    memcpy(output + platform_copy, " | ", separator_length);
    memcpy(output + platform_copy + separator_length, info->software_version, software_copy);
    output[platform_copy + separator_length + software_copy] = '\0';
}

bool cdp_fill_neighbor(const cdp_info_t* info, neighbor_t* neighbor) {
    if(!info || !neighbor || !info->valid) {
        return false;
    }

    memset(neighbor, 0, sizeof(neighbor_t));
    memcpy(neighbor->mac, info->source_mac, sizeof(neighbor->mac));
    snprintf(neighbor->name, sizeof(neighbor->name), "%s", info->device_id);
    snprintf(neighbor->port, sizeof(neighbor->port), "%s", info->port_id);
    snprintf(
        neighbor->management_address,
        sizeof(neighbor->management_address),
        "%s",
        info->management_address);

    cdp_store_description(info, neighbor->description, sizeof(neighbor->description));

    neighbor->ttl = info->ttl;

    // The shared model and current details UI intentionally expose the low 16-bit capability set.
    neighbor->capabilities = (uint16_t)(info->capabilities & 0xFFFFU);
    neighbor->discovery_sources = NEIGHBOR_SOURCE_CDP;
    neighbor->occupied = true;

    return true;
}
