#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include "enc28j60.h"

typedef struct {
    uint8_t mac_destination[6];
    uint8_t mac_source[6];
    uint8_t type[2];
} ethernet_header_t;

typedef enc28j60_t ethernet_t;

/**
 *
 * To init the ethernet
 *
 */

ethernet_t* ethernet_init(uint8_t* MAC);
void ethernet_deinit(ethernet_t* ethernet);

bool set_ethernet_header(
    ethernet_header_t* ethernet_header,
    uint8_t* mac_origin,
    uint8_t* mac_destination,
    uint8_t* type);

#endif
