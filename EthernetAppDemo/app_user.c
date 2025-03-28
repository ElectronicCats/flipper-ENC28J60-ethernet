#include "app_user.h"
// #include "furi_hal_bt.h"

#include "libraries/protocols/dhcp_protocol.h"

uint8_t MAC[6] = {0, 1, 2, 3, 4, 5};

int app_main(void* p) {
    UNUSED(p);

    uint8_t buffer[1500] = {0};

    uint16_t lenght = 0;

    set_dhcp_discover_message(buffer, &lenght);

    enc28j60_t* enc = enc28j60_alloc(MAC);
    enc28j60_start(enc);

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(is_link_up(enc)) {
            if(furi_hal_gpio_read(&gpio_button_ok)) {
                send_packet(enc, buffer, lenght);
                FURI_LOG_I("Flipper", "Sent");
                furi_delay_ms(200);
            }
        }
        furi_delay_ms(1);
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
