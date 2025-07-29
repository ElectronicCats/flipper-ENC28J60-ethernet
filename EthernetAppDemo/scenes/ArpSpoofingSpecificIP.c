#include "../app_user.h"

/**
 * Scene for the menu to select some options in the arp spoofing to an specific IP
 */

void arp_spoofing_menu_to_ip_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case 0:
        /* code */
        break;

    case 1:
        scene_manager_set_scene_state(app->scene_manager, app_scene_arp_scanner_menu_option, 2);
        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_menu_option);
        break;

    case 2:
        /* code */
        break;

    default:
        break;
    }
}

// Function for the menu arp spoofing scene on enter
void app_scene_arp_spoofing_specific_ip_menu_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);

    furi_string_reset(app->text);

    furi_string_printf(
        app->text,
        "Attack IP [%u.%u.%u.%u]",
        app->ip_helper[0],
        app->ip_helper[1],
        app->ip_helper[2],
        app->ip_helper[3]);

    submenu_set_header(app->submenu, "ARP Spoofing To IP");

    // Option to run the ARP spoofing IP
    submenu_add_item(
        app->submenu, furi_string_get_cstr(app->text), 0, arp_spoofing_menu_to_ip_callback, app);

    // Option Scan an IP
    submenu_add_item(app->submenu, "Scan for IP", 1, arp_spoofing_menu_to_ip_callback, app);

    // Option set an IP by manual
    submenu_add_item(app->submenu, "Set IP manually", 2, arp_spoofing_menu_to_ip_callback, app);

    // switch view to menu
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the menu arp spoofing scene on event
bool app_scene_arp_spoofing_specific_ip_menu_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the menu arp spoofing scene on exit
void app_scene_arp_spoofing_specific_ip_menu_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}
