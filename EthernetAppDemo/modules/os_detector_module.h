#pragma once

#include <furi.h>

typedef enum {
    OS_WINDOWS,
    OS_LINUX,
    OS_MACOS,
    OS_FREEBSD,
    OS_ANDROID,
    OS_NETWORK_DEVICE,
    OS_UNKNOWN,
    OS_TYPE_COUNT,
} OsType;

typedef struct {
    OsType type;
    uint8_t ttl;
    uint16_t window_size;
} OsResult;

void os_scan(void* context, uint8_t* target_ip, OsResult* result);
