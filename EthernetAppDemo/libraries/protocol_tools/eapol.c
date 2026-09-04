#include "eapol.h"

#include <string.h>

#define ETHERNET_HEADER_LENGTH 14U
#define EAPOL_HEADER_LENGTH    4U
#define EAP_HEADER_LENGTH      4U
#define EAP_TYPE_LENGTH        1U

static uint16_t eapol_read_u16(const uint8_t* data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

static bool eapol_version_is_supported(uint8_t version) {
    return version >= 1U && version <= 3U;
}

bool is_eapol(const uint8_t* frame, uint16_t length) {
    if(!frame || length < ETHERNET_HEADER_LENGTH + EAPOL_HEADER_LENGTH) {
        return false;
    }

    return eapol_read_u16(frame + 12) == EAPOL_ETHERTYPE;
}

static void eapol_copy_identity(
    const uint8_t* source,
    uint16_t length,
    char* destination,
    size_t destination_size) {
    if(!destination || destination_size == 0) {
        return;
    }

    size_t copy_length = length;
    if(copy_length >= destination_size) {
        copy_length = destination_size - 1;
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

static bool eapol_parse_eap(const uint8_t* body, uint16_t body_length, eapol_info_t* info) {
    if(body_length < EAP_HEADER_LENGTH) {
        return false;
    }

    uint8_t code = body[0];
    uint16_t eap_length = eapol_read_u16(body + 2);
    if(eap_length < EAP_HEADER_LENGTH || eap_length > body_length) {
        return false;
    }

    info->eap_code = code;

    if(code == EAP_CODE_SUCCESS || code == EAP_CODE_FAILURE) {
        return eap_length == EAP_HEADER_LENGTH;
    }

    if(code != EAP_CODE_REQUEST && code != EAP_CODE_RESPONSE) {
        return false;
    }

    if(eap_length < EAP_HEADER_LENGTH + EAP_TYPE_LENGTH) {
        return false;
    }

    info->eap_type = body[EAP_HEADER_LENGTH];
    if(code == EAP_CODE_RESPONSE && info->eap_type == EAP_TYPE_IDENTITY) {
        uint16_t identity_length = eap_length - EAP_HEADER_LENGTH - EAP_TYPE_LENGTH;
        eapol_copy_identity(
            body + EAP_HEADER_LENGTH + EAP_TYPE_LENGTH,
            identity_length,
            info->identity,
            sizeof(info->identity));
        info->has_identity = info->identity[0] != '\0';
    }

    return true;
}

bool eapol_parse(const uint8_t* frame, uint16_t length, eapol_info_t* info) {
    if(!frame || !info || !is_eapol(frame, length)) {
        return false;
    }

    memset(info, 0, sizeof(*info));

    const uint8_t* header = frame + ETHERNET_HEADER_LENGTH;
    uint8_t version = header[0];
    uint8_t packet_type = header[1];
    uint16_t body_length = eapol_read_u16(header + 2);
    size_t available_length = length - ETHERNET_HEADER_LENGTH - EAPOL_HEADER_LENGTH;

    if(!eapol_version_is_supported(version) || body_length > available_length) {
        return false;
    }

    memcpy(info->source_mac, frame + 6, sizeof(info->source_mac));
    info->version = version;
    info->packet_type = packet_type;

    const uint8_t* body = header + EAPOL_HEADER_LENGTH;
    switch(packet_type) {
    case EAPOL_PACKET_EAP:
        if(!eapol_parse_eap(body, body_length, info)) {
            return false;
        }
        break;

    case EAPOL_PACKET_START:
    case EAPOL_PACKET_LOGOFF:
        if(body_length != 0) {
            return false;
        }
        break;

    case EAPOL_PACKET_KEY:
        // A descriptor type is required; key material remains intentionally opaque.
        if(body_length < 1) {
            return false;
        }
        break;

    default:
        return false;
    }

    info->valid = true;
    return true;
}

bool eapol_fill_neighbor(const eapol_info_t* info, neighbor_t* neighbor) {
    if(!info || !neighbor || !info->valid) {
        return false;
    }

    memset(neighbor, 0, sizeof(*neighbor));
    memcpy(neighbor->mac, info->source_mac, sizeof(neighbor->mac));
    if(info->has_identity) {
        memcpy(neighbor->name, info->identity, sizeof(neighbor->name));
        neighbor->name[sizeof(neighbor->name) - 1] = '\0';
    }

    neighbor->eapol_version = info->version;
    neighbor->eapol_packet_type = info->packet_type;
    neighbor->eap_code = info->eap_code;
    neighbor->eap_type = info->eap_type;
    neighbor->discovery_sources = NEIGHBOR_SOURCE_EAPOL;
    neighbor->occupied = true;

    return true;
}
