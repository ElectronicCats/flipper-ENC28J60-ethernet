#pragma once

#include "../../app_user.h"

#define STARTUP_GUARD_HEAP_ALIGNMENT_BYTES         8U
#define STARTUP_GUARD_HEAP_BLOCK_HEADER_BYTES      8U
#define STARTUP_GUARD_THREAD_METADATA_HEAP_BYTES   328U
#define STARTUP_GUARD_SCANNER_SEMAPHORE_HEAP_BYTES 96U
#define STARTUP_GUARD_RESERVE_BYTES                1024U

typedef struct {
    size_t required_total_free;
    size_t required_max_block;
} StartupGuardRequirements;

typedef enum {
    StartupGuardReady,
    StartupGuardInsufficientTotal,
    StartupGuardInsufficientBlock,
} StartupGuardResult;

size_t startup_guard_allocation_block_size(size_t payload_bytes);

/*
 * API 87.1 allocates the FuriThread metadata and strings before its stack.
 * This derives a conservative first-fit max-block requirement as well as the
 * simultaneously-live total. The fixed reserve is headroom, not an allocation.
 */
StartupGuardRequirements startup_guard_thread_requirements(
    size_t stack_bytes,
    size_t additional_total_bytes,
    size_t additional_max_block_bytes);

StartupGuardResult startup_guard_check(StartupGuardRequirements requirements);

bool startup_guard_thread_slot_available(App* app, startup_retry_callback_t retry_callback);

void startup_guard_show_low_memory(
    App* app,
    const char* reason,
    startup_retry_callback_t retry_callback);

void startup_guard_clear(App* app);
