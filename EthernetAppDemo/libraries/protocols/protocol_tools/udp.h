#ifndef _UDP_H_
#define _UDP_H_

#include <furi.h>
#include <furi_hal.h>

#
#define UDP_HEADER_LEN 8

typedef struct {
    uint8_t source_port[2]; // Source Port(2 bytes)
    uint8_t dest_port[2]; // Puerto de destino (2 bytes)
    uint8_t length[2]; // header length + data (2 bytes)
    uint8_t checksum[2]; // Checksum (2 bytes)
} udp_header_t;

// Set the header UDP
bool set_udp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t destination_port,
    uint16_t length);

// Get the UDP header from the message
udp_header_t udp_get_header(uint8_t* buffer);

// To know if the buffer is UDP
bool is_udp_packet(uint8_t* buffer);

#endif
