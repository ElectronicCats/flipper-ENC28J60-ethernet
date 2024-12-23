#ifndef _UDP_PROTOCOL_H_
#define _UDP_PROTOCOL_H_

#include "chip/enc28j60.h"

typedef struct {
    enc28j60_t* ethernet;
    uint16_t src_port;
    uint16_t dest_port;
    uint8_t mac_src[6];
    uint8_t mac_dest[6];
    uint8_t ip_origin[4];
    uint8_t ip_destination[4];
} udp_message_t;

void udp_set_enc_device(udp_message_t* message, enc28j60_t* enc_device);

void udp_send_packet(udp_message_t message, uint8_t* payload, uint16_t payload_length);

void udp_set_ip_address(udp_message_t* message, uint8_t* src_ip, uint8_t* dest_ip);

void udp_set_mac_address(udp_message_t* message, uint8_t* src_mac, uint8_t* dest_mac);

void udp_set_port(udp_message_t* message, uint16_t src_port, uint16_t des_port);

bool udp_listen(udp_message_t* message, uint8_t* payload, uint16_t* payload_length);

#endif
