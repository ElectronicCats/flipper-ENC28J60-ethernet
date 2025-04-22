#include "tcp.h"
#include "ipv4.h"

// Helper function to convert 16-bit value to network byte order (big-endian)
static void uint16_to_bytes(uint16_t value, uint8_t* bytes) {
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
}

// Helper function to convert 32-bit value to network byte order (big-endian)
static void uint32_to_bytes(uint32_t value, uint8_t* bytes) {
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
}

bool set_tcp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint8_t flags,
    uint16_t window_size,
    uint16_t urgent_pointer) {
    if(buffer == NULL) return false;

    // Set source and destination ports
    uint16_to_bytes(source_port, buffer);
    uint16_to_bytes(dest_port, buffer + 2);

    // Set sequence and acknowledgment numbers
    uint32_to_bytes(sequence, buffer + 4);
    uint32_to_bytes(ack_number, buffer + 8);

    // Set data offset (5 = 20 bytes / 4) and reserved bits
    buffer[12] = (5 << 4); // 5 * 4 = 20 bytes header length

    // Set flags
    buffer[13] = flags;

    // Set window size
    uint16_to_bytes(window_size, buffer + 14);

    // Checksum will be calculated later
    buffer[16] = 0;
    buffer[17] = 0;

    // Set urgent pointer
    uint16_to_bytes(urgent_pointer, buffer + 18);

    return true;
}

tcp_header_t tcp_get_header(uint8_t* buffer) {
    tcp_header_t header = {0};

    // TCP header starts after Ethernet + IP headers
    uint8_t* tcp_start = buffer + 14 + IP_HEADER_LEN;

    // Copy header fields
    memcpy(&header, tcp_start, sizeof(tcp_header_t));

    return header;
}

bool is_tcp(uint8_t* buffer) {
    if(buffer == NULL) return false;

    // Check if it's an IPv4 packet first
    if(!is_ipv4(buffer)) return false;

    // Get IP header and check protocol field
    ipv4_header_t ip_header = ipv4_get_header(buffer);
    return ip_header.protocol == 6;
}

uint16_t calculate_tcp_checksum(
    uint8_t* tcp_segment,
    uint16_t tcp_length,
    uint8_t* src_ip,
    uint8_t* dst_ip) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)tcp_segment;

    // Add TCP pseudo header
    // Source IP
    sum += (src_ip[0] << 8) | src_ip[1];
    sum += (src_ip[2] << 8) | src_ip[3];

    // Destination IP
    sum += (dst_ip[0] << 8) | dst_ip[1];
    sum += (dst_ip[2] << 8) | dst_ip[3];

    // Protocol (TCP = 6) and TCP length
    sum += 6;
    sum += tcp_length;

    // Add TCP header and data
    for(uint16_t i = 0; i < tcp_length / 2; i++) {
        sum += ptr[i];
    }

    // If length is odd, add the last byte
    if(tcp_length % 2) {
        sum += tcp_segment[tcp_length - 1] << 8;
    }

    // Fold 32-bit sum to 16 bits
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}
