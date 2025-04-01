#include "dhcp_protocol.h"
#include "protocol_tools/ethernet_protocol.h"
#include "protocol_tools/ipv4.h"
#include "protocol_tools/udp.h"
#include "protocol_tools/dhcp.h"
#include "chip/log_user.h"

// The Host Name need to be of 8 bytes length
uint8_t HOST[] = "Flipper0";
uint8_t host_size = sizeof(HOST);

uint8_t MAC_ADDRESS[6] = {0, 0, 0, 0, 0, 0}; // Este lo vamos a cambiar desde el Flipper Zero

uint8_t MAC_BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// The MAC destination for the DHCP SERVER at this case, the gateway or router/modem
uint8_t MAC_DESTINATION[6] = {0};

// Unique identifier for the DHCP communication
uint32_t xid = 0xDE078987;

// This is for the IPv4 Header
uint8_t source_ip[] = {0x0, 0x0, 0x0, 0x0};
uint8_t destination_ip[] = {0xff, 0xff, 0xff, 0xff};

// The Gatway
uint8_t gateway[4] = {0};
uint8_t dns_server[4] = {0};
uint8_t subnet_mask[4] = {0};

// The dhcp_server
uint8_t dhcp_server_ip[4] = {0};

// Our IP
uint8_t myip[4] = {0};

// To put all the IP on zero
uint8_t ip_zeros[4] = {0};

// The IP client and IP server
uint8_t ip_client[4] = {0};
uint8_t ip_server[4] = {0};

// States process
typedef enum {
    DHCP_STATE_INIT,
    DHCP_STATE_REQUEST,
    DHCP_STATE_WAITING,
    DHCP_OK,
    DHCP_FAIL
} state_dora_t;

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

    if(!is_dhcp(buffer)) return false;

    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    if(!dhcp_is_offer(dhcp_message)) return false;

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
// Function to deconstruct the acknowledge message
bool deconstruct_dhcp_ack(uint8_t* buffer) {
    if(buffer == NULL) return false;

    if(!is_dhcp(buffer)) return false;

    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    if(!dhcp_is_acknoledge(dhcp_message)) return false;

    uint32_t is_the_xid = dhcp_message.xid[0] << 24 | dhcp_message.xid[1] << 16 |
                          dhcp_message.xid[2] << 8 | dhcp_message.xid[3];

    if(is_the_xid != xid) return false;

    memcpy(myip, dhcp_message.yiaddr, 4);

    uint8_t length = 0;

    dhcp_get_option_data(dhcp_message, DHCP_OP_SUBNET_MASK, subnet_mask, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_ROUTER, gateway, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_NAME_SERVER, dns_server, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_SERVER_IDENTIFIER, dhcp_server_ip, &length);

    return true;
}

// Function to start the DORA process and get the ip and the gateway ip
bool process_dora(enc28j60_t* ethernet, uint8_t* static_ip, uint8_t* ip_router) {
    uint32_t current_time = furi_get_tick();

    xid = furi_hal_random_get();

    memcpy(MAC_ADDRESS, ethernet->mac_address, 6);

    uint8_t buffer[1500] = {0};
    uint16_t length = 0;

    state_dora_t state = DHCP_STATE_INIT;

    enable_broadcast(ethernet);

    while(state != DHCP_OK) {
        switch(state) {
        // This state is to send the discover message
        case DHCP_STATE_INIT:
            set_dhcp_discover_message(buffer, &length);
            send_packet(ethernet, buffer, length);
            memset(buffer, 0, 1500);
            state = DHCP_STATE_WAITING;
            break;

        // This state is to send the request message
        case DHCP_STATE_REQUEST:
            set_dhcp_request_message(buffer, &length);
            send_packet(ethernet, buffer, length);
            memset(buffer, 0, 1500);
            state = DHCP_STATE_WAITING;
            break;

        // It will waiting for a dhcp message, any of offer or
        case DHCP_STATE_WAITING:
            length = receive_packet(ethernet, buffer, 1500);

            // This part helps to know if it is dhcp offer
            if(is_dhcp(buffer)) {
                show_message(buffer, length); // To show the message

                if(deconstruct_dhcp_offer(buffer)) {
                    state = DHCP_STATE_REQUEST; // set the state in request
                    memset(buffer, 0, 1500);
                }

                // This part helps to know if it is dhcp acknowledge
                if(deconstruct_dhcp_ack(buffer)) {
                    state = DHCP_OK; // state ok
                }
            }
            break;

        // If the process fail it will stop and return a false
        case DHCP_FAIL:
            return false;
            break;

        default:
            break;
        }

        if(furi_get_tick() > (current_time + 10000)) {
            state = DHCP_FAIL;
        }
        furi_delay_ms(1);
    }

    disable_broadcast(ethernet);

    memcpy(static_ip, myip, 4);

    memcpy(ip_router, gateway, 4);

    return true;
}

// Function to copy the MAC DESTINATION in this case the MAC of the router
void get_mac_server(uint8_t* MAC_SERVER) {
    memcpy(MAC_SERVER, MAC_DESTINATION, 6);
}

// Function to get the gateway
void get_gateway_ip(uint8_t* ip_gateway) {
    memcpy(ip_gateway, gateway, 4);
}
