#include "../app_user.h"
#include "../modules/passive_discovery_module.h"

static neighbor_source_t passive_scene_get_source(App* app) {
    switch(app->passive_discovery.protocol) {
    case PassiveProtocolLLDP:
        return NEIGHBOR_SOURCE_LLDP;

    case PassiveProtocolCDP:
        return NEIGHBOR_SOURCE_CDP;

    case PassiveProtocolEAPOL:
        return NEIGHBOR_SOURCE_EAPOL;

    default:
        return 0;
    }
}

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

    FURI_LOG_I("PASSIVE", "DB count = %u", passive_discovery_module_get_neighbor_count());

    size_t count = neighbor_db_count_by_source(passive_scene_get_source(app));

    for(size_t i = 0; i < count; i++) {
        neighbor_t* neighbor = neighbor_db_get_by_source(passive_scene_get_source(app), i);

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

    uint8_t page_count =
        passive_discovery_module_get_details_page_count(app->passive_discovery.protocol, neighbor);

    passive_discovery_module_build_details_page(
        app->passive_discovery.protocol,
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
    widget_add_string_element(
        app->widget, 64, 60, AlignCenter, AlignCenter, FontSecondary, "↑ View Neighbors");
    widget_reset(app->widget);

    char protocol_text[24];

    snprintf(
        protocol_text,
        sizeof(protocol_text),
        "< %s >",
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

    app->passive_discovery.state = PassiveDiscoveryStateConfig;
    app->passive_discovery.protocol = PassiveProtocolLLDP;
    app->passive_discovery_stop = false;
    neighbor_db_load();

    app->passive_neighbor_count = passive_discovery_module_get_neighbor_count();

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
            passive_discovery_module_stop(app);
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

    passive_discovery_module_stop(app);

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
            if(passive_discovery_module_get_neighbor_count() > 0) {
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
            app->passive_discovery.protocol = (app->passive_discovery.protocol + 1) %
                                              passive_discovery_module_get_protocol_count();

            passive_discovery_refresh(app);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateNeighborDetails) {
            uint8_t page_count = passive_discovery_module_get_details_page_count(
                app->passive_discovery.protocol,
                passive_discovery_module_get_neighbor(app->passive_selected_neighbor));
            if(app->details_page < page_count - 1) {
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

            passive_discovery_module_start(app);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            passive_discovery_module_stop(app);

            app->passive_discovery.state = PassiveDiscoveryStateNeighborList;

            show_neighbor_list(app);
        }

    default:
        break;
    }
}
