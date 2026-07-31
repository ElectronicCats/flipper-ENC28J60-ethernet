#include "../app_user.h"
#include "../libraries/protocol_tools/neighbor_db.h"

static void passive_details_callback(GuiButtonType type, InputType input_type, void* context);

static void mac_to_string(uint8_t* mac, char* buffer, size_t size) {
    snprintf(
        buffer,
        size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

static void passive_format_sources(const neighbor_t* neighbor, char* output, size_t size) {
    if(!neighbor || !output) {
        return;
    }

    bool lldp = neighbor->discovery_sources & NEIGHBOR_SOURCE_LLDP;

    bool cdp = neighbor->discovery_sources & NEIGHBOR_SOURCE_CDP;

    bool eap = neighbor->discovery_sources & NEIGHBOR_SOURCE_EAPOL;

    if(lldp && cdp && eap) {
        snprintf(output, size, "LLDP CDP EAP");

    } else if(lldp && cdp) {
        snprintf(output, size, "LLDP CDP");

    } else if(lldp && eap) {
        snprintf(output, size, "LLDP EAP");

    } else if(cdp && eap) {
        snprintf(output, size, "CDP EAP");

    } else if(lldp) {
        snprintf(output, size, "LLDP");

    } else if(cdp) {
        snprintf(output, size, "CDP");

    } else if(eap) {
        snprintf(output, size, "EAPOL");

    } else {
        snprintf(output, size, "Unknown");
    }
}

static void passive_draw_details(App* app, neighbor_t* neighbor) {
    widget_reset(app->widget);

    char mac_str[20];
    char source_str[32];

    mac_to_string(neighbor->mac, mac_str, sizeof(mac_str));

    passive_format_sources(neighbor, source_str, sizeof(source_str));

    widget_add_string_element(
        app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, "NEIGHBOR DETAILS");

    widget_add_string_element(
        app->widget,
        64,
        20,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        neighbor->name[0] ? neighbor->name : "Unnamed");

    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, mac_str);

    widget_add_string_element(
        app->widget,
        64,
        42,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        neighbor->port[0] ? neighbor->port : "No Port");

    widget_add_string_element(
        app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, source_str);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "INFO", passive_details_callback, app);
}

void app_scene_passive_neighbor_details_on_enter(void* context) {
    App* app = context;

    neighbor_t* neighbor = neighbor_db_get_by_position(app->passive_selected_neighbor);

    if(!neighbor) {
        widget_reset(app->widget);

        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, "Neighbor lost");

        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

        return;
    }

    passive_draw_details(app, neighbor);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void passive_details_callback(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;

    if(input_type != InputTypeShort) {
        return;
    }

    if(type == GuiButtonTypeRight) {
        scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_list_option);

    } else if(type == GuiButtonTypeLeft) {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool app_scene_passive_neighbor_details_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void app_scene_passive_neighbor_details_on_exit(void* context) {
    App* app = context;

    widget_reset(app->widget);
}
