#include "app_user.h"

uint8_t MAC[] = {255, 121, 122, 255, 18, 0};

int app_main(void* p) {
    UNUSED(p);

    log_info("Program Starts");

    enc28j60_t* ethernet = enc28j60_alloc(MAC);

    enc28j60_start(ethernet);

    uint8_t buffer[500];

    memset(buffer, 0, sizeof(buffer));

    uint16_t len = 0;

    log_info("ENC connected: %u", is_link_up(ethernet));

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            //log_info("Pressed");
            len = receive_packet(ethernet, buffer, 500);
            if(len) {
                for(uint16_t i = 0; i < len; i++) {
                    log_info("P(%x): %x", i, buffer[i]);
                }
            } else {
                log_warning("Packet Not Received");
            }
            furi_delay_ms(200);
        }
        furi_delay_ms(1);
    }

    enc28j60_deinit(ethernet);
    free_enc28j60(ethernet);

    log_info("Program FINISHES");

    return 0;
}
