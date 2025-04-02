#include "app_user.h"

#include "libraries/dhcp_protocol.h"

#include "libraries/protocol_tools/arp.h"
#include "libraries/protocol_tools/ethernet_protocol.h"

uint8_t MAC[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d}; // BA:3F:91:C2:7E:5D
uint8_t MAC_ALL[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

uint8_t my_ip[4] = {0};
uint8_t router_ip[4] = {0};

uint8_t ip_to_all[4] = {0xff, 0xff, 0xff, 0xff};

void show_ip(uint8_t* data) {
    for(uint8_t i = 0; i < 4; i++) {
        printf("%u:", data[i]);
    }
    printf("\n");
}

void set_arp_message(uint8_t* buffer, uint16_t* len) {
    set_ethernet_header(buffer, MAC, MAC_ALL, 0x806);

    arp_set_header_ipv4(buffer + ETHERNET_HEADER_LEN, MAC, MAC_ALL, my_ip, ip_to_all, 0x0002);

    *len = ETHERNET_HEADER_LEN + ARP_LEN;
}

int app_main(void* p) {
    UNUSED(p);

    uint8_t buffer[1500] = {0};

    uint16_t lenght = 0;

    UNUSED(buffer);

    UNUSED(lenght);

    enc28j60_t* enc = enc28j60_alloc(MAC);
    enc28j60_start(enc);

    set_arp_message(buffer, &lenght);

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            // if(process_dora(enc, my_ip, router_ip)) {
            //     FURI_LOG_I("FLIPPER", "ALL OKAY");
            //     show_ip(my_ip);
            //     show_ip(router_ip);
            // } else {
            //     FURI_LOG_E("FLIPPER", "NOTHING");
            // }

            send_packet(enc, buffer, lenght);

            furi_delay_ms(200);
        }

        furi_delay_ms(1);
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
