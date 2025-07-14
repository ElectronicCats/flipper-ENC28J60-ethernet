#ifndef PING_MODULE_H_
#define PING_MODULE_H_

#include <furi.h>
#include <furi_hal.h>

#include "../libraries/chip/enc28j60.h"

uint16_t create_flipper_ping_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* payload,
    uint16_t payload_len);

bool process_ping_response(
    enc28j60_t* ethernet,
    uint8_t* ping_packet,
    uint16_t ping_packet_size,
    uint8_t* ip_dest);

#endif
