#ifndef ICMP_H
#define ICMP_H

#include <furi.h>
#include <furi_hal.h>

#define ICMP_HEADER_LEN            8
#define ICMP_TYPE_ECHO_REPLY       0
#define ICMP_TYPE_ECHO_REQUEST     8
#define ICMP_TYPE_DEST_UNREACHABLE 3
#define ICMP_TYPE_TIME_EXCEEDED    11
#define ICMP_TYPE_REDIRECT         5

typedef struct {
    uint8_t type;
    uint8_t code;
    uint8_t checksum[2];
    uint8_t identifier[2];
    uint8_t sequence[2];
} icmp_header_t;

bool icmp_set_header(
    uint8_t* buffer,
    uint8_t type,
    uint8_t code,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* data,
    uint16_t data_length);

icmp_header_t icmp_get_header(uint8_t* buffer);

uint16_t icmp_calculate_checksum(icmp_header_t* header, uint8_t* data, uint16_t data_length);

#endif // ICMP_H
