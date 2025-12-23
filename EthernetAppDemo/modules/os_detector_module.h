#pragma once

#include <furi.h>

typedef enum {
    OFP_UNSET,
    OFP_TSEQ,
    OFP_TOPS,
    OFP_TECN,
    OFP_T1_7,
    OFP_TICMP,
    OFP_TUDP,
} OFP_PROBES_TYPE;

void os_scan(void* context, uint8_t* target_ip);