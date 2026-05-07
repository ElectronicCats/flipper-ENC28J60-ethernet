#pragma once
#include "../../app_user.h"

typedef bool (*rx_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);
typedef void (*rx_handler_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct rx_handle rx_handle_t;

void rx_dispatch_init(App* app);
void rx_dispatch_deinit(App* app);

rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx);
void rx_unregister(rx_handle_t* handle);

/**
 * Temporarily stop the dispatcher from consuming chip RX. Used by
 * ethernet_thread around DORA so that DHCP packets reach the worker's
 * own receive_packet calls instead of being eaten (and dropped, since
 * no registered handler matches DHCP) by the dispatcher.
 *
 * pause() blocks until the dispatcher has reached its idle sleep
 * (so the caller can safely call receive_packet right after).
 * resume() unblocks the dispatcher.
 *
 * Calls nest as a flat counter — pause-pause-resume-resume is fine.
 *
 * Proper architectural fix is F0.4b (DHCP becomes a registered
 * handler, no more shared chip access from the worker).
 */
void rx_dispatch_pause(void);
void rx_dispatch_resume(void);
