#include "ethernet_protocol.h"

bool set_ethernet_header(
    ethernet_header_t* ethernet_header,
    uint8_t* mac_origin,
    uint8_t* mac_destination,
    uint8_t* type) {
    if(!ethernet_header || !mac_origin || !mac_destination || !type) {
        printf("Error: Puntero nulo detectado\n");
        return false;
    }

    memcpy(ethernet_header->mac_destination, mac_destination, 6);
    memcpy(ethernet_header->mac_source, mac_origin, 6);
    memcpy(ethernet_header->type, type, 2);

    return true;
}
