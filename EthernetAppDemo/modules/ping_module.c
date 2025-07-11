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

// Función para crear packet de ping para Flipper Zero
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
    memcpy(ping_data, &timestamp, sizeof(timestamp));

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
    return PACKET_SIZE;
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
