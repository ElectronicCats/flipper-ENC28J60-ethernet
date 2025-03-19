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
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[208];
    uint8_t magic_cookie[4];
    uint8_t dhcp_options[1000];
} dhcp_message_t;

dhcp_message_t dhcp_message_discover(uint8_t* MAC_ADDRESS, uint32_t xid, uint16_t* len);

dhcp_message_t dhcp_message_request(
    uint8_t* MAC_ADDRESS,
    uint32_t xid,
    uint8_t* ip_client,
    uint8_t* ip_server,
    uint16_t* len);

dhcp_message_t dhcp_deconstruct_dhcp_message(uint8_t* buffer);

bool dhcp_is_discover(dhcp_message_t message);

bool dhcp_is_offer(dhcp_message_t message);

bool dhcp_is_request(dhcp_message_t message);

bool dhcp_is_acknoledge(dhcp_message_t message);

bool is_dhcp(uint8_t* buffer);

#endif
