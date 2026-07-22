#include "cdp.h"
#include "ethernet_protocol.h"

#define CDP_ETHERNET_HEADER_LEN 14
#define CDP_HEADER_LEN          4
#define CDP_LLC_SNAP_LEN        8

static void cdp_copy_string(const uint8_t* src, uint16_t length, char* dst, size_t dst_size) {
    if(!src || !dst || dst_size == 0) return;

    size_t copy_length = length;

    if(copy_length >= dst_size) copy_length = dst_size - 1;

    memcpy(dst, src, copy_length);

    dst[copy_length] = '\0';
}

static void cdp_parse_device_id(const uint8_t* data, uint16_t length, cdp_info_t* info) {
    cdp_copy_string(data, length, info->device_id, sizeof(info->device_id));
}

static void cdp_parse_port_id(const uint8_t* data, uint16_t length, cdp_info_t* info) {
    cdp_copy_string(data, length, info->port_id, sizeof(info->port_id));
}

static void cdp_parse_platform(const uint8_t* data, uint16_t length, cdp_info_t* info) {
    cdp_copy_string(data, length, info->platform, sizeof(info->platform));
}

static void cdp_parse_software(const uint8_t* data, uint16_t length, cdp_info_t* info) {
    cdp_copy_string(data, length, info->software_version, sizeof(info->software_version));
}

static void cdp_parse_capabilities(const uint8_t* data, uint16_t length, cdp_info_t* info) {
    if(length < 4) return;

    info->capabilities = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                         ((uint32_t)data[2] << 8) | data[3];
}

static void cdp_parse_address(const uint8_t* data, uint16_t length, cdp_info_t* info) {
    /*
     * CDP Address TLV:
     *
     * Number of addresses
     * Protocol type
     * Protocol length
     * Protocol
     * Address
     */

    if(length < 4) return;

    /*
     * We only handle IPv4
     */

    uint8_t protocol_length = data[3];

    if(protocol_length != 1) return;

    if(length < 10) return;

    /*
     * IPv4 address starts after:
     *
     * count       4 bytes
     * protocol id 1
     * length      1
     * protocol    1
     *
     */

    const uint8_t* ip = &data[6];

    snprintf(
        info->management_address,
        sizeof(info->management_address),
        "%u.%u.%u.%u",
        ip[0],
        ip[1],
        ip[2],
        ip[3]);
}

bool is_cdp(const uint8_t* buffer, uint16_t length) {
    if(!buffer || length < 22) {
        return false;
    }

    ethernet_header_t header = ethernet_get_header((uint8_t*)buffer);

    uint8_t cdp_mac[] = {0x01, 0x00, 0x0C, 0xCC, 0xCC, 0xCC};

    if(memcmp(header.mac_destination, cdp_mac, 6) != 0) {
        return false;
    }

    /*
     * LLC SNAP
     */

    if(buffer[14] != 0xAA || buffer[15] != 0xAA || buffer[16] != 0x03) {
        return false;
    }

    /*
     * Cisco OUI
     */

    if(buffer[17] != 0x00 || buffer[18] != 0x00 || buffer[19] != 0x0C) {
        return false;
    }

    if(buffer[20] != 0x20 || buffer[21] != 0x00) {
        return false;
    }

    return true;
}

bool cdp_parse(const uint8_t* frame, uint16_t length, cdp_info_t* info) {
    if(!frame || !info) return false;

    if(length < 30) return false;

    if(!is_cdp(frame, length)) return false;

    memset(info, 0, sizeof(cdp_info_t));

    ethernet_header_t header = ethernet_get_header((uint8_t*)frame);

    memcpy(info->source_mac, header.mac_source, 6);

    /*
     * CDP header begins after:
     *
     * Ethernet 14
     * LLC/SNAP 8
     */

    const uint8_t* ptr = frame + 22;

    uint8_t version = ptr[0];

    uint8_t ttl = ptr[1];

    UNUSED(version);
    UNUSED(ttl);

    /*
     * Skip:
     * version
     * ttl
     * checksum
     */

    ptr += 4;

    const uint8_t* end = frame + length;

    while(ptr + 4 <= end) {
        uint16_t type = (ptr[0] << 8) | ptr[1];

        uint16_t tlv_length = (ptr[2] << 8) | ptr[3];

        if(tlv_length < 4) break;

        if(ptr + tlv_length > end) break;

        uint16_t data_length = tlv_length - 4;

        const uint8_t* data = ptr + 4;

        switch(type) {
        case CDP_TLV_DEVICE_ID:
            cdp_parse_device_id(data, data_length, info);
            break;

        case CDP_TLV_ADDRESS:
            cdp_parse_address(data, data_length, info);
            break;

        case CDP_TLV_PORT_ID:
            cdp_parse_port_id(data, data_length, info);
            break;

        case CDP_TLV_CAPABILITIES:
            cdp_parse_capabilities(data, data_length, info);
            break;

        case CDP_TLV_SOFTWARE_VERSION:
            cdp_parse_software(data, data_length, info);
            break;

        case CDP_TLV_PLATFORM:
            cdp_parse_platform(data, data_length, info);
            break;

        default:
            break;
        }

        ptr += tlv_length;
    }

    info->valid = true;

    return true;
}

bool cdp_fill_neighbor(const cdp_info_t* info, neighbor_t* neighbor) {
    if(!info || !neighbor || !info->valid) {
        return false;
    }

    memset(neighbor, 0, sizeof(neighbor_t));

    memcpy(neighbor->mac, info->source_mac, 6);

    strncpy(neighbor->name, info->device_id, sizeof(neighbor->name) - 1);

    strncpy(neighbor->port, info->port_id, sizeof(neighbor->port) - 1);

    strncpy(
        neighbor->management_address,
        info->management_address,
        sizeof(neighbor->management_address) - 1);

    snprintf(
        neighbor->description,
        sizeof(neighbor->description),
        "%.63s %.63s",
        info->platform,
        info->software_version);

    neighbor->capabilities = info->capabilities;

    neighbor->discovery_sources = NEIGHBOR_SOURCE_CDP;
    neighbor->last_seen_source = NEIGHBOR_SOURCE_CDP;
    neighbor->occupied = true;

    return true;
}
