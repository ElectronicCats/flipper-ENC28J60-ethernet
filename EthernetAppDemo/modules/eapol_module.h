#pragma once

#include "../app_user.h"
#include "../libraries/protocol_tools/eapol.h"
#include "passive_discovery_handler.h"

#include <stdbool.h>
#include <stdint.h>

void eapol_module_init(void);

void eapol_module_reset(void);

bool eapol_module_process_frame(uint8_t* frame, uint16_t length);

extern const PassiveProtocolHandler eapol_protocol_handler;
