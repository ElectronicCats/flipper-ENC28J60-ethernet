#include "tcp.h"
#include "ipv4.h"
#include "ethernet_protocol.h"

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
    uint8_t* tcp_start = buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN;

    // Copy source and destination ports
    memcpy(header.source_port, tcp_start, 2);
    memcpy(header.dest_port, tcp_start + 2, 2);

    // Copy sequence and acknowledgment numbers
    memcpy(header.sequence, tcp_start + 4, 4);
    memcpy(header.ack_number, tcp_start + 8, 4);

    // Copy data offset and flags
    header.data_offset = tcp_start[12];
    header.flags = tcp_start[13];

    // Copy window size
    memcpy(header.window_size, tcp_start + 14, 2);

    // Copy checksum
    memcpy(header.checksum, tcp_start + 16, 2);

    // Copy urgent pointer
    memcpy(header.urgent_pointer, tcp_start + 18, 2);

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
    // Create pseudo-header for checksum calculation
    uint8_t pseudo_header[12];

    // Source IP address (4 bytes)
    memcpy(pseudo_header, src_ip, 4);

    // Destination IP address (4 bytes)
    memcpy(pseudo_header + 4, dst_ip, 4);

    // Reserved byte (always 0)
    pseudo_header[8] = 0;

    // Protocol (TCP = 6)
    pseudo_header[9] = 6;

    // TCP segment length (16 bits)
    pseudo_header[10] = (tcp_length >> 8) & 0xFF;
    pseudo_header[11] = tcp_length & 0xFF;

    // Calculate the sum of pseudo-header
    uint32_t sum = 0;
    for(int i = 0; i < 12; i += 2) {
        sum += (pseudo_header[i] << 8) + pseudo_header[i + 1];
    }

    // Add the TCP segment to the sum
    uint16_t* ptr = (uint16_t*)tcp_segment;
    for(int i = 0; i < tcp_length / 2; i++) {
        sum += ptr[i];
    }

    // If length is odd, add the last byte padded with zero
    if(tcp_length % 2) {
        sum += tcp_segment[tcp_length - 1] << 8;
    }

    // Fold 32-bit sum to 16 bits
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // One's complement
    return ~sum;
}

// New function to create a complete TCP packet
bool create_tcp_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t src_port,
    uint16_t dst_port,
    uint32_t seq_num,
    uint32_t ack_num,
    uint8_t flags,
    uint16_t window_size,
    uint8_t* payload,
    uint16_t payload_length) {
    if(!buffer || !src_mac || !dst_mac || !src_ip || !dst_ip) {
        return false;
    }

    // Set Ethernet header
    if(!set_ethernet_header(buffer, src_mac, dst_mac, 0x0800)) { // 0x0800 is IPv4
        return false;
    }

    // Set IP header
    uint8_t* ip_header_ptr = buffer + ETHERNET_HEADER_LEN;
    uint16_t total_tcp_length = TCP_HEADER_LEN + payload_length;

    if(!set_ipv4_header(ip_header_ptr, 6, total_tcp_length, src_ip, dst_ip)) { // 6 is TCP
        return false;
    }

    // Set TCP header
    uint8_t* tcp_header_ptr = buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN;

    if(!set_tcp_header(
           tcp_header_ptr, src_port, dst_port, seq_num, ack_num, flags, window_size, 0)) {
        return false;
    }

    // Copy payload data if any
    if(payload_length > 0 && payload != NULL) {
        memcpy(tcp_header_ptr + TCP_HEADER_LEN, payload, payload_length);
    }

    // Calculate and set TCP checksum
    uint16_t checksum = calculate_tcp_checksum(tcp_header_ptr, total_tcp_length, src_ip, dst_ip);
    tcp_header_ptr[16] = (checksum >> 8) & 0xFF;
    tcp_header_ptr[17] = checksum & 0xFF;

    return true;
}
