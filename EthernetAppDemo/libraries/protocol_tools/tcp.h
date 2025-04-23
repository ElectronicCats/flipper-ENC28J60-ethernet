#ifndef _TCP_H_
#define _TCP_H_

#include <furi.h>
#include <furi_hal.h>

#define TCP_HEADER_LEN 20 // Minimum TCP header length (without options)

// TCP Flag definitions
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_ECE 0x40
#define TCP_CWR 0x80

typedef struct {
    uint8_t source_port[2]; // Source port (16 bits)
    uint8_t dest_port[2]; // Destination port (16 bits)
    uint8_t sequence[4]; // Sequence number (32 bits)
    uint8_t ack_number[4]; // Acknowledgment number (32 bits)
    uint8_t data_offset; // 4 bits data offset + 4 bits reserved
    uint8_t flags; // Control flags (6 bits) + 2 bits reserved
    uint8_t window_size[2]; // Window size (16 bits)
    uint8_t checksum[2]; // Checksum (16 bits)
    uint8_t urgent_pointer[2]; // Urgent pointer (16 bits)
    // Options can be added here if needed
} tcp_header_t;

// Set TCP header in buffer
bool set_tcp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint8_t flags,
    uint16_t window_size,
    uint16_t urgent_pointer);

// Get TCP header from buffer
tcp_header_t tcp_get_header(uint8_t* buffer);

// Check if the packet is TCP
bool is_tcp(uint8_t* buffer);

// Calculate TCP checksum
uint16_t calculate_tcp_checksum(
    uint8_t* tcp_segment,
    uint16_t tcp_length,
    uint8_t* src_ip,
    uint8_t* dst_ip);

// Create a complete TCP packet
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
    uint16_t payload_length);

#endif
