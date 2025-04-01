#include "app_user.h"

#include "libraries/protocols/dhcp_protocol.h"

uint8_t MAC[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d}; // BA:3F:91:C2:7E:5D

uint8_t my_ip[4] = {0};
uint8_t router_ip[4] = {0};

int app_main(void* p) {
    UNUSED(p);

    uint8_t buffer[1500] = {0};

    uint16_t lenght = 0;

    UNUSED(buffer);

    UNUSED(lenght);

    enc28j60_t* enc = enc28j60_alloc(MAC);
    enc28j60_start(enc);

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(is_link_up(enc)) {
            if(furi_hal_gpio_read(&gpio_button_ok)) {
                if(process_dora(enc, my_ip, router_ip)) {
                    FURI_LOG_I("FLIPPER", "ALL OKAY");
                } else {
                    FURI_LOG_E("FLIPPER", "NOTHING");
                }
            }
        }
        furi_delay_ms(1);
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
