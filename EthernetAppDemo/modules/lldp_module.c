#include "lldp_module.h"

void lldp_module_init(void) {
    neighbor_db_clear();
}

void lldp_module_reset(void) {
    neighbor_db_clear();
}

bool lldp_module_process_frame(uint8_t* frame, uint16_t length) {
    if(!frame) return false;

    if(!is_lldp(frame)) {
        return false;
    }

    lldp_info_t info;

    if(!lldp_parse(frame, length, &info)) {
        return false;
    }

    neighbor_t neighbor;

    if(!lldp_fill_neighbor(&info, &neighbor)) {
        return false;
    }

    return neighbor_db_add(&neighbor);
}

bool lldp_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);

    return lldp_module_process_frame((uint8_t*)frame, len);
}

bool lldp_module_run(scanner_session_t* session, uint32_t timeout_ms) {
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
