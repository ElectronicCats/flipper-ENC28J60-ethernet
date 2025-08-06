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

/**
 * @brief Populates a TCP header in a given buffer.
 *
 * This function constructs a TCP header within the provided buffer,
 * setting all the necessary fields such as source and destination ports,
 * sequence and acknowledgment numbers, flags, window size, and urgent pointer.
 * The checksum field is typically calculated and set separately.
 *
 * @param buffer A pointer to the buffer where the TCP header will be written.
 * @param source_port The 16-bit source port number.
 * @param dest_port The 16-bit destination port number.
 * @param sequence The 32-bit sequence number for the segment.
 * @param ack_number The 32-bit acknowledgment number.
 * @param flags The 8-bit TCP flags (e.g., SYN, ACK, FIN, PSH, RST, URG).
 * @param window_size The 16-bit window size, indicating the receive window.
 * @param urgent_pointer The 16-bit urgent pointer, used with the URG flag.
 * @return `true` if the header was successfully set, `false` otherwise.
 */
bool set_tcp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint8_t flags,
    uint16_t window_size,
    uint16_t urgent_pointer);

/**
 * @brief Parses a network packet and extracts its TCP header.
 *
 * This function reads the relevant bytes from a network packet buffer
 * (assuming it points to the start of the TCP segment) and populates a
 * `tcp_header_t` structure with the corresponding TCP header information.
 *
 * @param buffer A pointer to the buffer containing the TCP segment.
 * @return A `tcp_header_t` structure containing the parsed TCP header information.
 */
tcp_header_t tcp_get_header(uint8_t* buffer);

/**
 * @brief Checks if a network packet is a TCP packet.
 *
 * This function typically inspects the Protocol field of the IPv4 header
 * to determine if the payload is a TCP packet. The protocol number for TCP is `6`.
 *
 * @param buffer A pointer to the buffer containing the start of the IPv4 packet.
 * @return `true` if the packet's payload is TCP, `false` otherwise.
 */
bool is_tcp(uint8_t* buffer);

/**
 * @brief Calculates the TCP checksum, including the pseudo-header.
 *
 * This function computes the 16-bit one's complement checksum for a
 * TCP segment. This calculation includes a pseudo-header (derived from the
 * IPv4 source and destination addresses, protocol, and TCP length), the TCP
 * header itself, and the TCP payload.
 *
 * @param tcp_segment A pointer to the start of the TCP header.
 * @param tcp_length The total length of the TCP segment (header + payload) in bytes.
 * @param src_ip A pointer to the 4-byte source IP address (from the IPv4 header).
 * @param dst_ip A pointer to the 4-byte destination IP address (from the IPv4 header).
 * @return The calculated 16-bit TCP checksum value.
 */
uint16_t calculate_tcp_checksum(
    uint8_t* tcp_segment,
    uint16_t tcp_length,
    uint8_t* src_ip,
    uint8_t* dst_ip);

/**
 * @brief Creates a complete TCP packet (Ethernet + IPv4 + TCP + Payload).
 *
 * This comprehensive function assembles a full TCP packet by
 * sequentially building the Ethernet, IPv4, and TCP headers, and then
 * appending the provided payload. It also handles the calculation of
 * the IPv4 and TCP checksums.
 *
 * @param buffer A pointer to the buffer where the complete packet will be assembled.
 * @param src_mac A pointer to the 6-byte source MAC address.
 * @param dst_mac A pointer to the 6-byte destination MAC address.
 * @param src_ip A pointer to the 4-byte source IP address.
 * @param dst_ip A pointer to the 4-byte destination IP address.
 * @param src_port The 16-bit source TCP port.
 * @param dst_port The 16-bit destination TCP port.
 * @param seq_num The 32-bit TCP sequence number.
 * @param ack_num The 32-bit TCP acknowledgment number.
 * @param flags The 8-bit TCP flags.
 * @param window_size The 16-bit TCP window size.
 * @param payload A pointer to the application layer data (payload).
 * @param payload_length The length of the payload in bytes.
 * @return `true` if the complete TCP packet was successfully created, `false` otherwise.
 */
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
