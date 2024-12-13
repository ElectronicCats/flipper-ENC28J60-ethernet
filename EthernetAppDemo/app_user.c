#include "app_user.h"

uint8_t MAC[] = {255, 121, 122, 255, 18, 0};

int app_main(void* p) {
    UNUSED(p);

    log_info("Program Starts");

    enc28j60_t* ethernet = enc28j60_alloc(MAC);

    enc28j60_start(ethernet);

    log_info("ENC connected: %u", is_link_up(ethernet));

    free_enc28j60(ethernet);

    log_info("Program FINISHES");

    return 0;
}
