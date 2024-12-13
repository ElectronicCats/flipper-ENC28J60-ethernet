#include "app_user.h"

uint8_t MAC[] = {255, 255, 255, 255, 255, 255};

int app_main(void* p) {
    UNUSED(p);

    enc28j60_t* ethernet = enc28j60_alloc(MAC);

    free_enc28j60(ethernet);

    return 0;
}
