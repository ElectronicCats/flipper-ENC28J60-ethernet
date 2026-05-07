#include "../app_user.h"
#include <stdio.h>

/**
 * The main menu is the first scene to see in the Ethernet App
 * here the user selects an option that wants to do.
 */

// Time to show the LOGO
const uint32_t time_showing = 1000;

// List for the menu options
// Order follows the natural network-audit flow:
//   setup (Get IP) → discovery (Scan Hosts) → recon (Ping/Ports/OS)
//   → attack (ARP Actions) → capture/analyze (Sniffer/Read Pcaps)
//   → admin (Settings/About).
enum {
    GET_IP_OPTION,
    SCAN_HOSTS_OPTION,
    PING_OPTION,
    PORTS_SCANNER_OPTION,
    OS_DETECTOR_OPTION,
    ARP_ACTIONS_OPTION,
    SNIFFER_OPTION,
    READ_PCAPS_OPTION,
    SETTINGS_OPTION,
    ABOUT_US,
    TESTING_OPTION
} main_menu_options;

// Function to display init at the start of the app
void draw_start(App* app) {
    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignCenter, FontPrimary, APP_NAME);
    widget_add_string_element(
        app->widget, 64, 55, AlignCenter, AlignCenter, FontSecondary, "Electronic Cats");

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

    furi_delay_ms(time_showing);
}

//  Callback for the Options on the main menu
void main_menu_options_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    scene_manager_set_scene_state(app->scene_manager, app_scene_main_menu_option, index);

    switch(index) {
#if DEV_MODE
    case TESTING_OPTION:

        //printf("TEST OPTION\n");

        break;
#endif
    case GET_IP_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_get_ip_option);
        break;

    case SCAN_HOSTS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_menu_option);
        break;

    case SNIFFER_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_sniffer_option);
        break;

    case PING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ping_menu_option);
        break;

    case PORTS_SCANNER_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ports_scanner_option);
        break;

    case OS_DETECTOR_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_os_detector_option);
        break;

    case ARP_ACTIONS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_arp_action_menu_option);
        break;

    case READ_PCAPS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_browser_pcaps_option);
        break;

    case SETTINGS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_settings_option);
        break;

    case ABOUT_US:
        scene_manager_next_scene(app->scene_manager, app_scene_about_us_option);
        break;

    default:
        break;
    }
}

// Function for the main menu on enter
void app_scene_main_menu_on_enter(void* context) {
    App* app = (App*)context;

    // Variable used to show the EC logo once
    static bool is_logo_shown = false;
    if(!is_logo_shown) draw_start(app);

    // F0.4c — the resume-on-entry guard is obsolete: scenes no longer
    // suspend app->thread (rx_dispatch + scanner_session own the chip).

    is_logo_shown = true;

    // Reset Menu
    submenu_reset(app->submenu);

    // header for the  submenu
    submenu_set_header(app->submenu, "ETHERNET FUNCTIONS");

    submenu_add_item(app->submenu, "Get IP", GET_IP_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Scan Hosts", SCAN_HOSTS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Ping", PING_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Ports Scanner", PORTS_SCANNER_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "OS Detector", OS_DETECTOR_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "ARP Actions", ARP_ACTIONS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Sniffer", SNIFFER_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Read Pcaps", READ_PCAPS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Settings", SETTINGS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "About Us", ABOUT_US, main_menu_options_callback, app);

#if DEV_MODE
    submenu_add_item(app->submenu, "...", TESTING_OPTION, main_menu_options_callback, app);
#endif

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

    uint32_t index = scene_manager_get_scene_state(app->scene_manager, app_scene_main_menu_option);
    submenu_set_selected_item(app->submenu, index);
}

// Function for the main menu on event
bool app_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the main menu on exit
void app_scene_main_menu_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
