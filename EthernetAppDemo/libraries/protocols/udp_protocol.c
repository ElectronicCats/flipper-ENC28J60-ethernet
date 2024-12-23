#include "udp_protocol.h"
#include "protocol_tools/ethernet_protocol.h"
#include "protocol_tools/ipv4.h"
#include "protocol_tools/udp.h"

void udp_send_packet(udp_message_t message, uint8_t* payload, uint16_t payload_length) {
    uint8_t buffer[1520];

    memset(buffer, 0, sizeof(buffer));

    set_ethernet_header(buffer, message.mac_src, message.mac_dest, 0x0800);

    set_ipv4_header(
        buffer + sizeof(ethernet_header_t),
        0x11,
        payload_length + sizeof(udp_header_t),
        message.ip_origin,
        message.ip_destination);

    set_udp_header(
        buffer + sizeof(ethernet_header_t) + sizeof(ipv4_header_t),
        message.src_port,
        message.dest_port,
        payload_length + sizeof(udp_header_t));

    memcpy(
        buffer + sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t),
        payload,
        payload_length);

    uint16_t total_length =
        sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + payload_length;

    send_packet(message.ethernet, buffer, total_length);
}

void udp_set_enc_device(udp_message_t* message, enc28j60_t* enc_device) {
    message->ethernet = enc_device;
}

void udp_set_ip_address(udp_message_t* message, uint8_t* src_ip, uint8_t* dest_ip) {
    memcpy(message->ip_origin, src_ip, 4);
    memcpy(message->ip_destination, dest_ip, 4);
}

void udp_set_mac_address(udp_message_t* message, uint8_t* src_mac, uint8_t* dest_mac) {
    memcpy(message->mac_src, src_mac, 6);
    memcpy(message->mac_dest, dest_mac, 6);
}

void udp_set_port(udp_message_t* message, uint16_t src_port, uint16_t des_port) {
    message->dest_port = des_port;
    message->src_port = src_port;
}

bool udp_listen(udp_message_t* message) {
    memset(message, 0, sizeof(udp_message_t));
    uint8_t buffer[1520];
    uint16_t total_length = receive_packet(message->ethernet, buffer, 1520);

    if(total_length < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN)) return false;

    return true;
}
