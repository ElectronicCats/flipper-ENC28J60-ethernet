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

typedef enum {
    IPID_UNKNOWN = 0,
    IPID_CONSTANT,
    IPID_INCREMENTAL,
    IPID_INCREMENTAL_LARGE,
    IPID_RANDOM,
    IPID_ZERO
} ipid_pattern_t;

typedef struct {
    int windows_score;
    int linux_score;
    int ios_score;
} os_scoreboard_t;

int32_t os_scan(void* context, uint8_t* target_ip);
