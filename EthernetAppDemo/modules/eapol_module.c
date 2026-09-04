#include "eapol_module.h"

#include <stdio.h>
#include <string.h>

void eapol_module_init(void) {
    neighbor_db_clear_by_source(NEIGHBOR_SOURCE_EAPOL);
}

void eapol_module_reset(void) {
    neighbor_db_clear_by_source(NEIGHBOR_SOURCE_EAPOL);
}

bool eapol_module_process_frame(uint8_t* frame, uint16_t length) {
    eapol_info_t info;
    if(!eapol_parse(frame, length, &info)) {
        return false;
    }

    neighbor_t neighbor;
    if(!eapol_fill_neighbor(&info, &neighbor)) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find_by_source(neighbor.mac, NEIGHBOR_SOURCE_EAPOL);
    if(existing) {
        if(!neighbor.name[0] && existing->name[0]) {
            memcpy(neighbor.name, existing->name, sizeof(neighbor.name));
            neighbor.name[sizeof(neighbor.name) - 1] = '\0';
        }
        return neighbor_db_update(&neighbor);
    }

    return neighbor_db_add(&neighbor);
}

static const char* eapol_get_display_name(void) {
    return "EAPOL";
}

static void eapol_init(App* app) {
    UNUSED(app);
    eapol_module_init();
}

static void eapol_cleanup(App* app) {
    UNUSED(app);
}

static uint8_t eapol_get_details_page_count(neighbor_t* neighbor) {
    UNUSED(neighbor);
    return 3;
}

static const char* eapol_packet_type_name(uint8_t packet_type) {
    switch(packet_type) {
    case EAPOL_PACKET_EAP:
        return "EAP-Packet";
    case EAPOL_PACKET_START:
        return "Start";
    case EAPOL_PACKET_LOGOFF:
        return "Logoff";
    case EAPOL_PACKET_KEY:
        return "Key";
    default:
        return "Unknown";
    }
}

static const char* eap_code_name(const neighbor_t* neighbor) {
    if(neighbor->eapol_packet_type != EAPOL_PACKET_EAP) {
        return "N/A";
    }

    switch(neighbor->eap_code) {
    case EAP_CODE_REQUEST:
        return "Request";
    case EAP_CODE_RESPONSE:
        return "Response";
    case EAP_CODE_SUCCESS:
        return "Success";
    case EAP_CODE_FAILURE:
        return "Failure";
    default:
        return "Unknown";
    }
}

static void eap_type_name(const neighbor_t* neighbor, char* output, size_t output_size) {
    if(neighbor->eapol_packet_type != EAPOL_PACKET_EAP || neighbor->eap_code == EAP_CODE_SUCCESS ||
       neighbor->eap_code == EAP_CODE_FAILURE) {
        snprintf(output, output_size, "N/A");
        return;
    }

    switch(neighbor->eap_type) {
    case EAP_TYPE_IDENTITY:
        snprintf(output, output_size, "Identity");
        break;
    case EAP_TYPE_NOTIFICATION:
        snprintf(output, output_size, "Notification");
        break;
    case EAP_TYPE_NAK:
        snprintf(output, output_size, "NAK");
        break;
    case EAP_TYPE_MD5_CHALLENGE:
        snprintf(output, output_size, "MD5-Challenge");
        break;
    case EAP_TYPE_OTP:
        snprintf(output, output_size, "OTP");
        break;
    case EAP_TYPE_GTC:
        snprintf(output, output_size, "GTC");
        break;
    case EAP_TYPE_TLS:
        snprintf(output, output_size, "TLS");
        break;
    case EAP_TYPE_SIM:
        snprintf(output, output_size, "SIM");
        break;
    case EAP_TYPE_TTLS:
        snprintf(output, output_size, "TTLS");
        break;
    case EAP_TYPE_AKA:
        snprintf(output, output_size, "AKA");
        break;
    case EAP_TYPE_PEAP:
        snprintf(output, output_size, "PEAP");
        break;
    default:
        snprintf(output, output_size, "Type %u", neighbor->eap_type);
        break;
    }
}

static void eapol_build_details_page(
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size) {
    if(!neighbor) {
        return;
    }

    line1[0] = '\0';
    line2[0] = '\0';
    line3[0] = '\0';
    line4[0] = '\0';

    switch(page) {
    case 0:
        snprintf(line1, line1_size, "EAPOL VERSION");
        snprintf(line2, line2_size, "Version %u", neighbor->eapol_version);
        snprintf(line3, line3_size, "PACKET TYPE");
        snprintf(line4, line4_size, "%s", eapol_packet_type_name(neighbor->eapol_packet_type));
        break;

    case 1:
        snprintf(line1, line1_size, "EAP CODE");
        snprintf(line2, line2_size, "%s", eap_code_name(neighbor));
        snprintf(line3, line3_size, "EAP TYPE");
        eap_type_name(neighbor, line4, line4_size);
        break;

    default:
        break;
    }
}

const PassiveProtocolHandler eapol_protocol_handler = {
    .get_display_name = eapol_get_display_name,
    .init = eapol_init,
    .run = NULL,
    .process_frame = eapol_module_process_frame,
    .cleanup = eapol_cleanup,
    .get_details_page_count = eapol_get_details_page_count,
    .build_details_page = eapol_build_details_page,
};
