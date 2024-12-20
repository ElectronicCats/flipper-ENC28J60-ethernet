#include "ethernet_protocol.h"

#define alloc_ethernet(MAC) enc28j60_alloc(MAC)
#define init_enc(enc)       enc28j60_start(enc)

#define deinit_enc(enc) enc28j60_deinit(enc)
#define free_enc(enc)   free_enc28j60(enc)

ethernet_t* ethernet_init(uint8_t* MAC) {
    ethernet_t* ethernet = enc28j60_alloc(MAC);
    init_enc(ethernet);
    return ethernet;
}

void ethernet_deinit(ethernet_t* ethernet) {
    deinit_enc(ethernet);
    free_enc(ethernet);
}

bool set_ethernet_header(
    ethernet_header_t* ethernet_header,
    uint8_t* mac_origin,
    uint8_t* mac_destiny,
    uint8_t* type) {
    if(!ethernet_header || !mac_origin || !mac_destiny || !type) {
        printf("Error: Puntero nulo detectado\n");
        return false;
    }

    memcpy(ethernet_header->mac_destiny, mac_destiny, 6);
    memcpy(ethernet_header->mac_source, mac_origin, 6);
    memcpy(ethernet_header->type, type, 2);

    return true;
}
