#include "../app_user.h"

/**
 * In this C file is where be the scenes for the Settings Option
 * for example to set the IP via manually or set the IP with the
 * DORA Process, you can set the MAC Address Via manually or
 * generate randomly the MAC Address
 */

/**
 * Function to generate a RANDOM MAC
 */

void generate_random_mac(uint8_t* mac) {
    // Generate random bytes for the MAC address
    for(int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)furi_hal_random_get() & 0xFF;
    }

    // Set the locally administered bit (bit 1) and clear the multicast bit (bit 0)
    // This ensures the MAC address is valid for local use and is unicast
    mac[0] = (mac[0] & 0xFC) | 0x02;
}

// Enumeration
enum {
    MAC_OPTION_SETTING,
    IP_OPTION_SETTING
} Options;

// Callback for the options
void app_scene_settings_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_settings_options_menu_option, index);
    scene_manager_set_scene_state(app->scene_manager, app_scene_set_address_option, index);
    scene_manager_next_scene(app->scene_manager, app_scene_settings_options_menu_option);
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
        app->ethernet->mac_address[0],
        app->ethernet->mac_address[1],
        app->ethernet->mac_address[2],
        app->ethernet->mac_address[3],
        app->ethernet->mac_address[4],
        app->ethernet->mac_address[5]);

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
        app->ethernet->ip_address[0],
        app->ethernet->ip_address[1],
        app->ethernet->ip_address[2],
        app->ethernet->ip_address[3]);

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

// Callback to set a random MAC address
void callback_random_mac(void* context, uint32_t index) {
    App* app = (App*)context;

    UNUSED(index);

    generate_random_mac(app->ethernet->mac_address);
    scene_manager_previous_scene(app->scene_manager);
}

// Callback to get the IP in the network
void callback_get_ip(void* context, uint32_t index) {
    App* app = (App*)context;

    UNUSED(index);

    scene_manager_next_scene(app->scene_manager, app_scene_get_ip_option);
}

// Set the address depending of the scene state
void set_address_callback(void* context, uint32_t index) {
    App* app = (App*)context;
    UNUSED(index);
    scene_manager_next_scene(app->scene_manager, app_scene_set_address_option);
}

// Function to draw the menu in the MAC options
void draw_menu_for_MAC_settings(App* app) {
    submenu_reset(app->submenu);

    // Menu Header
    submenu_set_header(app->submenu, "SET MAC ADDRESS");

    // Generate a Random MAC
    submenu_add_item(app->submenu, "Set Random MAC", 0, callback_random_mac, app);

    // Set the MAC via manually
    submenu_add_item(app->submenu, "Set MAC manually", 1, set_address_callback, app);
}

// Function to draw the menu in the IP options
void draw_menu_for_IP_settings(App* app) {
    submenu_reset(app->submenu);

    // Menu Header
    submenu_set_header(app->submenu, "SET IP ADDRESS");

    // Generate a Random MAC
    submenu_add_item(app->submenu, "Get IP", 0, callback_get_ip, app);

    // Set the MAC via manually
    submenu_add_item(app->submenu, "Set IP manually", 1, set_address_callback, app);
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

/**
 * Functions to set an Address
 */

//  Callback for the Input
void settings_input_byte_address(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// Function to set MAC address
void mac_set_address_view(App* app) {
    byte_input_set_header_text(app->input_byte_value, "MAC ADDRESS");
    byte_input_set_result_callback(
        app->input_byte_value,
        settings_input_byte_address,
        NULL,
        app,
        app->ethernet->mac_address,
        6);
}

// Function to set the IP address
void ip_set_address_view(App* app) {
    byte_input_set_header_text(app->input_byte_value, "IP ADDRESS");
    byte_input_set_result_callback(
        app->input_byte_value,
        settings_input_byte_address,
        NULL,
        app,
        app->ethernet->ip_address,
        4);
}

// Function on enter when the user needs to set the IP or the MAC address
void app_scene_set_address_on_enter(void* context) {
    App* app = (App*)context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_set_address_option);

    if(state == MAC_OPTION_SETTING) mac_set_address_view(app);
    if(state == IP_OPTION_SETTING) ip_set_address_view(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

// Function on event when the user needs to set the IP or the MAC address
bool app_scene_set_address_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function on exit when the user needs to set the IP or the MAC address
void app_scene_set_address_on_exit(void* context) {
    UNUSED(context);
}
