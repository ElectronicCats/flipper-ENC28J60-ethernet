#include "app_user.h"
#include "tcp.h"
#include "ipv4.h"
#include "ethernet_protocol.h"

// New function to create a complete TCP packet
bool create_tcp_header(
    tcp_header_t* tcp_header,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t flags,
    uint16_t window_size,
    uint16_t urgent_pointer) {
    if(tcp_header == NULL) return false;

    // Set source and destination ports
    uint_to_bytes(&source_port, tcp_header->source_port, sizeof(uint16_t));
    uint_to_bytes(&dest_port, tcp_header->dest_port, sizeof(uint16_t));

    // Set sequence and acknowledgment numbers
    uint_to_bytes(&sequence, tcp_header->sequence, sizeof(uint32_t));
    uint_to_bytes(&ack_number, tcp_header->ack_number, sizeof(uint32_t));

    // Set data offset (5 = 20 bytes / 4) and reserved bits
    tcp_header->data_offset_flags[0] = (5 << 4); // 5 * 4 = 20 bytes header length

    // Set flags
    tcp_header->data_offset_flags[0] |= ((uint8_t*)&flags)[1] & 0x01;
    tcp_header->data_offset_flags[1] = ((uint8_t*)&flags)[0];

    // Set window size
    uint_to_bytes(&window_size, tcp_header->window_size, sizeof(uint16_t));

    // Checksum will be calculated later
    tcp_header->checksum[0] = 0;
    tcp_header->checksum[1] = 0;

    // Set urgent pointer
    uint_to_bytes(&urgent_pointer, tcp_header->urgent_pointer, sizeof(uint16_t));

    return true;
}

void calculate_checksum_tcp(
    uint16_t options_size,
    pseudo_header_ip_t* pseudo_header,
    tcp_header_t* tcp_header) {
    uint8_t* buffer_checksum =
        calloc(1, sizeof(pseudo_header_ip_t) + TCP_HEADER_LEN + options_size);

    pseudo_header->tcp_lenght[0] = (options_size + TCP_HEADER_LEN) >> 8;
    pseudo_header->tcp_lenght[1] = options_size + TCP_HEADER_LEN;

    memcpy(buffer_checksum, pseudo_header, sizeof(pseudo_header_ip_t));
    memcpy(
        buffer_checksum + sizeof(pseudo_header_ip_t), tcp_header, TCP_HEADER_LEN + options_size);

    uint16_t cheksum = calculate_checksum_ipv4(
        (uint16_t*)buffer_checksum,
        (sizeof(pseudo_header_ip_t) + TCP_HEADER_LEN + options_size) / 2);

    tcp_header->checksum[0] = ((uint8_t*)&cheksum)[1];
    tcp_header->checksum[1] = ((uint8_t*)&cheksum)[0];

    free(buffer_checksum);
}

bool set_tcp_header_syn(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* len) {
    if(buffer == NULL || source_ip == NULL || target_ip == NULL) return false;

    pseudo_header_ip_t* pseudo_header = calloc(1, sizeof(pseudo_header_ip_t));
    memcpy(pseudo_header->source_ip, source_ip, 4);
    memcpy(pseudo_header->dest_ip, target_ip, 4);
    pseudo_header->protocol = 0x06;

    tcp_header_t* tcp_header = calloc(1, sizeof(tcp_header_t));

    create_tcp_header(
        tcp_header,
        source_port,
        dest_port,
        sequence,
        ack_number,
        TCP_SYN,
        window_size,
        urgent_pointer);

    // Crear la funcion para armar el paquete SYN
    uint16_t options_size = 0;

    // This is the array with the values to send the request
    uint8_t first_option[] = {TCP_MSS, 0x04, 0x05, 0xB4, 0x01};
    memcpy(tcp_header->options + options_size, first_option, sizeof(first_option));
    options_size += sizeof(first_option);

    uint8_t second_option[] = {TCP_WS, 0x03, 0x08, 0x01, 0x01};
    memcpy(tcp_header->options + options_size, second_option, sizeof(second_option));
    options_size += sizeof(second_option);

    uint8_t tree_option[] = {TCP_SACK_P, 0x02};
    memcpy(tcp_header->options + options_size, tree_option, sizeof(tree_option));
    options_size += sizeof(tree_option);

    tcp_header->data_offset_flags[0] =
        ((tcp_header->data_offset_flags[0] >> 4) + (options_size / 4)) << 4;

    calculate_checksum_tcp(options_size, pseudo_header, tcp_header);

    memcpy(buffer, tcp_header, TCP_HEADER_LEN + options_size);

    free(tcp_header);

    *len = TCP_HEADER_LEN + options_size;

    return true;
}

bool set_tcp_header_fin(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* len) {
    if(buffer == NULL) return false;

    pseudo_header_ip_t* pseudo_header = calloc(1, sizeof(pseudo_header_ip_t));
    memcpy(pseudo_header->source_ip, source_ip, 4);
    memcpy(pseudo_header->dest_ip, target_ip, 4);
    pseudo_header->protocol = 0x06;

    tcp_header_t* tcp_header = calloc(1, sizeof(tcp_header_t));

    if(!create_tcp_header(
           tcp_header,
           source_port,
           dest_port,
           sequence,
           ack_number,
           TCP_FIN,
           window_size,
           urgent_pointer))
        return false;

    calculate_checksum_tcp(0, pseudo_header, tcp_header);

    memcpy(buffer, tcp_header, TCP_HEADER_LEN);

    free(tcp_header);

    *len = TCP_HEADER_LEN;

    return true;
}

bool set_tcp_header_ack(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* len) {
    if(buffer == NULL) return false;

    pseudo_header_ip_t* pseudo_header = calloc(1, sizeof(pseudo_header_ip_t));
    memcpy(pseudo_header->source_ip, source_ip, 4);
    memcpy(pseudo_header->dest_ip, target_ip, 4);
    pseudo_header->protocol = 0x06;

    tcp_header_t* tcp_header = calloc(1, sizeof(tcp_header_t));

    if(!create_tcp_header(
           tcp_header,
           source_port,
           dest_port,
           sequence,
           ack_number,
           TCP_ACK,
           window_size,
           urgent_pointer))
        return false;

    calculate_checksum_tcp(0, pseudo_header, tcp_header);

    memcpy(buffer, tcp_header, TCP_HEADER_LEN);

    free(tcp_header);

    *len = TCP_HEADER_LEN;

    return true;
}

tcp_header_t tcp_get_header(uint8_t* buffer) {
    tcp_header_t header = {0};

    memcpy(&header, buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, sizeof(tcp_header_t) - 50);

    uint8_t data_offset = (header.data_offset_flags[0] >> 4) * 4;

    memcpy(&header, buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, data_offset);

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
