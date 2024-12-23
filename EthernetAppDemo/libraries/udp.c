#include "udp.h"

#include "ipv4.h"

typedef struct {
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint8_t zeros;
    uint8_t protocol;
    uint8_t length[2];
} ip_pseudo_header_t;

uint16_t udp_calculate_checksum(
    ipv4_header_t* ip_header,
    udp_header_t* udp_header,
    uint8_t* data,
    uint16_t len) {
    uint16_t total_len = sizeof(udp_header_t) + len;
    uint8_t buffer[1518]; // Temporary buffer
    ip_pseudo_header_t* pseudo_header = (ip_pseudo_header_t*)buffer;

    // Create pseudo header
    memcpy(pseudo_header->src_ip, ip_header->source_ip, 4);
    memcpy(pseudo_header->dst_ip, ip_header->dest_ip, 4);
    pseudo_header->zeros = 0;
    pseudo_header->protocol = 17; // UDP
    pseudo_header->length[0] = total_len >> 8;
    pseudo_header->length[1] = (uint8_t)total_len;

    // Copy UDP header and data
    memcpy(buffer + sizeof(ip_pseudo_header_t), udp_header, sizeof(udp_header_t));
    memcpy(buffer + sizeof(ip_pseudo_header_t) + sizeof(udp_header_t), data, len);

    // If length is odd, pad with zero
    if(len % 2) {
        buffer[sizeof(ip_pseudo_header_t) + sizeof(udp_header_t) + len] = 0;
        total_len++;
    }

    return calculate_checksum(buffer, sizeof(ip_pseudo_header_t) + total_len);
}

bool set_udp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t destination_port,
    uint16_t length) {
    if(!buffer) return false;

    udp_header_t* header = (udp_header_t*)buffer;

    header->dest_port[0] = (destination_port >> 8) & 0xff;
    header->dest_port[1] = destination_port & 0xff;

    header->source_port[0] = (source_port >> 8) & 0xff;
    header->source_port[1] = source_port & 0xff;

    // Set lenght of the Message
    header->length[0] = (length >> 8) & 0xff;
    header->length[1] = length & 0xff;

    // Checksum set in zeros (in IPV4 is optional)
    header->checksum[0] = 0;
    header->checksum[1] = 0;

    return true;
}
