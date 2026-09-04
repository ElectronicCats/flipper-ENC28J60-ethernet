#pragma once

#include "neighbor_db.h"

#include <stdbool.h>
#include <stdint.h>

#define EAPOL_ETHERTYPE 0x888EU

typedef enum {
    EAPOL_PACKET_EAP = 0,
    EAPOL_PACKET_START = 1,
    EAPOL_PACKET_LOGOFF = 2,
    EAPOL_PACKET_KEY = 3,
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
    EAP_TYPE_MD5_CHALLENGE = 4,
    EAP_TYPE_OTP = 5,
    EAP_TYPE_GTC = 6,
    EAP_TYPE_TLS = 13,
    EAP_TYPE_SIM = 18,
    EAP_TYPE_TTLS = 21,
    EAP_TYPE_AKA = 23,
    EAP_TYPE_PEAP = 25,
} eap_type_t;

typedef struct {
    uint8_t source_mac[6];
    char identity[64];
    uint8_t version;
    uint8_t packet_type;
    uint8_t eap_code;
    uint8_t eap_type;
    bool has_identity;
    bool valid;
} eapol_info_t;

/** Return true when an untagged Ethernet-II frame has EtherType 0x888E. */
bool is_eapol(const uint8_t* frame, uint16_t length);

/** Parse a bounded EAPOL observation without retaining packet or key material. */
bool eapol_parse(const uint8_t* frame, uint16_t length, eapol_info_t* info);

/** Convert parsed EAPOL metadata into the shared observation representation. */
bool eapol_fill_neighbor(const eapol_info_t* info, neighbor_t* neighbor);
