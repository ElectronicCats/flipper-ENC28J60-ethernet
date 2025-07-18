#ifndef _DHCP_H_
#define _DHCP_H_

#include <furi.h>
#include <furi_hal.h>

// Parameters of type of message
#define DHCP_DISCOVER   1
#define DHCP_OFFER      2
#define DHCP_REQUEST    3
#define DHCP_ACKNOLEDGE 5

// Parameters
#define DHCP_OP_SUBNET_MASK              1
#define DHCP_OP_TIME_OFFSET              2
#define DHCP_OP_ROUTER                   3
#define DHCP_OP_TIME_SERVER              4
#define DHCP_OP_NAME_SERVER              5
#define DHCP_OP_DOMAIN_NAME_SERVER       6
#define DHCP_OP_LOG_SERVER               7
#define DHCP_OP_COOKIE_SERVER            8
#define DHCP_OP_LPR_SERVER               9
#define DHCP_OP_IMPRESS_SERVER           10
#define DHCP_OP_RESOURCE_LOCATION_SERVER 11
#define DHCP_OP_HOST_NAME                12
#define DHCP_OP_BOOT_FILE_SIZE           13
#define DHCP_OP_MERIT_DUMP_FILE          14
#define DHCP_OP_DOMAIN_NAME              15
#define DHCP_OP_SWAP_SERVER              16
#define DHCP_OP_ROOT_PATH                17
#define DHCP_OP_EXTENSIONS_PATH          18

// IP Parameters
#define DHCP_OP_INTERFACE_MTU               26
#define DHCP_OP_ALL_SUBNETS_ARE_LOCAL       27
#define DHCP_OP_BROADCAST_ADDRESS           28
#define DHCP_OP_PERFORM_MASK_DISCOVERY      29
#define DHCP_OP_MASK_SUPPLIER               30
#define DHCP_OP_PERFOM_ROUTER_DISCOVERY     31
#define DHCP_OP_ROUTER_SOLICITATION_ADDRESS 32
#define DHCP_OP_STATIC_ROUTE                33

// extensions DHCP
#define DHCP_OP_REQUESTED_IP           50
#define DHCP_OP_IP_ADDRESS_LEASE_TIME  51
#define DHCP_OP_OPTION_OVERLOAD        52
#define DHCP_OP_DHCP_MESSAGE_TYPE      53
#define DHCP_OP_SERVER_IDENTIFIER      54
#define DHCP_OP_PARAMETER_REQUEST_LIST 55
#define DHCP_OP_MESSAGE                56
#define DHCP_OP_MAXIMUM_MESSAGE_SIZE   57
#define DHCP_OP_CLIENT_IDENTIFIER      61

// End of message
#define DHCP_END 0xff

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

dhcp_message_t
    dhcp_message_discover(uint8_t* MAC_ADDRESS, uint32_t xid, uint8_t* host_name, uint16_t* len);

dhcp_message_t dhcp_message_request(
    uint8_t* MAC_ADDRESS,
    uint32_t xid,
    uint8_t* ip_client,
    uint8_t* ip_server,
    uint8_t* host_name,
    uint16_t* len);

dhcp_message_t dhcp_deconstruct_dhcp_message(uint8_t* buffer);

bool dhcp_is_discover(dhcp_message_t message);

bool dhcp_is_offer(dhcp_message_t message);

bool dhcp_is_request(dhcp_message_t message);

bool dhcp_is_acknoledge(dhcp_message_t message);

bool is_dhcp(uint8_t* buffer);

bool dhcp_get_option_data(dhcp_message_t message, uint8_t option, uint8_t* data, uint8_t* len_data);

#endif
