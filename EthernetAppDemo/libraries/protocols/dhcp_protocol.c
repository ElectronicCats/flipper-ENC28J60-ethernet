#include "dhcp_protocol.h"
#include "protocol_tools/ethernet_protocol.h"
#include "protocol_tools/ipv4.h"
#include "protocol_tools/udp.h"
#include "protocol_tools/dhcp.h"

#define DHCP_OP_SUBNET_MASK 1
#define DHCP_OP_ROUTER      3
#define DHCP_OP_NAME_SERVER 5

// The Host Name need to be of 8 bytes length
uint8_t HOST[] = "Flipper0";
uint8_t host_size = sizeof(HOST);

uint8_t MAC_ADDRESS[] =
    {0xf, 0x0, 0x23, 0x56, 0x67, 0x89}; // Este lo vamos a cambiar desde el Flipper Zero

uint8_t MAC_BROADCAST[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// The MAC destination for the DHCP SERVER at this case, the gateway or router/modem
uint8_t MAC_DESTINATION[6] = {0};

// Unique identifier for the DHCP communication
const uint32_t xid = 0xDE078987;

// This is for the IPv4 Header
uint8_t source_ip[] = {0x0, 0x0, 0x0, 0x0};
uint8_t destination_ip[] = {0xff, 0xff, 0xff, 0xff};

// The Gatway
uint8_t gateway[4] = {0};
uint8_t dns_server[4] = {0};
uint8_t subnet_mask[4] = {0};

// Our IP
uint8_t myip[4] = {0};

// To put all the IP on zero
uint8_t ip_zeros[4] = {0};

// The IP client and IP server
uint8_t ip_client[4] = {0};
uint8_t ip_server[4] = {0};

// Function to set a Discover Message
void set_dhcp_discover_message(uint8_t* buffer, uint16_t* length) {
    if(buffer == NULL || length == NULL) return;

    uint16_t dhcp_len = 0;

    dhcp_message_t dhcp_message = dhcp_message_discover(MAC_ADDRESS, xid, HOST, &dhcp_len);

    set_ethernet_header(buffer, MAC_ADDRESS, MAC_BROADCAST, 0x800);

    set_ipv4_header(
        buffer + ETHERNET_HEADER_LEN, 0x11, dhcp_len + UDP_HEADER_LEN, source_ip, destination_ip);

    set_udp_header(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, 0x44, 0x43, dhcp_len + UDP_HEADER_LEN);

    memcpy(buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN, &dhcp_message, dhcp_len);

    *length = dhcp_len + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN;
}

// Function to get the dhcp message offer
bool deconstruct_dhcp_offer(uint8_t* buffer) {
    if(buffer == NULL) return false;

    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    uint32_t is_the_xid = dhcp_message.xid[0] << 24 | dhcp_message.xid[1] << 16 |
                          dhcp_message.xid[2] << 8 | dhcp_message.xid[3];

    if(is_the_xid != xid) return false;

    // Get the MAC Adress from the dhcp server
    ethernet_header_t ethernet_header = ethernet_get_header(buffer);
    memcpy(MAC_DESTINATION, ethernet_header.mac_source, 6);

    // dhcp message yiaddr
    memcpy(ip_client, dhcp_message.yiaddr, 4);

    // dhcp message ip server
    memcpy(ip_server, dhcp_message.siaddr, 4);

    return true;
}

// Function to set the dhcp message request
void set_dhcp_request_message(uint8_t* buffer, uint16_t* length) {
    if(buffer == NULL || length == NULL) return;

    uint16_t dhcp_len = 0;

    dhcp_message_t dhcp_message =
        dhcp_message_request(MAC_ADDRESS, xid, ip_client, ip_server, HOST, &dhcp_len);

    set_ethernet_header(buffer, MAC_ADDRESS, MAC_BROADCAST, 0x800);

    set_ipv4_header(
        buffer + ETHERNET_HEADER_LEN, 0x11, dhcp_len + UDP_HEADER_LEN, source_ip, destination_ip);

    set_udp_header(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, 0x44, 0x43, dhcp_len + UDP_HEADER_LEN);

    memcpy(buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN, &dhcp_message, dhcp_len);

    *length = dhcp_len + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN;
}

// Function to deconstruct the acknwoledge message
bool deconstruct_dhcp_ack(uint8_t* buffer) {
    if(buffer == NULL) return false;

    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    memcpy(myip, dhcp_message.yiaddr, 4);

    uint8_t length = 0;

    dhcp_get_option_data(dhcp_message, DHCP_OP_SUBNET_MASK, subnet_mask, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_ROUTER, gateway, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_NAME_SERVER, dns_server, &length);

    return true;
}
