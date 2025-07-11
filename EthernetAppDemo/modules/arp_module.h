#ifndef ARP_HELPER_H_
#define ARP_HELPER_H_

#include <furi.h>
#include <furi_hal.h>
#include "../libraries/chip/enc28j60.h"

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
} arp_list;

// Function to set the message for the ARPspoofing
void set_arp_message_for_attack_all(
    uint8_t* buffer,
    uint8_t* MAC,
    uint8_t* ip_for_router,
    uint16_t* len);

// Function to attack the network with the ARP Spoofing to all
void send_arp_spoofing(enc28j60_t* ethernet, uint8_t* buffer, uint16_t len);

// Function to sget all the ip in the network
void arp_scan_network(
    enc28j60_t* ethernet,
    arp_list* list,
    uint8_t* own_mac,
    uint8_t* own_ip,
    uint8_t init_ip[4],
    uint8_t* list_count,
    uint8_t range);

// Function to get the MAC address
bool arp_get_specific_mac(enc28j60_t* ethernet, uint8_t* src_ip, uint8_t* dst_ip, uint8_t* mac_dst);

#endif
