#ifndef _IPV4_H_
#define _IPV4_H_

#include <furi.h>
#include <furi_hal.h>

typedef struct {
    uint8_t version_ihl; // 4 bits for the versión + 4 bits of header length (IHL)
    uint8_t type_of_service; // type of service
    uint8_t total_length[2]; // Total length (2 bytes)
    uint8_t identification[2]; // Identification (2 bytes)
    uint8_t flags_offset[2]; // 3 bits for flags + 13 bits for fragment offset(2 bytes)
    uint8_t ttl; // time to live (TTL)
    uint8_t protocol; // Protocol (TCP, UDP, etc.)
    uint8_t checksum[2]; // Checksum (2 bytes)
    uint8_t source_ip[4]; // origin ip(4 bytes)
    uint8_t dest_ip[4]; // destination ip(4 bytes)
} ipv4_header_t;

// Function to calculate checksum
uint16_t calculate_checksum(uint8_t* data, uint16_t len);

// Set the header
bool ipv4_set_header(
    ipv4_header_t* header,
    uint8_t protocol,
    uint16_t data_length,
    uint8_t* src_ip,
    uint8_t* dst_ip);

#endif
