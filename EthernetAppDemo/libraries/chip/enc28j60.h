#ifndef _ENC28J60_LIBRARY_
#define _ENC28J60_LIBRARY_

#include <furi.h>
#include <furi_hal.h>

#include "../protocol_tools/helper_mem.h"
#include "Spi_lib.h"

#define MAX_FRAMELEN 1500

#define RXSTART_INIT 0x0000
#define RXSTOP_INIT  0x0BFF

#define TXSTART_INIT 0x0C00
#define TXSTOP_INIT  0x11FF

#define ETHERCARD_SEND_PIPELINING      0
#define ETHERCARD_RETRY_LATECOLLISIONS 0

// Struct for the enc28j60
typedef struct {
    FuriHalSpiBusHandle* spi;
    uint8_t mac_address[6];
} enc28j60_t;

enc28j60_t* enc28j60_alloc(uint8_t* mac_address);
void free_enc28j60(enc28j60_t* instance);
uint8_t enc28j60_start(enc28j60_t* instance);
bool is_link_up(enc28j60_t* instance);
bool is_the_network_connected(enc28j60_t* instance);
uint16_t receive_packet(enc28j60_t* instance, uint8_t* buffer, uint16_t size);
void send_packet(enc28j60_t* instance, uint8_t* buffer, uint16_t len);
void enable_broadcast(enc28j60_t* instance);
void disable_broadcast(enc28j60_t* instance);
void enable_multicast(enc28j60_t* instance);
void disable_multicast(enc28j60_t* instance);
void enable_promiscuous(enc28j60_t* instance);
void disable_promiscuous(enc28j60_t* instance);

// To debugg
void show_message(uint8_t* buffer, uint16_t len);

#endif
