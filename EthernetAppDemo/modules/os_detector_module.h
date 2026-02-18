#pragma once
#include <stdint.h>

typedef enum {
    WINDOWS,
    LINUX,
    IOS,
    NO_DETECTED,
} OS_DETECTOR_OS;

typedef enum {
    OFP_UNSET,
    OFP_TSEQ,
    OFP_TOPS,
    OFP_TECN,
    OFP_T1_7,
    OFP_TICMP,
    OFP_TUDP,
} OFP_PROBES_TYPE;

int32_t os_scan(void* context, uint8_t* target_ip);
