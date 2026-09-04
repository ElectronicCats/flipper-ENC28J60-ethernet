#pragma once

#include "../app_user.h"
#include "../libraries/protocol_tools/cdp.h"
#include "passive_discovery_handler.h"

#include <stdbool.h>
#include <stdint.h>

void cdp_module_init(void);

void cdp_module_reset(void);

bool cdp_module_process_frame(uint8_t* frame, uint16_t length);

extern const PassiveProtocolHandler cdp_protocol_handler;
