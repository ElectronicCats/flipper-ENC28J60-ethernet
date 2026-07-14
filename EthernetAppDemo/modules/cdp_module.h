#ifndef CDP_MODULE_H
#define CDP_MODULE_H

#include "../libraries/protocol_tools/cdp.h"
#include "../libraries/protocol_tools/neighbor_db.h"
#include "../libraries/scanner/scanner_session.h"
#include "passive_protocol_handler.h"

void cdp_module_init(void);

void cdp_module_reset(void);

bool cdp_module_process_frame(uint8_t* frame, uint16_t length);

bool cdp_module_run(scanner_session_t* session, uint32_t timeout_ms);

size_t cdp_module_count(void);

neighbor_t* cdp_module_get(size_t index);

extern const PassiveProtocolHandler cdp_protocol_handler;

#endif
