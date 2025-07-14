#include "ping_module.h"

#include "../libraries/protocol_tools/icmp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"

#define PING_DATA_SIZE 64 // 64 is the minimum size of the ICMP message (8 bytes)
#define PACKET_SIZE                                         \
    (PING_DATA_SIZE + ETHERNET_HEADER_LEN + IP_HEADER_LEN + \
     ICMP_HEADER_LEN) // Real size of the packet

// Función para obtener timestamp (simulado para Flipper Zero)
uint32_t get_flipper_timestamp() {
    return furi_get_tick();
}

//  Function to compare IP
bool compare_ip(uint8_t* ip_a, uint8_t* ip_b) {
    for(uint8_t i = 0; i < 4; i++) {
        if(ip_a[i] != ip_b[i]) return false;
    }
    return true;
}

// Function to create a ping packet
uint16_t create_flipper_ping_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* payload,
    uint16_t payload_len) {
    // Check if there is not a NULL pointer
    if(!buffer || !src_mac || !src_ip || !dst_ip || !payload) return 0;

    // Check the size of payload
    if(payload_len > 56) {
        return 0;
    }

    // Pingpacket position
    uint8_t position = 0;

    // Need to set the time of the timestamp
    uint8_t ping_data[PING_DATA_SIZE] = {0};
    uint32_t timestamp = get_flipper_timestamp();

    // Set the format on big eldian
    ping_data[0] = (uint8_t)timestamp >> 24 & 0xff;
    ping_data[1] = (uint8_t)timestamp >> 16 & 0xff;
    ping_data[2] = (uint8_t)timestamp >> 8 & 0xff;
    ping_data[3] = (uint8_t)timestamp & 0xff;

    // Add the position
    position += sizeof(timestamp);

    // add padding for timestamp
    memset(ping_data + position, 0, 4);

    // move position
    position += 4;

    // add the payload
    memcpy(ping_data + position, payload, payload_len);

    // add position
    position += payload_len;

    // Add padding
    if(position < 64) {
        for(uint8_t i = position; i < 64; i++) {
            ping_data[i] = i - position;
        }
    }

    printf("Ping message \n");

    for(uint8_t i = 0; i < 64; i++) {
        printf("%02x ", ping_data[i]);
    }

    printf("\n");

    // set Ethernet header
    if(!set_ethernet_header(buffer, src_mac, dst_mac, 0x0800)) {
        return 0;
    }

    // Set IPv4 header
    if(!set_ipv4_header(
           buffer + ETHERNET_HEADER_LEN,
           1, // Protocolo ICMP
           ICMP_HEADER_LEN + PING_DATA_SIZE,
           src_ip,
           dst_ip)) {
        return 0;
    }

    // Set the header ICMP
    if(!icmp_set_header(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           ICMP_TYPE_ECHO_REQUEST,
           0, // code
           identifier,
           sequence,
           ping_data,
           PING_DATA_SIZE)) {
        return 0;
    }

    // Set the payload
    memcpy(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN, ping_data, PING_DATA_SIZE);

    return PACKET_SIZE;
}

// Function to get the ping packet
bool is_the_ping_packet(uint8_t* packet, uint8_t* ip_ping) {
    if(!packet || !ip_ping) return false;

    if(!is_icmp(packet)) return false;

    icmp_header_t icmp_header = icmp_get_header(packet);

    if(icmp_header.type != ICMP_TYPE_ECHO_REPLY) return false;

    ipv4_header_t ipv4_header = ipv4_get_header(packet);

    uint8_t* ip_des = ipv4_header.source_ip;

    return compare_ip(ip_ping, ip_des);
}

// Process to send and get the ping packets
// This only reads if the packet comes from the IP indicated
bool process_ping_response(
    enc28j60_t* ethernet,
    uint8_t* ping_packet,
    uint16_t ping_packet_size,
    uint8_t* ip_dest) {
    if(!ethernet || !ping_packet || !ip_dest) return false;

    // Value to return
    bool ret = false;

    // Send the ping packet
    send_packet(ethernet, ping_packet, ping_packet_size);

    printf("Message to send\n");

    for(uint16_t i = 0; i < ping_packet_size; i++) {
        printf("%02x ", ping_packet[i]);
    }

    printf("\n");

    // Time to get the last time
    uint32_t last_time = furi_get_tick();

    // Generate packet to received
    uint8_t packet_to_received[250] = {0}; // 250 bytes, the message is not to long

    // Set in promiscous mode
    enable_promiscuous(ethernet);
    while(!ret && ((furi_get_tick() - last_time) < 1000)) {
        if(receive_packet(ethernet, packet_to_received, 250)) {
            ret = is_the_ping_packet(packet_to_received, ip_dest);
        }
        furi_delay_ms(1);
    }
    disable_promiscuous(ethernet);

    return ret;
}

// bool process_ping_response(uint8_t* buffer, uint16_t packet_size, uint8_t* ip_dest) {
//     // Verificar que es un packet ICMP
//     if(!is_icmp(buffer)) {
//         printf("Packet recibido no es ICMP\n");
//         return false;
//     }

//     ipv4_header_t ip_header = ipv4_get_header(buffer);
//     icmp_header_t icmp_header = icmp_get_header(buffer);

//     // Verificar que es Echo Reply
//     if(icmp_header.type != ICMP_TYPE_ECHO_REPLY) {
//         printf("Packet ICMP no es Echo Reply (tipo: %d)\n", icmp_header.type);
//         return false;
//     }

//     // Extraer datos del ping
//     uint8_t* ping_data = buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN;
//     uint32_t sent_timestamp;
//     memcpy(&sent_timestamp, ping_data, sizeof(sent_timestamp));

//     // Calcular tiempo de respuesta
//     uint32_t current_time = get_flipper_timestamp();
//     double_t rtt = (current_time - sent_timestamp) / 1000.0; // en milisegundos

//     // Extraer información
//     uint16_t identifier = (icmp_header.identifier[0] << 8) | icmp_header.identifier[1];
//     uint16_t sequence = (icmp_header.sequence[0] << 8) | icmp_header.sequence[1];

//     // Mostrar información de la respuesta
//     printf(
//         "Reply from %d.%d.%d.%d: bytes=%d time=%.2fms TTL=%d seq=%d id=%d\n",
//         ip_header.source_ip[0],
//         ip_header.source_ip[1],
//         ip_header.source_ip[2],
//         ip_header.source_ip[3],
//         packet_size - ETHERNET_HEADER_LEN - IP_HEADER_LEN - ICMP_HEADER_LEN,
//         rtt,
//         ip_header.ttl,
//         sequence,
//         identifier);

//     return true;
// }
