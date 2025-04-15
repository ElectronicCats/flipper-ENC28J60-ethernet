#include "../app_user.h"

/**
 * In this C file is where be the scenes for the Settings Option
 * for example to set the IP via manually or set the IP with the
 * DORA Process, you can set the MAC Address Via manually or
 * generate randomly the MAC Address
 */

// Enumeration
enum {
    MAC_OPTION_SETTING,
    IP_OPTION_SETTING
} Options;

// Callback for the options
void app_scene_settings_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case MAC_OPTION_SETTING:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_settings_options_menu_option, MAC_OPTION_SETTING);
        scene_manager_next_scene(app->scene_manager, app_scene_settings_options_menu_option);
        break;

    case IP_OPTION_SETTING:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_settings_options_menu_option, IP_OPTION_SETTING);
        scene_manager_next_scene(app->scene_manager, app_scene_settings_options_menu_option);
        break;

    default:
        break;
    }
}

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
    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        MAC_OPTION_SETTING,
        app_scene_settings_callback,
        app);

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
    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        IP_OPTION_SETTING,
        app_scene_settings_callback,
        app);

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

/**
 * Functions to draw the options for MAC and IP address
 */

// Function to draw the menu in the MAC options
void draw_menu_for_MAC_settings(App* app) {
    submenu_reset(app->submenu);

    // Menu Header
    submenu_set_header(app->submenu, "SET MAC ADDRESS");

    // Generate a Random MAC
    submenu_add_item(app->submenu, "Set Random MAC", 0, NULL, app);

    // Set the MAC via manually
    submenu_add_item(app->submenu, "Set MAC manually", 1, NULL, app);
}

// Function to draw the menu in the IP options
void draw_menu_for_IP_settings(App* app) {
    submenu_reset(app->submenu);

    // Menu Header
    submenu_set_header(app->submenu, "SET IP ADDRESS");

    // Generate a Random MAC
    submenu_add_item(app->submenu, "Get IP", 0, NULL, app);

    // Set the MAC via manually
    submenu_add_item(app->submenu, "Set IP manually", 1, NULL, app);
}

// Settings option menu on enter
void app_scene_settings_options_menu_on_enter(void* context) {
    App* app = (App*)context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_settings_options_menu_option);

    switch(state) {
    case MAC_OPTION_SETTING:
        draw_menu_for_MAC_settings(app);
        break;

    case IP_OPTION_SETTING:
        draw_menu_for_IP_settings(app);
        break;

    default:
        break;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Settings option menu on event
bool app_scene_settings_options_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Settings option menu on exit
void app_scene_settings_options_menu_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
