#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <furi.h>
#include <furi_hal.h>

#define ETHERNET_HEADER_LEN 12

typedef struct {
    uint8_t mac_destination[6];
    uint8_t mac_source[6];
    uint8_t type[2];
} ethernet_header_t;

// Set the ethernet header
bool set_ethernet_header(
    uint8_t* buffer,
    uint8_t* mac_origin,
    uint8_t* mac_destination,
    uint16_t type);

// Get the ethernet header from the message
ethernet_header_t ethernet_get_header(uint8_t* buffer);

#endif
