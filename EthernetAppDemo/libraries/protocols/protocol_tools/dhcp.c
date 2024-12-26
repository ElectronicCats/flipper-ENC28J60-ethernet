#include "dhcp.h"

const uint8_t HOST[] = "Flipper0";
const uint8_t host_size = sizeof(HOST);

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

// Throw a dhcp discover message
dhcp_message_t dhcp_message_discover(uint8_t* MAC_ADDRESS, uint32_t xid, uint16_t* len) {
    dhcp_message_t message = {0};
    message.operation = 1;
    message.htype = 1;
    message.hlen = 6;
    message.hops = 0;

    message.xid[0] = (xid >> 24) & 0xff;
    message.xid[1] = (xid >> 16) & 0xff;
    message.xid[2] = (xid >> 8) & 0xff;
    message.xid[3] = (xid) & 0xff;

    memset(message.secs, 0, 2);
    memset(message.flags, 0, 2);

    memcpy(message.chaddr, MAC_ADDRESS, 6);

    message.magic_cookie[0] = 0x63;
    message.magic_cookie[1] = 0x82;
    message.magic_cookie[2] = 0x53;
    message.magic_cookie[3] = 0x63;

    uint16_t size = 0;

    // This is the array with the values to send the request
    uint8_t first_option[] = {DHCP_OP_DHCP_MESSAGE_TYPE, 0x01, DHCP_DISCOVER};
    memcpy(message.dhcp_options, first_option, sizeof(first_option));
    size = sizeof(first_option);

    // This is to send the cliend identifier
    uint8_t second_option[9] = {DHCP_OP_CLIENT_IDENTIFIER, 7, 0x1, 0, 0, 0, 0, 0, 0};
    memcpy(second_option + 3, MAC_ADDRESS, 6);

    memcpy(message.dhcp_options + size, second_option, sizeof(second_option));
    size += sizeof(second_option);

    // The host name
    uint8_t third_option[] = {DHCP_OP_HOST_NAME, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(third_option + 2, HOST, 8);

    memcpy(message.dhcp_options + size, third_option, sizeof(third_option));
    size += sizeof(third_option);

    // The requested list
    uint8_t fourth_option[] = {
        DHCP_OP_PARAMETER_REQUEST_LIST,
        3,
        DHCP_OP_SUBNET_MASK,
        DHCP_OP_ROUTER,
        DHCP_OP_DOMAIN_NAME_SERVER};

    memcpy(message.dhcp_options + size, fourth_option, sizeof(fourth_option));
    size += sizeof(fourth_option);

    message.dhcp_options[size++] = DHCP_END;

    *len = sizeof(dhcp_message_t) - 1000;
    *len += size;

    return message;
}

// Throw a dhcp request message
dhcp_message_t dhcp_message_request(
    uint8_t* MAC_ADDRESS,
    uint32_t xid,
    uint8_t* ip_client,
    uint8_t* ip_server,
    uint16_t* len) {
    dhcp_message_t message = {0};
    message.operation = 1;
    message.htype = 1;
    message.hlen = 6;
    message.hops = 0;

    message.xid[0] = (xid >> 24) & 0xff;
    message.xid[1] = (xid >> 16) & 0xff;
    message.xid[2] = (xid >> 8) & 0xff;
    message.xid[3] = (xid) & 0xff;

    memset(message.secs, 0, 2);
    memset(message.flags, 0, 2);

    memcpy(message.ciaddr, ip_client, 4);

    memcpy(message.siaddr, ip_server, 4);

    memcpy(message.chaddr, MAC_ADDRESS, 6);

    message.magic_cookie[0] = 0x63;
    message.magic_cookie[1] = 0x82;
    message.magic_cookie[2] = 0x53;
    message.magic_cookie[3] = 0x63;

    uint16_t size = 0;

    // This is the array with the values to send the request
    uint8_t first_option[] = {DHCP_OP_DHCP_MESSAGE_TYPE, 0x01, DHCP_REQUEST};
    memcpy(message.dhcp_options, first_option, sizeof(first_option));
    size = sizeof(first_option);

    // This is to send the cliend identifier
    uint8_t second_option[9] = {DHCP_OP_CLIENT_IDENTIFIER, 7, 0x1, 0, 0, 0, 0, 0, 0};
    memcpy(second_option + 3, MAC_ADDRESS, 6);

    memcpy(message.dhcp_options + size, second_option, sizeof(second_option));
    size += sizeof(second_option);

    // The host name
    uint8_t third_option[] = {DHCP_OP_HOST_NAME, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(third_option + 2, HOST, 8);

    memcpy(message.dhcp_options + size, third_option, sizeof(third_option));
    size += sizeof(third_option);

    // Add the IP address
    uint8_t fourth_option[] = {DHCP_OP_REQUESTED_IP, 4, 0, 0, 0, 0};
    memcpy(fourth_option + 2, ip_client, 4);

    memcpy(message.dhcp_options + size, fourth_option, sizeof(fourth_option));
    size += sizeof(fourth_option);

    uint8_t fifth_option[] = {DHCP_OP_SERVER_IDENTIFIER, 4, 0, 0, 0, 0};
    memcpy(fifth_option + 2, ip_server, 4);

    memcpy(message.dhcp_options + size, fifth_option, sizeof(fifth_option));
    size += sizeof(fifth_option);

    // The requested list
    uint8_t sixth_option[] = {
        DHCP_OP_PARAMETER_REQUEST_LIST,
        3,
        DHCP_OP_SUBNET_MASK,
        DHCP_OP_ROUTER,
        DHCP_OP_DOMAIN_NAME_SERVER};

    memcpy(message.dhcp_options + size, sixth_option, sizeof(sixth_option));
    size += sizeof(sixth_option);

    message.dhcp_options[size++] = DHCP_END;

    *len = sizeof(dhcp_message_t) - 1000;
    *len += size;

    return message;
}

// Function to deconstruct the dhcp message
dhcp_message_t dhcp_deconstruct_dhcp_message(uint8_t* payload, uint16_t len) {
    dhcp_message_t message = {0};
    memcpy((uint8_t*)&message, payload, len);

    return message;
}

// Function to know if it is an dhcp offer message
bool dhcp_is_offer(dhcp_message_t message) {
    if(message.operation != 2) return false;
    if(message.dhcp_options[2] != DHCP_OFFER) return false;

    return true;
}

// Function to know if it is an dhcp acknowledge message
bool dhcp_is_acknoledge(dhcp_message_t message) {
    if(message.operation != 2) return false;
    if(message.dhcp_options[2] != DHCP_ACKNOLEDGE) return false;

    return true;
}
