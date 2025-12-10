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

typedef enum {
    TCP_TN_FIN_ACK,
    TCP_TN_ACK,
} TCP_TERMINATION;

bool tcp_send_syn(
    enc28j60_t* ethernet,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number) {
    uint8_t* buffer = calloc(1, ETHERNET_HEADER_LEN + IP_HEADER_LEN + sizeof(tcp_header_t));

    uint16_t tcp_len;
    if(!set_tcp_header_syn(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           1,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(buffer + ETHERNET_HEADER_LEN, 6, tcp_len, source_ip, target_ip))
        return false;

    if(!set_ethernet_header(buffer, source_mac, target_mac, 0x800)) return false;

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

bool tcp_send_fin(
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
    if(!set_tcp_header_fin(
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

    printf("TCP FIN ENVIADO: ");
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
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_mac,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number) {
    if(ethernet == NULL || target_ip == NULL) return false;

    uint8_t* buffer = calloc(1, ETHERNET_HEADER_LEN + IP_HEADER_LEN + sizeof(tcp_header_t));

    uint16_t tcp_len;
    if(!set_tcp_header_ack(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           source_ip,
           target_ip,
           source_port,
           dest_port,
           sequence,
           ack_number,
           0xFFFF,
           0,
           &tcp_len))
        return false;

    if(!set_ipv4_header(buffer + ETHERNET_HEADER_LEN, 6, tcp_len, source_ip, target_ip))
        return false;

    if(!set_ethernet_header(buffer, source_mac, target_mac, 0x800)) return false;

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

    bool result = false;

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
               target_ip :
               app->ip_gateway,
           app->ethernet->mac_address,
           target_mac))
        return false;

    uint32_t sequence = 1;
    uint32_t ack_number = 0;

    uint32_t last_time = 0;
    uint8_t state = TCP_HS_SYN;
    switch(state) {
    case TCP_HS_SYN:
        if(tcp_send_syn(
               app->ethernet,
               app->ethernet->mac_address,
               app->ethernet->ip_address,
               target_mac,
               target_ip,
               source_port,
               dest_port,
               sequence,
               ack_number)) {
            // Get time
            last_time = furi_get_tick();

            state = TCP_HS_SYN_ACK;
        } else {
            result = false;
        }
    case TCP_HS_SYN_ACK:
        while(!(furi_get_tick() - last_time > 3000)) {
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

                        uint16_t data_offset_flags = 0;
                        bytes_to_uint(
                            &data_offset_flags, tcp_header.data_offset_flags, sizeof(uint16_t));
                        data_offset_flags &= 0x1FF;
                        if((data_offset_flags & (uint16_t)(TCP_SYN | TCP_ACK)) ==
                           (uint16_t)(TCP_SYN | TCP_ACK)) {
                            bytes_to_uint(&sequence, tcp_header.sequence, sizeof(uint32_t));
                            bytes_to_uint(&ack_number, tcp_header.ack_number, sizeof(uint32_t));

                            state = TCP_HS_ACK;
                            break;
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
                        } else if(
                            (data_offset_flags & (uint16_t)(TCP_RST | TCP_ACK)) ==
                            (uint16_t)(TCP_RST | TCP_ACK)) {
                            result = false;
                            break;

                        } else {
                            result = false;
                        }
                    }
                }
            }
        }
        if(state == TCP_HS_ACK) {
            if(tcp_send_ack(
                   app->ethernet,
                   app->ethernet->mac_address,
                   app->ethernet->ip_address,
                   target_mac,
                   target_ip,
                   source_port,
                   dest_port,
                   ack_number,
                   sequence + 1))
                result = true;
        }
    }
    return result;
}

bool tcp_handshake_process_spoof(
    void* context,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port) {
    App* app = context;

    bool result = false;

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
               target_ip :
               app->ip_gateway,
           app->ethernet->mac_address,
           target_mac))
        return false;

    uint32_t sequence = 1;
    uint32_t ack_number = 0;

    uint32_t last_time = 0;
    uint8_t state = TCP_HS_SYN;
    switch(state) {
    case TCP_HS_SYN:
        if(tcp_send_syn(
               app->ethernet,
               app->mac_gateway,
               app->ip_gateway,
               target_mac,
               target_ip,
               source_port,
               dest_port,
               sequence,
               ack_number)) {
            // Get time
            last_time = furi_get_tick();

            state = TCP_HS_SYN_ACK;
        } else {
            result = false;
        }
    case TCP_HS_SYN_ACK:
        while(!(furi_get_tick() - last_time > 3000)) {
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

                        uint16_t data_offset_flags = 0;
                        bytes_to_uint(
                            &data_offset_flags, tcp_header.data_offset_flags, sizeof(uint16_t));
                        data_offset_flags &= 0x1FF;
                        if((data_offset_flags & (uint16_t)(TCP_SYN | TCP_ACK)) ==
                           (uint16_t)(TCP_SYN | TCP_ACK)) {
                            bytes_to_uint(&sequence, tcp_header.sequence, sizeof(uint32_t));
                            bytes_to_uint(&ack_number, tcp_header.ack_number, sizeof(uint32_t));

                            state = TCP_HS_ACK;
                            break;
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
                        } else if(
                            (data_offset_flags & (uint16_t)(TCP_RST | TCP_ACK)) ==
                            (uint16_t)(TCP_RST | TCP_ACK)) {
                            result = false;
                            break;

                        } else {
                            result = false;
                        }
                    }
                }
            }
        }
        if(state == TCP_HS_ACK) {
            if(tcp_send_ack(
                   app->ethernet,
                   app->mac_gateway,
                   app->ip_gateway,
                   target_mac,
                   target_ip,
                   source_port,
                   dest_port,
                   ack_number,
                   sequence + 1))
                result = true;
        }
    }
    return result;
}

bool tcp_os_detector(void* context, uint8_t* target_ip, uint16_t source_port, uint16_t dest_port) {
    App* app = context;

    bool result = false;

    uint8_t target_mac[6] = {0};

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                   (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
               target_ip :
               app->ip_gateway,
           app->ethernet->mac_address,
           target_mac))
        return false;

    uint32_t sequence = 1;
    uint32_t ack_number = 0;

    uint32_t last_time = 0;
    uint8_t state = TCP_HS_SYN;
    switch(state) {
    case TCP_HS_SYN:
        if(tcp_send_syn(
               app->ethernet,
               app->mac_gateway,
               app->ip_gateway,
               target_mac,
               target_ip,
               source_port,
               dest_port,
               sequence,
               ack_number)) {
            // Get time
            last_time = furi_get_tick();

            state = TCP_HS_SYN_ACK;
        }
    case TCP_HS_SYN_ACK:
        while(!(furi_get_tick() - last_time > 3000)) {
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
                        result = true;
                        break;
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
                    } else {
                        result = false;
                    }
                }
            }
        }
    }
    return result;
}
