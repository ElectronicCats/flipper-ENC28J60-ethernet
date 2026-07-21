#include "cdp_module.h"

void cdp_module_init(void) {
    FURI_LOG_I("CDP", "CDP module initialized");
}

bool cdp_module_process_frame(uint8_t* frame, uint16_t length) {
    FURI_LOG_I("CDP", "Frame received (%u bytes)", length);

    if(!frame) {
        FURI_LOG_I("CDP", "NULL frame");
        return false;
    }

    if(!is_cdp(frame, length)) {
        FURI_LOG_I("CDP", "Not a CDP frame");
        return false;
    }

    FURI_LOG_I("CDP", "CDP EtherType detected");

    cdp_info_t info;

    if(!cdp_parse(frame, length, &info)) {
        FURI_LOG_I("CDP", "Parse failed");
        return false;
    }

    FURI_LOG_I("CDP", "Parse OK");

    neighbor_t neighbor = {0};

    if(!cdp_fill_neighbor(&info, &neighbor)) {
        FURI_LOG_I("CDP", "Neighbor build failed");
        return false;
    }

    neighbor.discovery_sources |= NEIGHBOR_SOURCE_CDP;

    neighbor_t* existing = neighbor_db_find(neighbor.mac);

    bool ok = false;

    if(existing) {
        FURI_LOG_I("CDP", "Updating neighbor");
        ok = neighbor_db_update(&neighbor);
    } else {
        FURI_LOG_I("CDP", "Adding neighbor");
        ok = neighbor_db_add(&neighbor);
    }

    FURI_LOG_I("CDP", "Device: %s", info.device_id);
    FURI_LOG_I("CDP", "Port: %s", info.port_id);
    FURI_LOG_I("CDP", "Platform: %s", info.platform);
    FURI_LOG_I("CDP", "Version: %s", info.software_version);

    FURI_LOG_I("CDP", "DB operation result: %d", ok);
    FURI_LOG_I("CDP", "DB count now: %u", neighbor_db_count());

    return ok;
}

static bool cdp_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);

    return cdp_module_process_frame((uint8_t*)frame, len);
}

bool cdp_module_run(scanner_session_t* session, uint32_t timeout_ms) {
    FURI_LOG_I("CDP", "Waiting for CDP frames...");
    uint16_t length = 0;

    return scanner_wait_for_packet(
        session, cdp_packet_predicate, NULL, NULL, NULL, &length, timeout_ms);
}

size_t cdp_module_count(void) {
    return neighbor_db_count_by_source(NEIGHBOR_SOURCE_CDP);
}

neighbor_t* cdp_module_get(size_t index) {
    return neighbor_db_get_by_source(NEIGHBOR_SOURCE_CDP, index);
}

static const char* cdp_get_display_name(void) {
    return "CDP";
}

static void cdp_init(App* app) {
    enable_multicast(app->ethernet);

    cdp_module_init();
}

static bool cdp_run(scanner_session_t* session, uint32_t timeout_ms) {
    return cdp_module_run(session, timeout_ms);
}

static void cdp_cleanup(App* app) {
    disable_multicast(app->ethernet);
}

const PassiveProtocolHandler cdp_protocol_handler = {
    .get_display_name = cdp_get_display_name,
    .init = cdp_init,
    .run = cdp_run,
    .cleanup = cdp_cleanup,
    .get_neighbor_count = cdp_module_count,
    .get_neighbor = cdp_module_get,
};
