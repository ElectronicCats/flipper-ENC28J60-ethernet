#ifndef EAPOL_H_
#define EAPOL_H_

#include <furi.h>
#include <furi_hal.h>

#define EAPOL_ETHERTYPE 0x888E

#include "neighbor_db.h"

typedef enum {
    EAPOL_PACKET_EAP = 0,
    EAPOL_PACKET_START = 1,
    EAPOL_PACKET_LOGOFF = 2,
    EAPOL_PACKET_KEY = 3,
    EAPOL_PACKET_ASF_ALERT = 4,
} eapol_packet_type_t;

typedef enum {
    EAP_CODE_REQUEST = 1,
    EAP_CODE_RESPONSE = 2,
    EAP_CODE_SUCCESS = 3,
    EAP_CODE_FAILURE = 4,
} eap_code_t;

typedef enum {
    EAP_TYPE_IDENTITY = 1,
    EAP_TYPE_NOTIFICATION = 2,
    EAP_TYPE_NAK = 3,
    EAP_TYPE_MD5 = 4,
    EAP_TYPE_OTP = 5,
    EAP_TYPE_GTC = 6,
    EAP_TYPE_TLS = 13,
    EAP_TYPE_SIM = 18,
    EAP_TYPE_TTLS = 21,
    EAP_TYPE_AKA = 23,
    EAP_TYPE_PEAP = 25,
} eap_type_t;

/**
 * @brief Parsed information extracted from an EAPOL frame.
 *
 * Stores the authentication information advertised through
 * IEEE 802.1X EAPOL frames.
 */

typedef struct {
    uint8_t source_mac[6];

    uint8_t version;
    eapol_packet_type_t packet_type;
    uint16_t packet_length;

    eap_code_t code;
    uint8_t identifier;
    uint16_t eap_length;
    eap_type_t type;

    char identity[64];

    bool valid;

    bool has_identity;
    bool is_request;
    bool is_response;
    bool is_start;
    bool is_logoff;
} eapol_info_t;

bool is_eapol(const uint8_t* frame, uint16_t length);

bool eapol_parse(const uint8_t* frame, uint16_t length, eapol_info_t* info);

bool eapol_fill_neighbor(const eapol_info_t* info, neighbor_t* neighbor);

#endif
