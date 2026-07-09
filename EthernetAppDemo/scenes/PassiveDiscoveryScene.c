#include "../app_user.h"
#include "../modules/lldp_module.h"

typedef struct {
    App* app;
    volatile bool stopped;
} passive_discovery_thread_state_t;

static const char* passive_protocol_names[] = {
    "LLDP",
    "EAPOL",
    "CDP",
};

int32_t passive_discovery_thread(void* context);

static App* passive_discovery_app = NULL;

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context);

static void passive_discovery_refresh(App* app);
static void show_neighbor_list(App* app);

static void passive_neighbor_callback(void* context, uint32_t index) {
    App* app = context;

    app->passive_selected_neighbor = index;
    app->details_page = 0;

    app->passive_discovery.state = PassiveDiscoveryStateNeighborDetails;

    passive_discovery_refresh(app);
}

static void build_neighbor_submenu(App* app) {
    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "DISCOVERED DEVICES");

    FURI_LOG_I("PASSIVE", "DB count = %u", neighbor_db_count());

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        neighbor_t* neighbor = neighbor_db_get(i);

        FURI_LOG_I("PASSIVE", "slot=%u ptr=%p", i, neighbor);

        if(!neighbor) {
            continue;
        }

        FURI_LOG_I("PASSIVE", "occupied=%u name='%s'", neighbor->occupied, neighbor->name);

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

        FURI_LOG_I("PASSIVE", "ADDING '%s'", furi_string_get_cstr(app->text));

        submenu_add_item(
            app->submenu, furi_string_get_cstr(app->text), i, passive_neighbor_callback, app);
    }
}

static void show_neighbor_list(App* app) {
    build_neighbor_submenu(app);

    submenu_set_selected_item(app->submenu, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

static void show_neighbor_details(App* app) {
    neighbor_t* neighbor = neighbor_db_get(app->passive_selected_neighbor);

    widget_reset(app->widget);

    if(!neighbor) {
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No data");

        return;
    }

    char line1[64];
    char line2[64];
    char line3[64];
    char line4[64];

    const char* chassis_type = "Unknown";

    switch(neighbor->chassis_subtype) {
    case LLDP_CHASSIS_MAC_ADDRESS:
        chassis_type = "MAC";
        break;

    case LLDP_CHASSIS_NETWORK_ADDRESS:
        chassis_type = "IPv4";
        break;

    case LLDP_CHASSIS_INTERFACE_NAME:
        chassis_type = "IfName";
        break;

    case LLDP_CHASSIS_INTERFACE_ALIAS:
        chassis_type = "Alias";
        break;

    case LLDP_CHASSIS_LOCAL:
        chassis_type = "Local";
        break;

    case LLDP_CHASSIS_COMPONENT:
        chassis_type = "Component";
        break;

    case LLDP_CHASSIS_PORT_COMPONENT:
        chassis_type = "Port";
        break;
    }

    switch(app->details_page) {
    case 0:

        snprintf(line1, sizeof(line1), "Name:");

        snprintf(line2, sizeof(line2), "%s", neighbor->name[0] ? neighbor->name : "Unknown");

        if(neighbor->name[0]) {
            snprintf(line2, sizeof(line2), "%s", neighbor->name);
        } else {
            snprintf(
                line2,
                sizeof(line2),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                neighbor->mac[0],
                neighbor->mac[1],
                neighbor->mac[2],
                neighbor->mac[3],
                neighbor->mac[4],
                neighbor->mac[5]);
        }

        snprintf(line3, sizeof(line3), "Chassis(%s)", chassis_type);

        snprintf(
            line4,
            sizeof(line4),
            "%.20s",
            neighbor->chassis_id[0] ? neighbor->chassis_id : "Unknown");

        break;

    case 1:

        snprintf(line1, sizeof(line1), "Port: %.20s", neighbor->port[0] ? neighbor->port : "N/A");

        snprintf(
            line2,
            sizeof(line2),
            "IP: %s",
            neighbor->management_address[0] ? neighbor->management_address : "N/A");

        snprintf(line3, sizeof(line3), "TTL: %u", neighbor->ttl);

        snprintf(line4, sizeof(line4), "CAP: 0x%04X", neighbor->capabilities);

        break;

    case 2:

        snprintf(line1, sizeof(line1), "Description");

        snprintf(line2, sizeof(line2), "%.21s", neighbor->description);

        snprintf(line3, sizeof(line3), "%.21s", neighbor->description + 21);

        snprintf(line4, sizeof(line4), "%.21s", neighbor->description + 42);

        //snprintf(line1, sizeof(line1), "SOURCE: LLDP");

        break;

    default:
        break;
    }

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Neighbor Details");

    widget_add_string_element(app->widget, 64, 25, AlignCenter, AlignCenter, FontSecondary, line1);

    widget_add_string_element(app->widget, 64, 35, AlignCenter, AlignCenter, FontSecondary, line2);

    widget_add_string_element(app->widget, 64, 45, AlignCenter, AlignCenter, FontSecondary, line3);

    widget_add_string_element(app->widget, 64, 55, AlignCenter, AlignCenter, FontSecondary, line4);

    char page_text[16];

    snprintf(page_text, sizeof(page_text), "%u/3", app->details_page + 1);

    widget_add_string_element(
        app->widget, 120, 5, AlignCenter, AlignCenter, FontSecondary, page_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "<", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, ">", passive_discovery_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void passive_discovery_draw_config(App* app) {
    widget_add_string_element(
        app->widget, 64, 60, AlignCenter, AlignCenter, FontSecondary, "↑ View Neighbors");
    widget_reset(app->widget);

    char protocol_text[24];

    snprintf(
        protocol_text,
        sizeof(protocol_text),
        "< %s >",
        passive_protocol_names[app->passive_discovery.protocol]);

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
        app->widget, GuiButtonTypeRight, "Other", passive_discovery_button_callback, app);

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
    }
}

void app_scene_passive_discovery_on_enter(void* context) {
    printf("PASSIVE SCENE ENTER\n");
    App* app = context;

    passive_discovery_app = app;

    app->passive_discovery.state = PassiveDiscoveryStateConfig;
    app->passive_discovery.protocol = PassiveProtocolLLDP;
    app->passive_discovery_stop = false;
    app->passive_neighbor_count = 0;

    passive_discovery_refresh(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_passive_discovery_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
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

        if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            app->passive_discovery_stop = true;

            if(app->thread_alternative) {
                furi_thread_join(app->thread_alternative);

                furi_thread_free(app->thread_alternative);

                app->thread_alternative = NULL;
            }
        }

        return scene_manager_previous_scene(app->scene_manager);
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

    app->passive_discovery_stop = true;

    if(app->thread_alternative) {
        furi_thread_join(app->thread_alternative);

        furi_thread_free(app->thread_alternative);

        app->thread_alternative = NULL;
    }

    widget_reset(app->widget);
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
            if(neighbor_db_count() > 0) {
                app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

                show_neighbor_list(app);

            } else {
                widget_reset(app->widget);

                widget_add_string_element(
                    app->widget, 64, 30, AlignCenter, AlignCenter, FontPrimary, "No neighbors");

                widget_add_button_element(
                    app->widget, GuiButtonTypeCenter, "OK", passive_discovery_button_callback, app);

                view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
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
            app->passive_discovery.protocol =
                (app->passive_discovery.protocol + 1) % PassiveProtocolCount;

            passive_discovery_refresh(app);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateNeighborDetails) {
            if(app->details_page < 2) {
                app->details_page++;
            }

            passive_discovery_refresh(app);

            break;
        }

        break;

    case GuiButtonTypeCenter:
        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            app->passive_discovery.state = PassiveDiscoveryStateListening;

            FURI_LOG_I("PASSIVE", "Entering LISTENING");

            passive_discovery_refresh(app);

            app->thread_alternative =
                furi_thread_alloc_ex("Passive Discovery", 4096, passive_discovery_thread, app);

            furi_thread_start(app->thread_alternative);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            app->passive_discovery_stop = true;

            if(app->thread_alternative) {
                furi_thread_join(app->thread_alternative);

                furi_thread_free(app->thread_alternative);

                app->thread_alternative = NULL;
            }

            app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

            show_neighbor_list(app);
        }

    default:
        break;
    }
}

int32_t passive_discovery_thread(void* context) {
    printf("PASSIVE THREAD ENTERED\n");
    App* app = context;
    enc28j60_t* ethernet = app->ethernet;

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff;
        app->enc28j60_connected = start;
    }

    if(!start) {
        draw_device_no_connected(app);
        furi_delay_ms(1500);
        return 0;
    }

    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        furi_delay_ms(1500);
        return 0;
    }

    rx_dispatch_init(app);

    scanner_session_t session;

    scanner_session_init(&session, app);

    switch(app->passive_discovery.protocol) {
    case PassiveProtocolLLDP:
        enable_multicast(ethernet);
        lldp_module_init();
        break;

    case PassiveProtocolEAPOL:
        break;

    case PassiveProtocolCDP:
        break;

    default:
        break;
    }

    while(!app->passive_discovery_stop) {
        FURI_LOG_I("PASSIVE", "Protocol=%u", app->passive_discovery.protocol);
        switch(app->passive_discovery.protocol) {
        case PassiveProtocolLLDP:

            FURI_LOG_I("PASSIVE", "Calling lldp_module_run");

            bool result = lldp_module_run(&session, 500);

            FURI_LOG_I("PASSIVE", "lldp_module_run returned %d", result);

            if(result) {
                FURI_LOG_I("PASSIVE", "LLDP packet processed");
            }

            uint16_t count = neighbor_db_count();

            if(count != app->passive_neighbor_count) {
                app->passive_neighbor_count = count;
                view_dispatcher_send_custom_event(app->view_dispatcher, 1);
            }

            break;

        case PassiveProtocolEAPOL:
            break;

        case PassiveProtocolCDP:
            break;

        default:
            break;
        }
    }

    return 0;
}
