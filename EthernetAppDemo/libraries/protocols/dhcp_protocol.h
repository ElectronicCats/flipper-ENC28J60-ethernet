#ifndef DHCP_SETUP_H_
#define DHCP_SETUP_H_

#include <furi.h>
#include <furi_hal.h>

void set_dhcp_discover_message(uint8_t* buffer, uint16_t* length);

bool deconstruct_dhcp_offer(uint8_t* buffer);

void set_dhcp_request_message(uint8_t* buffer, uint16_t* length);

bool deconstruct_dhcp_ack(uint8_t* buffer);

#endif
