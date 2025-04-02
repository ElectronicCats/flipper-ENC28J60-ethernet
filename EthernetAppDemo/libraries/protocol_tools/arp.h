#ifndef ARP_H_
#define ARP_H_

#include <furi.h>
#include <furi_hal.h>

#define ARP_LEN 28

typedef struct {
    uint8_t hardware_type[2];
    uint8_t protocol_type[2];
    uint8_t hardware_length;
    uint8_t protocol_length;
    uint8_t operation_code[2];
    uint8_t mac_source[6];
    uint8_t ip_source[4];
    uint8_t mac_destiny[6];
    uint8_t ip_destiny[6];
} arp_header_t;

bool arp_set_header_ipv4(
    uint8_t* buffer,
    uint8_t* MAC_SRC,
    uint8_t* MAC_DES,
    uint8_t* IP_SRC,
    uint8_t* IP_DES,
    uint16_t opcode);

bool is_arp(uint8_t* buffer);

arp_header_t arp_get_header(uint8_t* buffer);

#endif
