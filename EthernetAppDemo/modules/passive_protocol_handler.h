#pragma once

#include "../app_user.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration to avoid dependency cycle with scanner headers
typedef struct ScannerSession scanner_session_t;

typedef struct {
    // Protocol display name
    const char* (*get_display_name)(void);

    // Protocol-specific hardware/database setup
    void (*init)(App* app);

    // Blocking loop packet acquisition function
    bool (*run)(scanner_session_t* session, uint32_t timeout_ms);

    // Process a single frame (used by ALL mode)
    bool (*process_frame)(uint8_t* frame, uint16_t length);

    // Protocol-specific teardown and hardware restore
    void (*cleanup)(App* app);

    // Number of discovered neighbors for this protocol
    size_t (*get_neighbor_count)(void);

    // Returns the neighbor at the specified index
    neighbor_t* (*get_neighbor)(size_t index);
} PassiveProtocolHandler;
