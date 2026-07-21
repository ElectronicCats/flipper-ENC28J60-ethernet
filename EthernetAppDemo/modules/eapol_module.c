#include "eapol_module.h"

static void eapol_init(App* app) {
    enable_multicast(app->ethernet);

    //neighbor_db_clear_by_source(NEIGHBOR_SOURCE_EAPOL);

    eapol_module_init();
}

void eapol_module_init(void) {
    neighbor_db_load();
}

bool eapol_module_process_frame(uint8_t* frame, uint16_t length) {
    if(!frame) {
        return false;
    }

    if(!is_eapol(frame, length)) {
        return false;
    }

    FURI_LOG_I("EAPOL", "EAPOL frame (%u bytes)", length);

    eapol_info_t info;

    if(!eapol_parse(frame, length, &info)) {
        FURI_LOG_W("EAPOL", "Parser failed");
        return false;
    }

    FURI_LOG_I("EAPOL", "Parsed: type=%u code=%u", info.packet_type, info.code);

    neighbor_t neighbor;

    if(!eapol_fill_neighbor(&info, &neighbor)) {
        FURI_LOG_W("EAPOL", "Neighbor conversion failed");
        return false;
    }

    bool ok;

    if(neighbor_db_find(neighbor.mac)) {
        ok = neighbor_db_update(&neighbor);
    } else {
        ok = neighbor_db_add(&neighbor);
    }

    FURI_LOG_I("EAPOL", "Neighbor DB: %s", ok ? "updated" : "failed");

    return ok;
}

static bool eapol_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);

    return eapol_module_process_frame((uint8_t*)frame, len);
}

bool eapol_module_run(scanner_session_t* session, uint32_t timeout_ms) {
    uint16_t length = 0;

    return scanner_wait_for_packet(
        session, eapol_packet_predicate, NULL, NULL, NULL, &length, timeout_ms);
}

size_t eapol_module_count(void) {
    return neighbor_db_count_by_source(NEIGHBOR_SOURCE_EAPOL);
}

static const char* eapol_get_display_name(void) {
    return "EAPOL";
}

static bool eapol_run(scanner_session_t* session, uint32_t timeout_ms) {
    return eapol_module_run(session, timeout_ms);
}

neighbor_t* eapol_module_get(size_t index) {
    return neighbor_db_get_by_source(NEIGHBOR_SOURCE_EAPOL, index);
}

static void eapol_cleanup(App* app) {
    disable_multicast(app->ethernet);
}

const PassiveProtocolHandler eapol_protocol_handler = {
    .get_display_name = eapol_get_display_name,
    .init = eapol_init,
    .run = eapol_run,
    .cleanup = eapol_cleanup,
    .get_neighbor_count = eapol_module_count,
    .get_neighbor = eapol_module_get,
};
