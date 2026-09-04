#include "lldp_module.h"

void lldp_module_init(void) {
    neighbor_db_clear_by_source(NEIGHBOR_SOURCE_LLDP);
}

void lldp_module_reset(void) {
    neighbor_db_clear_by_source(NEIGHBOR_SOURCE_LLDP);
}

bool lldp_module_process_frame(uint8_t* frame, uint16_t length) {
    printf("LLDP FRAME %u\n", length);
    fflush(stdout);
    if(!frame) {
        FURI_LOG_I("LLDP", "NULL frame");
        return false;
    }

    if(frame[0] == 0x01 && frame[1] == 0x80 && frame[2] == 0xC2) {
        FURI_LOG_I(
            "RX",
            "802.1 multicast received: %02X:%02X:%02X:%02X:%02X:%02X",
            frame[0],
            frame[1],
            frame[2],
            frame[3],
            frame[4],
            frame[5]);
    }

    if(!is_lldp(frame)) {
        FURI_LOG_I(
            "LLDP",
            "Ether bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            frame[0],
            frame[1],
            frame[2],
            frame[3],
            frame[4],
            frame[5],
            frame[6],
            frame[7],
            frame[8],
            frame[9],
            frame[10],
            frame[11],
            frame[12],
            frame[13]);

        return false;
    }

    lldp_info_t info;

    neighbor_t neighbor;

    if(!lldp_parse(frame, length, &info)) {
        FURI_LOG_I("LLDP", "Parse failed");
        return false;
    }

    if(!lldp_fill_neighbor(&info, &neighbor)) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find_by_source(neighbor.mac, NEIGHBOR_SOURCE_LLDP);

    bool ok = false;

    if(existing) {
        FURI_LOG_I("LLDP", "Updating neighbor");
        ok = neighbor_db_update(&neighbor);
    } else {
        FURI_LOG_I("LLDP", "Adding neighbor");
        ok = neighbor_db_add(&neighbor);
    }

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

static const char* lldp_get_display_name(void) {
    return "LLDP";
}

static void lldp_init(App* app) {
    UNUSED(app);
    lldp_module_init();
}

static bool lldp_run(scanner_session_t* session, uint32_t timeout_ms) {
    return lldp_module_run(session, timeout_ms);
}

static void lldp_cleanup(App* app) {
    UNUSED(app);
}

static uint8_t lldp_get_details_page_count(neighbor_t* neighbor) {
    UNUSED(neighbor);

    /*
 * LLDP detail pages
 *
 * Page 1  - Name / MAC
 * Page 2  - Port / IP
 * Page 3  - VLAN / VLAN Name
 * Page 4  - Network Policy / Capabilities
 * Page 5  - TTL / Chassis ID
 * Page 6  - Description
 * Page 7  - PoE / Power Class
 * Page 8  - Power / Source
 */
    return 8;
}

static void lldp_build_details_page(
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
    /*
 * Page 1
 * NAME / MAC
 */
    case 0:

        snprintf(line1, line1_size, "NAME");

        snprintf(line2, line2_size, "%s", neighbor->name[0] ? neighbor->name : "Unknown");

        snprintf(line3, line3_size, "MAC");

        snprintf(
            line4,
            line4_size,
            "%02X:%02X:%02X:%02X:%02X:%02X",
            neighbor->mac[0],
            neighbor->mac[1],
            neighbor->mac[2],
            neighbor->mac[3],
            neighbor->mac[4],
            neighbor->mac[5]);

        break;

    /*
 * Page 2
 * PORT / IP
 */
    case 1:

        snprintf(line1, line1_size, "PORT");

        snprintf(line2, line2_size, "%s", neighbor->port[0] ? neighbor->port : "N/A");

        snprintf(line3, line3_size, "IP");

        snprintf(
            line4,
            line4_size,
            "%s",
            neighbor->management_address[0] ? neighbor->management_address : "N/A");

        break;

    /*
 * Page 3
 * VLAN / VLAN NAME
 */
    case 2:

        snprintf(line1, line1_size, "VLAN");

        if(neighbor->has_pvid) {
            snprintf(line2, line2_size, "%u (PVID)", neighbor->pvid);

        } else if(neighbor->vlan_id != 0) {
            snprintf(line2, line2_size, "%u", neighbor->vlan_id);

        } else {
            snprintf(line2, line2_size, "N/A");
        }

        snprintf(line3, line3_size, "VLAN NAME");

        snprintf(
            line4,
            line4_size,
            "%s",
            neighbor->has_vlan_name && neighbor->vlan_name[0] ? neighbor->vlan_name : "N/A");

        break;

    /*
 * Page 4
 * NETWORK POLICY / CAPABILITIES
 */
    case 3:

        snprintf(line1, line1_size, "NETWORK POLICY");

        if(neighbor->has_network_policy) {
            snprintf(line2, line2_size, "VLAN %u", neighbor->network_policy_vlan);

        } else {
            snprintf(line2, line2_size, "N/A");
        }

        snprintf(line3, line3_size, "CAPABILITIES");

        snprintf(line4, line4_size, "0x%04X", neighbor->capabilities);

        break;

    /*
 * Page 5
 * TTL / CHASSIS ID
 */
    case 4:

        snprintf(line1, line1_size, "TTL");

        snprintf(line2, line2_size, "%u s", neighbor->ttl);

        snprintf(line3, line3_size, "CHASSIS ID");

        snprintf(line4, line4_size, "%s", neighbor->chassis_id[0] ? neighbor->chassis_id : "N/A");

        break;

    /*
 * Page 6
 * DESCRIPTION
 *
 * This page intentionally uses all four
 * available text lines.
 */
    case 5: {
        const char* description = neighbor->description[0] ? neighbor->description : "N/A";

        /*
     * Split the description into multiple
     * display lines.
     *
     * The widget uses FontSecondary,
     * therefore keep each line relatively
     * short to avoid clipping.
     */
        size_t length = strlen(description);

        if(length <= 20) {
            snprintf(line1, line1_size, "DESCRIPTION");

            snprintf(line2, line2_size, "%s", description);

        } else {
            snprintf(line1, line1_size, "DESCRIPTION");

            size_t split = 20;

            if(split >= length) {
                split = length;
            }

            while(split > 0 && description[split] != '\0' && description[split] != ' ') {
                split--;
            }

            if(split == 0) {
                split = 20;
            }

            if(split >= line2_size) {
                split = line2_size - 1;
            }

            memcpy(line2, description, split);

            line2[split] = '\0';

            while(description[split] == ' ') {
                split++;
            }

            snprintf(line3, line3_size, "%s", &description[split]);
        }

        break;
    }

    /*
 * Page 7
 * POE / POWER CLASS
 */
    case 6:

        snprintf(line1, line1_size, "POE");

        snprintf(line2, line2_size, "%s", neighbor->has_poe ? "Supported" : "N/A");

        snprintf(line3, line3_size, "POWER CLASS");

        if(neighbor->poe_power_class != 0) {
            snprintf(line4, line4_size, "%u", neighbor->poe_power_class);

        } else {
            snprintf(line4, line4_size, "N/A");
        }

        break;

    /*
 * Page 8
 * POWER / SOURCE
 */
    case 7:

        snprintf(line1, line1_size, "POWER");

        if(neighbor->has_poe_power_values) {
            snprintf(line2, line2_size, "%u W", neighbor->poe_power_watts);

        } else {
            snprintf(line2, line2_size, "N/A");
        }

        snprintf(line3, line3_size, "POWER SOURCE");

        if(neighbor->has_poe) {
            snprintf(line4, line4_size, "%u", neighbor->poe_power_source);

        } else {
            snprintf(line4, line4_size, "N/A");
        }

        break;

    default:
        break;
    }
}

const PassiveProtocolHandler lldp_protocol_handler = {
    .get_display_name = lldp_get_display_name,
    .init = lldp_init,
    .run = lldp_run,
    .process_frame = lldp_module_process_frame,
    .cleanup = lldp_cleanup,
    .get_details_page_count = lldp_get_details_page_count,
    .build_details_page = lldp_build_details_page,
};
