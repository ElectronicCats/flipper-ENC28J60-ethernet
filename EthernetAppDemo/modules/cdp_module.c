#include "cdp_module.h"
#include "passive_details_text.h"

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

static void cdp_format_capabilities(uint16_t mask, char* output, size_t output_size) {
    output[0] = '\0';

    if(mask == 0) {
        snprintf(output, output_size, "N/A");
        return;
    }

    if(mask & 0x0001U) passive_details_append_label(output, output_size, "Router");
    if(mask & 0x0002U) passive_details_append_label(output, output_size, "T-Bridge");
    if(mask & 0x0004U) passive_details_append_label(output, output_size, "SR-Bridge");
    if(mask & 0x0008U) passive_details_append_label(output, output_size, "Switch");
    if(mask & 0x0010U) passive_details_append_label(output, output_size, "Host");
    if(mask & 0x0020U) passive_details_append_label(output, output_size, "IGMP");
    if(mask & 0x0040U) passive_details_append_label(output, output_size, "Repeater");
    if(mask & 0x0080U) passive_details_append_label(output, output_size, "Phone");
    if(mask & 0x0100U) passive_details_append_label(output, output_size, "Remote");
    if(mask & 0x0200U) passive_details_append_label(output, output_size, "CVTA");
    if(mask & 0x0400U) passive_details_append_label(output, output_size, "TPMR");

    uint16_t unknown = mask & 0xF800U;
    if(unknown) {
        char raw[20];
        snprintf(raw, sizeof(raw), "Unknown 0x%04X", unknown);
        passive_details_append_label(output, output_size, raw);
    }

    size_t used = strlen(output);
    if(used < output_size - 1) {
        snprintf(output + used, output_size - used, " (0x%04X)", mask);
    }
}

static void cdp_get_description_parts(
    const neighbor_t* neighbor,
    char* platform,
    size_t platform_size,
    char* software,
    size_t software_size) {
    platform[0] = '\0';
    software[0] = '\0';

    const char* separator = strstr(neighbor->description, " | ");
    if(!separator) {
        snprintf(software, software_size, "%s", neighbor->description);
        return;
    }

    size_t platform_length = separator - neighbor->description;
    if(platform_length >= platform_size) {
        platform_length = platform_size - 1;
    }
    memcpy(platform, neighbor->description, platform_length);
    platform[platform_length] = '\0';
    snprintf(software, software_size, "%s", separator + 3);
}

static uint8_t cdp_get_details_page_count(neighbor_t* neighbor) {
    if(!neighbor) {
        return 0;
    }

    char capabilities[160];
    char platform[64];
    char software[128];
    cdp_format_capabilities(neighbor->capabilities, capabilities, sizeof(capabilities));
    cdp_get_description_parts(neighbor, platform, sizeof(platform), software, sizeof(software));

    return passive_details_text_page_count(neighbor->name) +
           passive_details_text_page_count(neighbor->port) +
           passive_details_text_page_count(capabilities) +
           passive_details_text_page_count(platform) + passive_details_text_page_count(software) +
           2U;
}

static bool cdp_build_long_page(
    const char* header,
    const char* value,
    uint8_t* page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size) {
    uint8_t page_count = passive_details_text_page_count(value);
    if(*page >= page_count) {
        *page -= page_count;
        return false;
    }

    snprintf(line1, line1_size, "%s", header);
    passive_details_text_get_page(value, *page, line2, line2_size);
    return true;
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

    if(cdp_build_long_page(
           "DEVICE ID", neighbor->name, &page, line1, line1_size, line2, line2_size)) {
        return;
    }

    if(page == 0) {
        snprintf(line1, line1_size, "SOURCE MAC");
        passive_details_format_mac(neighbor->mac, line2, line2_size);
        snprintf(line3, line3_size, "MANAGEMENT IPV4");
        snprintf(
            line4,
            line4_size,
            "%s",
            neighbor->management_address[0] ? neighbor->management_address : "N/A");
        return;
    }
    page--;

    if(cdp_build_long_page(
           "PORT ID", neighbor->port, &page, line1, line1_size, line2, line2_size)) {
        return;
    }

    if(page == 0) {
        snprintf(line1, line1_size, "TTL");
        snprintf(line2, line2_size, "%u s", neighbor->ttl);
        return;
    }
    page--;

    char capabilities[160];
    cdp_format_capabilities(neighbor->capabilities, capabilities, sizeof(capabilities));
    if(cdp_build_long_page(
           "CAPABILITIES", capabilities, &page, line1, line1_size, line2, line2_size)) {
        return;
    }

    char platform[64];
    char software[128];
    cdp_get_description_parts(neighbor, platform, sizeof(platform), software, sizeof(software));

    if(cdp_build_long_page("PLATFORM", platform, &page, line1, line1_size, line2, line2_size)) {
        return;
    }

    cdp_build_long_page("SOFTWARE VERSION", software, &page, line1, line1_size, line2, line2_size);
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
