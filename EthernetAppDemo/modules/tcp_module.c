#include "tcp_module.h"

#include "app_user.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/arp.h"

#define DEBUG 0

typedef enum {
    TCP_HS_SYN,
    TCP_HS_SYN_ACK,
    TCP_HS_ACK,
} TCP_HANDSHAKE;

bool tcp_send_syn(
    enc28j60_t* ethernet,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint8_t* ip_gateway,
    uint32_t sequence,
    uint32_t ack_number) {
    uint8_t* buffer = calloc(1, ETHERNET_HEADER_LEN + IP_HEADER_LEN + sizeof(tcp_header_t));

    if(!arp_get_specific_mac(
           ethernet,
           ethernet->ip_address,
           (*(uint32_t*)ip_gateway & *(uint32_t*)ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)ethernet->subnet_mask) ?
               target_ip :
               ip_gateway,
           ethernet->mac_address,
           target_mac))
        return false;

    uint16_t tcp_len;
    if(!set_tcp_header_syn(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           ethernet->ip_address,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           0xFFFF,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(buffer + ETHERNET_HEADER_LEN, 6, tcp_len, ethernet->ip_address, target_ip))
        return false;

    if(!set_ethernet_header(buffer, ethernet->mac_address, target_mac, 0x800)) return false;

    send_packet(ethernet, buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

#if DEBUG

    printf("TCP SYN ENVIADO: ");
    for(uint16_t i = 0; i < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); i++) {
        printf(
            "%02X%c",
            buffer[i],
            i == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }

#endif

    free(buffer);

    return true;
}

bool tcp_send_ack(
    enc28j60_t* ethernet,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint8_t* ip_gateway,
    uint32_t sequence,
    uint32_t ack_number) {
    if(ethernet == NULL || target_ip == NULL || ip_gateway == NULL) return false;

    uint8_t* buffer = calloc(1, ETHERNET_HEADER_LEN + IP_HEADER_LEN + sizeof(tcp_header_t));

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           ethernet,
           ethernet->ip_address,
           (*(uint32_t*)ethernet->ip_address & *(uint32_t*)ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)ethernet->subnet_mask) ?
               target_ip :
               ip_gateway,
           ethernet->mac_address,
           target_mac))
        return false;

    uint16_t tcp_len;
    if(!set_tcp_header_ack(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           ethernet->ip_address,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           0xFFFF,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(buffer + ETHERNET_HEADER_LEN, 6, tcp_len, ethernet->ip_address, target_ip))
        return false;

    if(!set_ethernet_header(buffer, ethernet->mac_address, target_mac, 0x800)) return false;

    send_packet(ethernet, buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

#if DEBUG

    printf("TCP ACK ENVIADO: ");
    for(uint16_t i = 0; i < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); i++) {
        printf(
            "%02X%c",
            buffer[i],
            i == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }

#endif

    free(buffer);

    return true;
}

bool tcp_handshake_process(
    void* context,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port) {
    App* app = context;

    bool run = true;
    bool result = false;

    uint8_t target_mac[6] = {0};

    uint32_t sequence = 1;
    uint32_t ack_number = 0;

    uint32_t last_time;
    uint8_t state = TCP_HS_SYN;
    while(run) {
        switch(state) {
        case TCP_HS_SYN:
            if(tcp_send_syn(
                   app->ethernet,
                   target_mac,
                   target_ip,
                   source_port,
                   dest_port,
                   app->ip_gateway,
                   sequence,
                   ack_number)) {
                // Get time
                last_time = furi_get_tick();

                state = TCP_HS_SYN_ACK;
            }
            break;
        case TCP_HS_SYN_ACK:
            if(!(furi_get_tick() - last_time > 3000)) {
                uint16_t packen_len = 0;

                packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

                if(packen_len) {
                    if(is_arp(app->ethernet->rx_buffer)) {
                        arp_reply_requested(
                            app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                    } else if(is_tcp(app->ethernet->rx_buffer)) {
                        // Packet is for me
                        if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                            *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                           (*(uint32_t*)app->ethernet->mac_address ==
                            *(uint32_t*)app->ethernet->rx_buffer)) {
                            tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);

                            bytes_to_uint(&sequence, tcp_header.sequence, sizeof(uint32_t));
                            bytes_to_uint(&ack_number, tcp_header.ack_number, sizeof(uint32_t));

#if DEBUG

                            printf("SEQUENCE: %lu\n", sequence);
                            printf("ACK: %lu\n", ack_number);

                            printf("RECIBIDO: ");
                            for(uint16_t i = 0; i < packen_len; i++) {
                                printf(
                                    "%02X%c",
                                    app->ethernet->rx_buffer[i],
                                    i == (packen_len - 1) ? '\n' : ' ');
                            }

#endif

                            state = TCP_HS_ACK;
                        }
                    }
                }
            } else {
                run = false;
                break;
            }
            break;
        case TCP_HS_ACK:
            if(tcp_send_ack(
                   app->ethernet,
                   target_ip,
                   source_port,
                   dest_port,
                   app->ip_gateway,
                   ack_number,
                   sequence + 1)) {
                result = true;
                run = false;
            }
            break;
        }
    }
    return result;
}
