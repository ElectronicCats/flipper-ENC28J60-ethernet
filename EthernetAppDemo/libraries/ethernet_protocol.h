#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <furi.h>
#include <furi_hal.h>

typedef struct {
    uint8_t mac_destination[6];
    uint8_t mac_source[6];
    uint8_t type[2];
} ethernet_header_t;

/**
 *
 * To init the ethernet
 *
 */

bool set_ethernet_header(
    ethernet_header_t* ethernet_header,
    uint8_t* mac_origin,
    uint8_t* mac_destination,
    uint8_t* type);

#endif
