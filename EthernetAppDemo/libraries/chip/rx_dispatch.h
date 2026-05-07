#pragma once
#include "../../app_user.h"

typedef bool (*rx_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);
typedef void (*rx_handler_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct rx_handle rx_handle_t;

void rx_dispatch_init(App* app);
void rx_dispatch_deinit(App* app);

rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx);
void rx_unregister(rx_handle_t* handle);
