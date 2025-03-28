#include "app_user.h"
#include "furi_hal_bt.h"

uint8_t MAC[6] = {0, 1, 2, 3, 4, 5};

int app_main(void* p) {
    UNUSED(p);

    enc28j60_t* enc = enc28j60_alloc(MAC);
    enc28j60_start(enc);

    while(furi_hal_gpio_read(&gpio_button_back)) {
        furi_delay_ms(1);
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
