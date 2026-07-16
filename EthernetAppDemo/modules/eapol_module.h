#pragma once

#include "../app_user.h"
#include "../libraries/protocol_tools/eapol.h"
#include "../libraries/protocol_tools/neighbor_db.h"
#include "../libraries/scanner/scanner_session.h"
#include "passive_protocol_handler.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initializes the EAPOL discovery module.
 *
 * Loads the persisted neighbor database before starting a new passive scan.
 */
void eapol_module_init(void);

/**
 * @brief Clears all discovered EAPOL neighbors.
 */
void eapol_module_reset(void);

/**
 * @brief Processes a received Ethernet frame.
 *
 * If the frame contains a valid EAPOL packet, it is parsed and the
 * neighbor database is updated.
 *
 * @param frame Pointer to the received Ethernet frame.
 * @param length Frame length in bytes.
 *
 * @return true if the frame contained valid EAPOL information.
 * @return false otherwise.
 */
bool eapol_module_process_frame(uint8_t* frame, uint16_t length);

/**
 * @brief Returns the number of discovered EAPOL neighbors.
 *
 * @return Number of occupied entries.
 */
size_t eapol_module_count(void);

/**
 * @brief Returns a discovered EAPOL neighbor.
 *
 * @param index Neighbor index.
 *
 * @return Pointer to the neighbor entry or NULL if index is invalid.
 */
neighbor_t* eapol_module_get(size_t index);

/**
 * @brief Passive packet acquisition loop.
 */
bool eapol_module_run(scanner_session_t* session, uint32_t timeout_ms);

/**
 * @brief Passive protocol handler exported to Passive Discovery.
 */
extern const PassiveProtocolHandler eapol_protocol_handler;
