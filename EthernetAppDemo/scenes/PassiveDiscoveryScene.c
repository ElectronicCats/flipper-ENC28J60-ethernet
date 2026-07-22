#include "../app_user.h"
#include "../modules/passive_discovery_module.h"

static uint8_t passive_details_page_count(const neighbor_t* neighbor);

static void passive_format_sources(const neighbor_t* neighbor, char* output, size_t size) {
    if(!neighbor || !output || size == 0) return;

    bool has_lldp = neighbor->discovery_sources & NEIGHBOR_SOURCE_LLDP;
    bool has_cdp = neighbor->discovery_sources & NEIGHBOR_SOURCE_CDP;
    bool has_eapol = neighbor->discovery_sources & NEIGHBOR_SOURCE_EAPOL;

    if(has_lldp && has_cdp && has_eapol) {
        snprintf(output, size, "LLDP|CDP|EAP");
    } else if(has_lldp && has_cdp) {
        snprintf(output, size, "LLDP|CDP");
    } else if(has_lldp && has_eapol) {
        snprintf(output, size, "LLDP|EAP");
    } else if(has_cdp && has_eapol) {
        snprintf(output, size, "CDP|EAP");
    } else if(has_lldp) {
        snprintf(output, size, "LLDP");
    } else if(has_cdp) {
        snprintf(output, size, "CDP");
    } else if(has_eapol) {
        snprintf(output, size, "EAPOL");
    } else {
        snprintf(output, size, "Unknown");
    }
}

static void passive_format_capabilities(uint16_t caps, char* output, size_t size) {
    if(!output || size == 0) return;

    if(caps == 0) {
        snprintf(output, size, "N/A");
        return;
    }

    if((caps & 0x0004) && (caps & 0x0010)) {
        snprintf(output, size, "Bridge Router");
    } else if(caps & 0x0004) {
        snprintf(output, size, "Bridge");
    } else if(caps & 0x0010) {
        snprintf(output, size, "Router");
    } else if(caps & 0x0002) {
        snprintf(output, size, "Repeater");
    } else if(caps & 0x0008) {
        snprintf(output, size, "WLAN AP");
    } else {
        snprintf(output, size, "0x%04X", caps);
    }
}

static void passive_build_details_page(
    const neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size);

static uint16_t passive_scene_get_source(App* app) {
    switch(app->passive_discovery.protocol) {
    case PassiveProtocolALL:
        return NEIGHBOR_SOURCE_LLDP | NEIGHBOR_SOURCE_CDP | NEIGHBOR_SOURCE_EAPOL;

    case PassiveProtocolLLDP:
        return NEIGHBOR_SOURCE_LLDP;

    case PassiveProtocolCDP:
        return NEIGHBOR_SOURCE_CDP;

    case PassiveProtocolEAPOL:
        return NEIGHBOR_SOURCE_EAPOL;

    case PassiveProtocolClearAll:
        return NEIGHBOR_SOURCE_LLDP | NEIGHBOR_SOURCE_CDP | NEIGHBOR_SOURCE_EAPOL;

    default:
        return 0;
    }
}

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context);

static void passive_discovery_refresh(App* app);
static void show_neighbor_list(App* app);
static void build_neighbor_submenu(App* app);

static void passive_neighbor_callback(void* context, uint32_t index) {
    App* app = context;

    if(index == 0xFFFF) {
        neighbor_db_clear_by_source(passive_scene_get_source(app));
        neighbor_db_save();

        app->passive_neighbor_count = neighbor_db_count_by_source(passive_scene_get_source(app));

        build_neighbor_submenu(app);

        submenu_set_selected_item(app->submenu, 0);

        view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

        return;
    }

    app->passive_selected_neighbor = index;
    app->details_page = 0;

    app->passive_discovery.state = PassiveDiscoveryStateNeighborDetails;

    passive_discovery_refresh(app);
}

static void build_neighbor_submenu(App* app) {
    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "DISCOVERED DEVICES");

    //FURI_LOG_I("PASSIVE", "DB count = %u", passive_discovery_module_get_neighbor_count());

    size_t count;

    if(app->passive_discovery.protocol == PassiveProtocolALL ||
       app->passive_discovery.protocol == PassiveProtocolClearAll) {
        count = neighbor_db_count();
    } else {
        count = neighbor_db_count_by_source(passive_scene_get_source(app));
    }

    for(size_t i = 0; i < count; i++) {
        neighbor_t* neighbor;

        if(app->passive_discovery.protocol == PassiveProtocolALL ||
           app->passive_discovery.protocol == PassiveProtocolClearAll) {
            neighbor = neighbor_db_get(i);
        } else {
            neighbor = neighbor_db_get_by_source(passive_scene_get_source(app), i);
        }

        //FURI_LOG_I("PASSIVE", "slot=%u ptr=%p", i, neighbor);

        if(!neighbor) {
            continue;
        }

        //FURI_LOG_I("PASSIVE", "occupied=%u name='%s'", neighbor->occupied, neighbor->name);

        furi_string_reset(app->text);

        if(neighbor->name[0]) {
            furi_string_printf(app->text, "%s", neighbor->name);
        } else {
            furi_string_printf(
                app->text,
                "%02X:%02X:%02X:%02X:%02X:%02X",
                neighbor->mac[0],
                neighbor->mac[1],
                neighbor->mac[2],
                neighbor->mac[3],
                neighbor->mac[4],
                neighbor->mac[5]);
        }

        //FURI_LOG_I("PASSIVE", "ADDING '%s'", furi_string_get_cstr(app->text));

        submenu_add_item(
            app->submenu, furi_string_get_cstr(app->text), i, passive_neighbor_callback, app);
    }

    submenu_add_item(app->submenu, "[ Clear Results ]", 0xFFFF, passive_neighbor_callback, app);
}

static void show_neighbor_list(App* app) {
    build_neighbor_submenu(app);

    submenu_set_selected_item(app->submenu, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

static void show_neighbor_details(App* app) {
    neighbor_t* neighbor =
        neighbor_db_get_by_source(passive_scene_get_source(app), app->passive_selected_neighbor);

    widget_reset(app->widget);

    if(!neighbor) {
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No data");

        return;
    }

    char line1[64] = {0};
    char line2[64] = {0};
    char line3[64] = {0};
    char line4[64] = {0};

    uint8_t page_count = passive_details_page_count(neighbor);

    passive_build_details_page(
        neighbor,
        app->details_page,
        line1,
        sizeof(line1),
        line2,
        sizeof(line2),
        line3,
        sizeof(line3),
        line4,
        sizeof(line4));

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Neighbor Details");

    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignCenter, FontSecondary, line1);

    widget_add_string_element(app->widget, 64, 35, AlignCenter, AlignCenter, FontSecondary, line2);

    widget_add_string_element(app->widget, 64, 45, AlignCenter, AlignCenter, FontSecondary, line3);

    widget_add_string_element(app->widget, 64, 55, AlignCenter, AlignCenter, FontSecondary, line4);

    char page_text[16];

    snprintf(page_text, sizeof(page_text), "%u/%u", app->details_page + 1, page_count);

    widget_add_string_element(
        app->widget, 120, 5, AlignCenter, AlignCenter, FontSecondary, page_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "<", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, ">", passive_discovery_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void passive_discovery_draw_config(App* app) {
    widget_reset(app->widget);

    char protocol_text[24];

    snprintf(
        protocol_text,
        sizeof(protocol_text),
        "  %s >",
        passive_discovery_module_get_protocol_name(app->passive_discovery.protocol));

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Protocol");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, protocol_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "View", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Start", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Next", passive_discovery_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void passive_discovery_draw_listening(App* app) {
    char neighbors_text[32];

    snprintf(neighbors_text, sizeof(neighbors_text), "Neighbors: %u", app->passive_neighbor_count);

    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Listening...");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, neighbors_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Stop", passive_discovery_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void passive_discovery_draw_clear_confirm(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "Clear All Results?");

    widget_add_string_element(
        app->widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, "Erase LLDP, CDP");

    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignCenter, FontSecondary, "and EAPOL history?");

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Erase", passive_discovery_button_callback, app);
}

static void passive_discovery_refresh(App* app) {
    switch(app->passive_discovery.state) {
    case PassiveDiscoveryStateConfig:
        passive_discovery_draw_config(app);
        break;

    case PassiveDiscoveryStateListening:
        passive_discovery_draw_listening(app);
        break;

    case PassiveDiscoveryStateFinished:
        break;

    case PassiveDiscoveryStateNeighborList:
        show_neighbor_list(app);
        break;

    case PassiveDiscoveryStateNeighborDetails:
        show_neighbor_details(app);
        break;

    case PassiveDiscoveryStateClearConfirm:
        passive_discovery_draw_clear_confirm(app);
        break;
    }
}

void app_scene_passive_discovery_on_enter(void* context) {
    printf("PASSIVE SCENE ENTER\n");
    App* app = context;

    app->passive_discovery.state = PassiveDiscoveryStateConfig;
    app->passive_discovery.protocol = PassiveProtocolALL;
    app->passive_discovery_stop = false;
    neighbor_db_load();

    app->passive_neighbor_count = passive_discovery_module_get_neighbor_count();

    passive_discovery_refresh(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_passive_discovery_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            return true;
        }

        if(app->passive_discovery.state == PassiveDiscoveryStateNeighborDetails) {
            app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

            passive_discovery_refresh(app);

            return true;
        }

        if(app->passive_discovery.state == PassiveDiscoveryStateNeighborList) {
            app->passive_discovery.state = PassiveDiscoveryStateConfig;

            passive_discovery_refresh(app);

            return true;
        }

        return scene_manager_previous_scene(app->scene_manager);
    }

    if(event.event == PassiveDiscoveryStateConfig) {
        widget_reset(app->widget);
        submenu_reset(app->submenu);

        passive_discovery_draw_config(app);

        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
                passive_discovery_refresh(app);
            }

            return true;
        }
    }

    return false;
}

void app_scene_passive_discovery_on_exit(void* context) {
    App* app = context;

    passive_discovery_module_stop(app);

    widget_reset(app->widget);
}

static uint8_t passive_details_page_count(const neighbor_t* neighbor) {
    UNUSED(neighbor);

    return 6;
}

static void passive_build_details_page(
    const neighbor_t* neighbor,
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
        snprintf(line1, line1_size, "No data");
        line2[0] = '\0';
        line3[0] = '\0';
        line4[0] = '\0';
        return;
    }

    /* Limpiar buffers */
    line1[0] = '\0';
    line2[0] = '\0';
    line3[0] = '\0';
    line4[0] = '\0';

    switch(page) {
        /* Página 1/6 - Identidad */
    case 0:
        char sources[32];
        passive_format_sources(neighbor, sources, sizeof(sources));

        snprintf(line1, line1_size, "%s", sources);

        snprintf(line2, line2_size, "%s", neighbor->name[0] ? neighbor->name : "Unnamed");

        snprintf(line3, line3_size, "Chassis");

        snprintf(
            line4, line4_size, "%.20s", neighbor->chassis_id[0] ? neighbor->chassis_id : "N/A");
        break;

    /* Página 2/6 - Red */
    case 1:
        snprintf(line1, line1_size, "Port ID:");
        snprintf(line2, line2_size, "%s", neighbor->port[0] ? neighbor->port : "N/A");

        snprintf(line3, line3_size, "IP Address:");
        snprintf(
            line4,
            line4_size,
            "%s",
            neighbor->management_address[0] ? neighbor->management_address : "N/A");
        break;

        /* Página 3/6 - Capacidades */
    case 2:
        char caps[48];

        passive_format_capabilities(neighbor->capabilities, caps, sizeof(caps));

        snprintf(line1, line1_size, "TTL");
        snprintf(line2, line2_size, "%u", neighbor->ttl);

        snprintf(line3, line3_size, "Capabilities");
        snprintf(line4, line4_size, "%.20s", caps);
        break;

        /* Página 4/6 - Descripción */
    case 3:
        snprintf(line1, line1_size, "Description");

        if(neighbor->description[0]) {
            snprintf(line2, line2_size, "%.20s", neighbor->description);

            if(strlen(neighbor->description) > 20) {
                snprintf(line3, line3_size, "%.20s", neighbor->description + 20);
            }

            if(strlen(neighbor->description) > 40) {
                snprintf(line4, line4_size, "%.20s", neighbor->description + 40);
            }
        } else {
            snprintf(line2, line2_size, "N/A");
        }
        break;

    /* Página 5/6 - VLAN y PoE */
    case 4:
        snprintf(line1, line1_size, "VLAN:");

        if(neighbor->has_vlan) {
            snprintf(line2, line2_size, "%u", neighbor->vlan_id);
        } else {
            snprintf(line2, line2_size, "N/A");
        }

        snprintf(line3, line3_size, "PoE:");

        if(neighbor->has_poe) {
            snprintf(line4, line4_size, "%u mW", neighbor->poe_power_mw);
        } else {
            snprintf(line4, line4_size, "N/A");
        }
        break;

        /* Página 6/6 - 802.1X / EAP */
    case 5:
        snprintf(line1, line1_size, "Identity");

        snprintf(
            line2, line2_size, "%.20s", neighbor->eap_identity[0] ? neighbor->eap_identity : "N/A");

        snprintf(line3, line3_size, "Method");

        switch(neighbor->eap_type) {
        case 1:
            snprintf(line4, line4_size, "EAP Identity");
            break;
        case 25:
            snprintf(line4, line4_size, "PEAP");
            break;
        case 21:
            snprintf(line4, line4_size, "EAP-TTLS");
            break;
        case 13:
            snprintf(line4, line4_size, "EAP-TLS");
            break;
        default:
            snprintf(line4, line4_size, "N/A");
            break;
        }
        break;

    default:
        snprintf(line1, line1_size, "Invalid:");
        break;
    }
}

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;

    if(input_type != InputTypeShort) {
        return;
    }

    switch(type) {
    case GuiButtonTypeLeft:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            if(neighbor_db_count_by_source(passive_scene_get_source(app)) > 0) {
                app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

                show_neighbor_list(app);

            } else {
                widget_reset(app->widget);

                view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
                app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

                show_neighbor_list(app);
            }
        }

        else if(app->passive_discovery.state == PassiveDiscoveryStateNeighborDetails) {
            if(app->details_page > 0) {
                app->details_page--;

                passive_discovery_refresh(app);
            }
        }

        break;

    case GuiButtonTypeRight:
        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            app->passive_discovery.protocol = (app->passive_discovery.protocol + 1) %
                                              passive_discovery_module_get_protocol_count();

            passive_discovery_refresh(app);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateNeighborDetails) {
            neighbor_t* neighbor = neighbor_db_get_by_source(
                passive_scene_get_source(app), app->passive_selected_neighbor);

            uint8_t page_count = passive_details_page_count(neighbor);
            if(app->details_page < page_count - 1) {
                app->details_page++;
            }

            passive_discovery_refresh(app);

            break;
        }

        break;

    case GuiButtonTypeCenter:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            if(app->passive_discovery.protocol == PassiveProtocolClearAll) {
                app->passive_discovery.state = PassiveDiscoveryStateClearConfirm;
                passive_discovery_refresh(app);

            } else {
                passive_discovery_module_start(app);

                app->passive_discovery.state = PassiveDiscoveryStateListening;
                passive_discovery_refresh(app);
            }
        } else if(app->passive_discovery.state == PassiveDiscoveryStateClearConfirm) {
            neighbor_db_clear();
            neighbor_db_save();

            app->passive_neighbor_count = 0;

            app->passive_discovery.state = PassiveDiscoveryStateConfig;
            app->passive_discovery.protocol = PassiveProtocolALL;

            view_dispatcher_send_custom_event(app->view_dispatcher, PassiveDiscoveryStateConfig);

            return;
        } else if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            passive_discovery_module_stop(app);

            app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

            show_neighbor_list(app);
        }

        break;
    }
}
