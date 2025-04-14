#include "../app_user.h"

/**
 * In this C file is where be the scenes for the Settings Option
 * for example to set the IP via manually or set the IP with the
 * DORA Process, you can set the MAC Address Via manually or
 * generate randomly the MAC Address
 */

enum {
    MAC_OPTION_SETTING,
    IP_OPTION_SETTING
} Options;

// Function of the settings scene on enter
void app_scene_settings_on_enter(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);

    // Set the submenu
    submenu_set_header(app->submenu, "DEVICE SETTINGS");

    // Get the text for the label
    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "MAC [%02x:%02x:%02x:%02x:%02x:%02x]",
        app->mac_device[0],
        app->mac_device[1],
        app->mac_device[2],
        app->mac_device[3],
        app->mac_device[4],
        app->mac_device[5]);

    // Add the Item to set the MAC address of the device
    submenu_add_item(app->submenu, furi_string_get_cstr(app->text), MAC_OPTION_SETTING, NULL, app);

    // Get the text for the label
    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "IP [%u:%u:%u:%0u]",
        app->ip_device[0],
        app->ip_device[1],
        app->ip_device[2],
        app->ip_device[3]);

    // Add the Item to set the IP address of the device
    submenu_add_item(app->submenu, furi_string_get_cstr(app->text), IP_OPTION_SETTING, NULL, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the settings scene on event
bool app_scene_settings_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the settings on exit
void app_scene_settings_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
