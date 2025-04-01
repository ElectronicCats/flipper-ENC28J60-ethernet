#ifndef DHCP_SETUP_H_
#define DHCP_SETUP_H_

#include <furi.h>
#include <furi_hal.h>
#include "chip/enc28j60.h"

bool process_dora(enc28j60_t* ethernet, uint8_t* static_ip, uint8_t* ip_router);

void get_mac_server(uint8_t* MAC_SERVER);

void get_gateway_ip(uint8_t* ip_gateway);

#endif
