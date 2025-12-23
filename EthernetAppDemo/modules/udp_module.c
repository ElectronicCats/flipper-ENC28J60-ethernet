#include "udp_module.h"

#include "app_user.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/icmp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/protocol_tools/arp.h"

#define DEBUG 0

bool send_empty_udp_packet(
    uint8_t* buffer,
    enc28j60_t* ethernet,
    uint8_t* ip_gateway,
    uint8_t* source_mac,
    uint8_t* target_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t target_port) {
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

    if(!set_udp_header(
           buffer + sizeof(ethernet_header_t) + sizeof(ipv4_header_t),
           source_port,
           target_port,
           sizeof(udp_header_t)))
        return false;

    if(!set_ipv4_header(
           buffer + sizeof(ethernet_header_t),
           17,
           sizeof(udp_header_t),
           source_ip,
           target_ip,
           0,
           0x4000,
           WIN_TTL))
        return false;

    if(!set_ethernet_header(buffer, source_mac, target_mac, 0x0800)) return false;

    send_packet(
        ethernet,
        buffer,
        sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t));

#if DEBUG
    printf("EMPTY UDP PACKET: \n");
    for(uint16_t i = 0;
        i < (sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t));
        i++) {
        printf(
            "%02X%c",
            buffer[i],
            i == (sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) - 1) ?
                '\n' :
                ' ');
    }
#endif

    return true;
}

/*void udp_port_scan(
    void* context,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t init_port,
    uint16_t range_port) {
    App* app = context;

    enc28j60_t* ethernet = app->ethernet;

    uint8_t* target_mac[6] = {0};

    for(uint16_t i = init_port; i < (init_port + range_port); i++) {
    }
}*/

bool udp_check_port(
    void* context,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t target_port) {
    App* app = context;
    enc28j60_t* ethernet = app->ethernet;

    bool result = false;
    bool run = true;

    uint8_t* buffer =
        calloc(1, sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t));

    uint8_t target_mac[6] = {0};

    if(!send_empty_udp_packet(
           buffer,
           ethernet,
           app->ip_gateway,
           source_mac,
           target_mac,
           source_ip,
           target_ip,
           source_port,
           target_port)) {
        run = false;
        result = false;
    }

    uint16_t packen_len = 0;
    uint32_t last_time = furi_get_tick();
    while(run) {
        if((furi_get_tick() - last_time > 3000)) break;
        packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);
        if(packen_len) {
            if(is_arp(app->ethernet->rx_buffer)) {
                arp_reply_requested(
                    app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
            } else if(is_udp(app->ethernet->rx_buffer)) {
                // Packet is for me
                if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                    *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                   (*(uint32_t*)app->ethernet->mac_address ==
                    *(uint32_t*)app->ethernet->rx_buffer)) {
                    result = true;
                    run = false;
#if DEBUG
                    printf("UDP PACKET RECIVED: ");
                    for(uint16_t i = 0; i < packen_len; i++) {
                        printf(
                            "%02X%c",
                            app->ethernet->rx_buffer[i],
                            i == (packen_len - 1) ? '\n' : ' ');
                    }
#endif
                }
            } else if(is_icmp(app->ethernet->rx_buffer)) {
                icmp_header_t icmp_header = icmp_get_header(app->ethernet->rx_buffer);
                if((icmp_header.type == ICMP_TYPE_DEST_UNREACHABLE) &&
                   (icmp_header.code == ICMP_CODE_PORT_UNREACHABLE)) {
                    result = false;
                    run = false;
                }
            }
        }
    }

    free(buffer);

    return result;
}
