#include "lldp_module.h"

static uint32_t frames_seen = 0;
static uint32_t lldp_frames = 0;

void lldp_module_init(void) {
    neighbor_db_clear();
}

void lldp_module_reset(void) {
    neighbor_db_clear();
}

bool lldp_module_process_frame(uint8_t* frame, uint16_t length) {
    printf("LLDP FRAME %u\n", length);
    fflush(stdout);
    FURI_LOG_I("LLDP", "Frame len=%u", length);
    if(!frame) {
        FURI_LOG_I("LLDP", "NULL frame");
        return false;
    }

    if(!is_lldp(frame)) {
        FURI_LOG_I("LLDP", "Not LLDP");
        return false;
    }

    FURI_LOG_I("LLDP", "EtherType LLDP OK");

    lldp_info_t info;

    if(!lldp_parse(frame, length, &info)) {
        FURI_LOG_I("LLDP", "Parse failed");
        return false;
    }

    FURI_LOG_I("LLDP", "Parse OK");

    neighbor_t neighbor;

    if(!lldp_fill_neighbor(&info, &neighbor)) {
        return false;
    }

    FURI_LOG_I(
        "LLDP",
        "Neighbor MAC parsed: %02X:%02X:%02X:%02X:%02X:%02X",
        neighbor.mac[0],
        neighbor.mac[1],
        neighbor.mac[2],
        neighbor.mac[3],
        neighbor.mac[4],
        neighbor.mac[5]);

    FURI_LOG_I("LLDP", "DB check starting");

    neighbor_t* existing = neighbor_db_find(neighbor.mac);

    if(existing) {
        FURI_LOG_I("LLDP", "Updating neighbor");
        return neighbor_db_update(&neighbor);
    } else {
        FURI_LOG_I("LLDP", "Adding neighbor");
        return neighbor_db_add(&neighbor);
    }

    bool ok = false;

    if(existing) {
        FURI_LOG_I("LLDP", "Updating neighbor");
        ok = neighbor_db_update(&neighbor);
    } else {
        FURI_LOG_I("LLDP", "Adding neighbor");
        ok = neighbor_db_add(&neighbor);
    }

    FURI_LOG_I("LLDP", "DB operation result: %d", ok);
    FURI_LOG_I("LLDP", "DB count now: %u", neighbor_db_count());

    frames_seen++;

    if(!is_lldp(frame)) {
        return false;
    }

    lldp_frames++;

    FURI_LOG_I("LLDP", "%lu / %lu", lldp_frames, frames_seen);

    return ok;
}

bool lldp_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);

    return lldp_module_process_frame((uint8_t*)frame, len);
}

bool lldp_module_run(scanner_session_t* session, uint32_t timeout_ms) {
    printf("LLDP RUN\n");
    fflush(stdout);

    uint16_t length = 0;

    return scanner_wait_for_packet(
        session, lldp_packet_predicate, NULL, NULL, NULL, &length, timeout_ms);
}

size_t lldp_module_count(void) {
    return neighbor_db_count();
}

neighbor_t* lldp_module_get(size_t index) {
    return neighbor_db_get(index);
}
