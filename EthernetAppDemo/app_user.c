#include "app_user.h"

uint8_t MAC[] = {255, 121, 122, 255, 18, 0};

uint8_t packet[60];

uint8_t mac_1[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t mac_2[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

uint16_t pos = 0;

void set_destination_mac() {
    for(uint8_t i = 0; i < 6; i++) {
        packet[pos++] = mac_1[i];
    }
}

void set_source_mac() {
    for(uint8_t i = 0; i < 6; i++) {
        packet[pos++] = mac_2[i];
    }
}

void set_data() {
    uint16_t data = 0;
    for(uint16_t counter = pos; counter < 60; counter++) {
        packet[pos++] = data++;
    }
}

int app_main(void* p) {
    UNUSED(p);

    log_info("Program Starts");

    enc28j60_t* ethernet = enc28j60_alloc(mac_2);

    enc28j60_start(ethernet);

    uint8_t buffer[500];

    memset(buffer, 0, sizeof(buffer));

    uint16_t len = 0;

    set_destination_mac();
    set_source_mac();
    set_data();

    log_info("ENC connected: %u", is_link_up(ethernet));

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            //log_info("Pressed");
            /*len = receive_packet(ethernet, buffer, 500);
            if(len) {
                for(uint16_t i = 0; i < len; i++) {
                    log_info("P(%x): %x", i, buffer[i]);
                }
            } else {
                log_warning("Packet Not Received");
            }*/

            log_info("Send Packet");

            send_packet(ethernet, packet, 60);

            furi_delay_ms(200);
        }

        len = receive_packet(ethernet, buffer, 500);

        if(len) {
            printf("-------------------------------------------------------------------------\n");
            for(uint16_t i = 0; i < len; i++) {
                printf("%x ", buffer[i]);
            }
            printf("\n");
        }

        furi_delay_ms(1);
    }

    enc28j60_deinit(ethernet);
    free_enc28j60(ethernet);

    log_info("Program FINISHES");

    return 0;
}
