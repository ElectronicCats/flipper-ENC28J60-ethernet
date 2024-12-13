#ifndef _ENC28J60_LIBRARY_
#define _ENC28J60_LIBRARY_

#include <furi.h>
#include <furi_hal.h>

#include "Spi_lib.h"
#include "log_user.h"

#define MAX_FRAMELEN 1500

#define RXSTART_INIT 0x0000
#define RXSTOP_INIT  0x0BFF

#define TXSTART_INIT 0x0C00
#define TXSTOP_INIT  0x11FF

typedef struct {
    FuriHalSpiBusHandle* spi;
    uint8_t* mac_address;
} enc28j60_t;

enc28j60_t* enc28j60_alloc(uint8_t* mac_address);
void enc28j60_deinit(enc28j60_t* instance);
void free_enc28j60(enc28j60_t* instance);
uint8_t enc28j60_start(enc28j60_t* instance);

#endif
