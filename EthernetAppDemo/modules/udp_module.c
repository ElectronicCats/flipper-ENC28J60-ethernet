#include "udp_module.h"

#include "app_user.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/icmp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/protocol_tools/arp.h"

#define TEXT_PORT_FORMAT "%lu%s"
#define TEXT_POINTS      "..."

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

void udp_port_scan(void* context, uint8_t* target_ip, uint16_t init_port, uint16_t range_port) {
    App* app = context;

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
        return;

    udp_header_t udp_header;

    uint32_t submenu_index = 0;

    submenu_add_item(app->submenu, "", submenu_index, NULL, NULL);

    for(uint32_t i = init_port; i < (init_port + range_port); i++) {
        if(!furi_hal_gpio_read(&gpio_button_back)) break;
        furi_string_reset(app->text);
        furi_string_cat_printf(app->text, TEXT_PORT_FORMAT, i, TEXT_POINTS);
        submenu_change_item_label(app->submenu, submenu_index, furi_string_get_cstr(app->text));

        send_empty_udp_packet(
            app->ethernet->tx_buffer,
            app->ethernet,
            app->ip_gateway,
            app->ethernet->mac_address,
            target_mac,
            app->ethernet->ip_address,
            target_ip,
            64892,
            i);

        uint32_t last_time = furi_get_tick();

        while(!(furi_get_tick() - last_time > 100)) {
            uint16_t packen_len = 0;

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
                        udp_header = udp_get_header(app->ethernet->rx_buffer);

                        uint16_t source_port;
                        uint16_t target_port;
                        bytes_to_uint(&source_port, udp_header.source_port, sizeof(uint16_t));
                        bytes_to_uint(&target_port, udp_header.dest_port, sizeof(uint16_t));

                        if(source_port == i && target_port == 64892) {
                            furi_string_reset(app->text);
                            furi_string_cat_printf(
                                app->text, TEXT_PORT_FORMAT, (uint32_t)source_port, "\0");
                            submenu_change_item_label(
                                app->submenu, submenu_index, furi_string_get_cstr(app->text));
                            //submenu_set_selected_item(app->submenu, submenu_index);
                            submenu_index++;
                            furi_string_reset(app->text);
                            furi_string_cat_printf(app->text, TEXT_PORT_FORMAT, i, TEXT_POINTS);
                            submenu_add_item(
                                app->submenu,
                                furi_string_get_cstr(app->text),
                                submenu_index,
                                NULL,
                                NULL);
                            submenu_set_selected_item(app->submenu, submenu_index);
                            break;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
    furi_string_reset(app->text);
    submenu_change_item_label(app->submenu, submenu_index, "FINISH");
}
