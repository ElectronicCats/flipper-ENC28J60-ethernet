#include "lldp_module.h"
#include "passive_details_text.h"

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

static void lldp_format_capability_mask(uint16_t mask, char* output, size_t output_size) {
    output[0] = '\0';

    if(mask & 0x0001U) passive_details_append_label(output, output_size, "Other");
    if(mask & 0x0002U) passive_details_append_label(output, output_size, "Repeater");
    if(mask & 0x0004U) passive_details_append_label(output, output_size, "Bridge");
    if(mask & 0x0008U) passive_details_append_label(output, output_size, "WLAN AP");
    if(mask & 0x0010U) passive_details_append_label(output, output_size, "Router");
    if(mask & 0x0020U) passive_details_append_label(output, output_size, "Telephone");
    if(mask & 0x0040U) passive_details_append_label(output, output_size, "DOCSIS");
    if(mask & 0x0080U) passive_details_append_label(output, output_size, "Station");
    if(mask & 0x0100U) passive_details_append_label(output, output_size, "C-VLAN");
    if(mask & 0x0200U) passive_details_append_label(output, output_size, "S-VLAN");
    if(mask & 0x0400U) passive_details_append_label(output, output_size, "TPMR");

    uint16_t unknown = mask & 0xF800U;
    if(unknown) {
        char raw[20];
        snprintf(raw, sizeof(raw), "Unknown 0x%04X", unknown);
        passive_details_append_label(output, output_size, raw);
    }

    if(!output[0]) {
        snprintf(output, output_size, "None");
    }
}

static void
    lldp_format_capabilities(const neighbor_t* neighbor, char* output, size_t output_size) {
    char available[96];
    char enabled[96];

    if(neighbor->capabilities == 0 && neighbor->enabled_capabilities == 0) {
        snprintf(output, output_size, "N/A");
        return;
    }

    lldp_format_capability_mask(neighbor->capabilities, available, sizeof(available));
    lldp_format_capability_mask(neighbor->enabled_capabilities, enabled, sizeof(enabled));

    if(neighbor->capabilities == neighbor->enabled_capabilities) {
        snprintf(output, output_size, "%s (0x%04X)", enabled, neighbor->enabled_capabilities);
    } else {
        snprintf(output, output_size, "Enabled: %s; Available: %s", enabled, available);
    }
}

static const char* lldp_chassis_header(uint8_t subtype) {
    switch(subtype) {
    case LLDP_CHASSIS_COMPONENT:
        return "CHASSIS COMPONENT";
    case LLDP_CHASSIS_INTERFACE_ALIAS:
        return "CHASSIS IF ALIAS";
    case LLDP_CHASSIS_PORT_COMPONENT:
        return "CHASSIS PORT";
    case LLDP_CHASSIS_MAC_ADDRESS:
        return "CHASSIS MAC";
    case LLDP_CHASSIS_NETWORK_ADDRESS:
        return "CHASSIS NETWORK";
    case LLDP_CHASSIS_INTERFACE_NAME:
        return "CHASSIS IF NAME";
    case LLDP_CHASSIS_LOCAL:
        return "CHASSIS LOCAL";
    default:
        return "CHASSIS ID";
    }
}

static const char* lldp_port_header(uint8_t subtype) {
    switch(subtype) {
    case LLDP_PORT_INTERFACE_ALIAS:
        return "PORT IF ALIAS";
    case LLDP_PORT_COMPONENT:
        return "PORT COMPONENT";
    case LLDP_PORT_MAC_ADDRESS:
        return "PORT MAC";
    case LLDP_PORT_NETWORK_ADDRESS:
        return "PORT NETWORK";
    case LLDP_PORT_INTERFACE_NAME:
        return "PORT IF NAME";
    case LLDP_PORT_AGENT_CIRCUIT_ID:
        return "PORT CIRCUIT ID";
    case LLDP_PORT_LOCAL:
        return "PORT LOCAL";
    default:
        return "PORT ID";
    }
}

static const char* lldp_power_type_name(uint8_t type) {
    switch(type) {
    case 0:
        return "PSE";
    case 1:
        return "PD";
    default:
        return "Unknown";
    }
}

static const char* lldp_power_source_name(uint8_t type, uint8_t source) {
    if(type == 0) {
        switch(source) {
        case 1:
            return "Primary";
        case 2:
            return "Backup";
        case 3:
            return "Reserved";
        default:
            return "Unknown";
        }
    }

    if(type == 1) {
        switch(source) {
        case 1:
            return "PSE";
        case 2:
            return "Local";
        case 3:
            return "Both";
        default:
            return "Unknown";
        }
    }

    return "Unknown";
}

static const char* lldp_power_priority_name(uint8_t priority) {
    switch(priority) {
    case 0:
        return "Unknown";
    case 1:
        return "Critical";
    case 2:
        return "High";
    case 3:
        return "Low";
    default:
        return "Reserved";
    }
}

static const char* lldp_power_pair_name(uint8_t pair) {
    switch(pair) {
    case 1:
        return "Signal";
    case 2:
        return "Spare";
    default:
        return "Unknown";
    }
}

static uint8_t lldp_get_details_page_count(neighbor_t* neighbor) {
    if(!neighbor) {
        return 0;
    }

    char capabilities[256];
    lldp_format_capabilities(neighbor, capabilities, sizeof(capabilities));

    return passive_details_text_page_count(neighbor->name) +
           passive_details_text_page_count(neighbor->port) +
           passive_details_text_page_count(neighbor->chassis_id) +
           passive_details_text_page_count(capabilities) +
           passive_details_text_page_count(neighbor->description) +
           passive_details_text_page_count(neighbor->vlan_name) + 6U;
}

static bool lldp_build_long_page(
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

    if(lldp_build_long_page(
           "SYSTEM NAME", neighbor->name, &page, line1, line1_size, line2, line2_size)) {
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

    if(lldp_build_long_page(
           lldp_port_header(neighbor->lldp_port_subtype),
           neighbor->port,
           &page,
           line1,
           line1_size,
           line2,
           line2_size)) {
        return;
    }

    if(lldp_build_long_page(
           lldp_chassis_header(neighbor->lldp_chassis_subtype),
           neighbor->chassis_id,
           &page,
           line1,
           line1_size,
           line2,
           line2_size)) {
        return;
    }

    if(page == 0) {
        snprintf(line1, line1_size, "TTL");
        snprintf(line2, line2_size, "%u s", neighbor->ttl);
        return;
    }
    page--;

    char capabilities[256];
    lldp_format_capabilities(neighbor, capabilities, sizeof(capabilities));
    if(lldp_build_long_page(
           "CAPABILITIES", capabilities, &page, line1, line1_size, line2, line2_size)) {
        return;
    }

    if(lldp_build_long_page(
           "SYSTEM DESCRIPTION",
           neighbor->description,
           &page,
           line1,
           line1_size,
           line2,
           line2_size)) {
        return;
    }

    if(page == 0) {
        snprintf(line1, line1_size, "PORT VLAN ID");
        if(neighbor->has_pvid) {
            snprintf(line2, line2_size, "%u", neighbor->pvid);
        } else {
            snprintf(line2, line2_size, "N/A");
        }
        snprintf(line3, line3_size, "NAMED VLAN ID");
        if(neighbor->has_vlan_name) {
            snprintf(line4, line4_size, "%u", neighbor->vlan_id);
        } else {
            snprintf(line4, line4_size, "N/A");
        }
        return;
    }
    page--;

    if(lldp_build_long_page(
           "VLAN NAME",
           neighbor->has_vlan_name ? neighbor->vlan_name : NULL,
           &page,
           line1,
           line1_size,
           line2,
           line2_size)) {
        return;
    }

    if(page == 0) {
        snprintf(line1, line1_size, "NETWORK POLICY VLAN");
        if(neighbor->has_network_policy) {
            snprintf(line2, line2_size, "%u", neighbor->network_policy_vlan);
        } else {
            snprintf(line2, line2_size, "N/A");
        }
        return;
    }
    page--;

    if(page == 0) {
        snprintf(line1, line1_size, "POE / DEVICE");
        if(!neighbor->has_poe) {
            snprintf(line2, line2_size, "N/A");
        } else if(neighbor->has_poe_mdi && neighbor->poe_supported) {
            snprintf(
                line2, line2_size, "Yes / %s", lldp_power_type_name(neighbor->poe_power_type));
        } else if(neighbor->has_poe_mdi) {
            snprintf(line2, line2_size, "No / %s", lldp_power_type_name(neighbor->poe_power_type));
        } else if(neighbor->has_poe_power_values) {
            snprintf(
                line2, line2_size, "MED / %s", lldp_power_type_name(neighbor->poe_power_type));
        } else {
            snprintf(line2, line2_size, "Advertised");
        }
        snprintf(line3, line3_size, "POWER PAIR / CLASS");
        if(neighbor->has_poe_mdi) {
            snprintf(
                line4,
                line4_size,
                "%s / Class %u",
                lldp_power_pair_name(neighbor->poe_power_pair),
                neighbor->poe_power_class);
        } else {
            snprintf(line4, line4_size, "N/A");
        }
        return;
    }
    page--;

    if(page == 0) {
        snprintf(line1, line1_size, "MED POWER");
        if(neighbor->has_poe_power_values) {
            snprintf(
                line2,
                line2_size,
                "%u.%u W",
                neighbor->poe_power_watts / 10U,
                neighbor->poe_power_watts % 10U);
            snprintf(line3, line3_size, "TYPE/SOURCE/PRIORITY");
            snprintf(
                line4,
                line4_size,
                "%s/%s/%s",
                lldp_power_type_name(neighbor->poe_power_type),
                lldp_power_source_name(neighbor->poe_power_type, neighbor->poe_power_source),
                lldp_power_priority_name(neighbor->poe_power_priority));
        } else {
            snprintf(line2, line2_size, "N/A");
            snprintf(line3, line3_size, "TYPE/SOURCE/PRIORITY");
            snprintf(line4, line4_size, "N/A");
        }
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
