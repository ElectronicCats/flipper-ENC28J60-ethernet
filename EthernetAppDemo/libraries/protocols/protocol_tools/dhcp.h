#ifndef _DHCP_H_
#define _DHCP_H_

#include <furi.h>
#include <furi_hal.h>

typedef struct {
    uint8_t operation;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint8_t xid[4];
    uint8_t secs[2];
    uint8_t flags[2];
    uint8_t chaddr[208];
    uint8_t magic_cookie[4];
    uint8_t* dhcp_options;
} dhcp_message_t;

#endif
