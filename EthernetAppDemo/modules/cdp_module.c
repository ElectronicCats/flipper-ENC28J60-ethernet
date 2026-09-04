#include "cdp_module.h"

#include <stdio.h>
#include <string.h>

void cdp_module_init(void) {
    neighbor_db_clear_by_source(NEIGHBOR_SOURCE_CDP);
}

void cdp_module_reset(void) {
    neighbor_db_clear_by_source(NEIGHBOR_SOURCE_CDP);
}

bool cdp_module_process_frame(uint8_t* frame, uint16_t length) {
    cdp_info_t info;
    if(!cdp_parse(frame, length, &info)) {
        return false;
    }

    neighbor_t neighbor;
    if(!cdp_fill_neighbor(&info, &neighbor)) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find_by_source(neighbor.mac, NEIGHBOR_SOURCE_CDP);
    if(existing) {
        return neighbor_db_update(&neighbor);
    }

    return neighbor_db_add(&neighbor);
}

static const char* cdp_get_display_name(void) {
    return "CDP";
}

static void cdp_init(App* app) {
    UNUSED(app);
    cdp_module_init();
}

static void cdp_cleanup(App* app) {
    UNUSED(app);
}

static uint8_t cdp_get_details_page_count(neighbor_t* neighbor) {
    UNUSED(neighbor);
    return 3;
}

static void cdp_copy_platform(
    const char* description,
    const char* separator,
    char* output,
    size_t output_size) {
    if(!description[0] || !separator || separator == description) {
        snprintf(output, output_size, "N/A");
        return;
    }

    size_t length = separator - description;
    if(length > 20) {
        length = 20;
    }
    snprintf(output, output_size, "%.*s", (int)length, description);
}

static void cdp_build_details_page(
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size) {
    if(!neighbor) {
        return;
    }

    line1[0] = '\0';
    line2[0] = '\0';
    line3[0] = '\0';
    line4[0] = '\0';

    switch(page) {
    case 0:
        snprintf(line1, line1_size, "PORT");
        snprintf(line2, line2_size, "%.20s", neighbor->port[0] ? neighbor->port : "N/A");
        snprintf(
            line3,
            line3_size,
            "IP %.16s",
            neighbor->management_address[0] ? neighbor->management_address : "N/A");
        snprintf(line4, line4_size, "TTL %u s", neighbor->ttl);
        break;

    case 1: {
        const char* separator = strstr(neighbor->description, " | ");
        const char* software = separator ? separator + 3 : NULL;

        snprintf(line1, line1_size, "PLATFORM");
        cdp_copy_platform(neighbor->description, separator, line2, line2_size);
        snprintf(line3, line3_size, "SOFTWARE CAP:%04X", neighbor->capabilities);
        snprintf(line4, line4_size, "%.20s", software && software[0] ? software : "N/A");
        break;
    }

    default:
        break;
    }
}

const PassiveProtocolHandler cdp_protocol_handler = {
    .get_display_name = cdp_get_display_name,
    .init = cdp_init,
    .run = NULL,
    .process_frame = cdp_module_process_frame,
    .cleanup = cdp_cleanup,
    .get_details_page_count = cdp_get_details_page_count,
    .build_details_page = cdp_build_details_page,
};
