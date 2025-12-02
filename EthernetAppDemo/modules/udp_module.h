#ifndef UDP_MODULE_H_
#define UDP_MODULE_H_

// Note: This module will be in development
#include <furi.h>

bool udp_check_port(
    void* context,
    uint8_t* source_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t target_port);

#endif
