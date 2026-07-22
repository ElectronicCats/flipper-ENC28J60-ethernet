#include "eapol.h"
#include "ethernet_protocol.h"
#include "neighbor_db.h"

bool is_eapol(const uint8_t* frame, uint16_t length) {
    if(!frame || length < ETHERNET_HEADER_LEN + 4) {
        return false;
    }

    //FURI_LOG_I("EAPOL", "EtherType 0x888E detected");

    ethernet_header_t header = ethernet_get_header((uint8_t*)frame);

    uint16_t type = (header.type[0] << 8) | header.type[1];

    return type == EAPOL_ETHERTYPE;
}

bool eapol_parse(const uint8_t* frame, uint16_t length, eapol_info_t* info) {
    if(!frame || !info) {
        return false;
    }

    if(length < ETHERNET_HEADER_LEN + 4) {
        return false;
    }

    memset(info, 0, sizeof(*info));

    ethernet_header_t header = ethernet_get_header((uint8_t*)frame);

    memcpy(info->source_mac, header.mac_source, sizeof(info->source_mac));

    const uint8_t* ptr = frame + ETHERNET_HEADER_LEN;

    info->version = ptr[0];
    info->packet_type = (eapol_packet_type_t)ptr[1];
    info->packet_length = ((uint16_t)ptr[2] << 8) | ptr[3];

    info->is_start = (info->packet_type == EAPOL_PACKET_START);
    info->is_logoff = (info->packet_type == EAPOL_PACKET_LOGOFF);

    if(info->packet_type != EAPOL_PACKET_EAP) {
        info->valid = true;
        return true;
    }

    if(length < ETHERNET_HEADER_LEN + 9) {
        return false;
    }

    const uint8_t* eap = ptr + 4;

    info->code = (eap_code_t)eap[0];
    info->identifier = eap[1];
    info->eap_length = ((uint16_t)eap[2] << 8) | eap[3];
    info->is_request = (info->code == EAP_CODE_REQUEST);
    info->is_response = (info->code == EAP_CODE_RESPONSE);

    if(info->eap_length > length - ETHERNET_HEADER_LEN - 4) {
        return false;
    }

    //FURI_LOG_I("EAPOL", "Code=%u Id=%u Len=%u", info->code, info->identifier, info->eap_length);

    if(info->code == EAP_CODE_SUCCESS || info->code == EAP_CODE_FAILURE) {
        info->valid = true;
        return true;
    }

    if(info->eap_length < 5) {
        return false;
    }

    info->type = (eap_type_t)eap[4];

    //FURI_LOG_I("EAPOL", "EAP Type=%u", info->type);

    if(info->type == EAP_TYPE_IDENTITY) {
        info->has_identity = true;
    }

    if(info->code == EAP_CODE_RESPONSE && info->type == EAP_TYPE_IDENTITY) {
        const uint8_t* identity = &eap[5];

        size_t identity_length = info->eap_length - 5;

        if(identity_length >= sizeof(info->identity)) {
            identity_length = sizeof(info->identity) - 1;
        }

        memcpy(info->identity, identity, identity_length);

        info->identity[identity_length] = '\0';

        //FURI_LOG_I("EAPOL", "Identity='%s'", info->identity);
    }

    /*FURI_LOG_I(
        "EAPOL",
        "Version=%u Type=%u Length=%u Code=%u Id=%u EAP Len=%u",
        info->version,
        info->packet_type,
        info->packet_length,
        info->code,
        info->identifier,
        info->eap_length);*/

    info->valid = true;
    return true;
}

bool eapol_fill_neighbor(const eapol_info_t* info, neighbor_t* neighbor) {
    if(!info || !neighbor || !info->valid) {
        return false;
    }

    memset(neighbor, 0, sizeof(neighbor_t));

    memcpy(neighbor->mac, info->source_mac, 6);

    neighbor->eap_code = info->code;
    neighbor->eapol_version = info->version;
    neighbor->eapol_packet_type = info->packet_type;
    neighbor->eap_type = info->type;

    if(info->has_identity) {
        strncpy(neighbor->eap_identity, info->identity, sizeof(neighbor->eap_identity) - 1);
    }

    neighbor->name[0] = '\0';
    neighbor->discovery_sources = NEIGHBOR_SOURCE_EAPOL;
    neighbor->last_seen_source = NEIGHBOR_SOURCE_EAPOL;
    neighbor->occupied = true;

    return true;
}
