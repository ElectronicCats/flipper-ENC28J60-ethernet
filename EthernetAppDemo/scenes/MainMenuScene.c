#include "../app_user.h"

/**
 * The main menu is the first scene to see in the Ethernet App
 * here the user selects an option that wants to do.
 */

// Time to show the LOGO
const uint32_t time_showing = 1000;

// List for the menu options
enum {
    TESTING_OPTION,
    SNIFFING_OPTION,
    READ_PCAPS_OPTION,
    ARP_ACTIONS_OPTION,
    PING_OPTION,
    SETTINGS_OPTION,
    ABOUT_US
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

    switch(index) {
    case TESTING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_testing_scene_option);
        break;

    case SNIFFING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_sniffer_option);
        break;

    case READ_PCAPS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_browser_pcaps_option);
        break;

    case ARP_ACTIONS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_arp_action_menu_option);
        break;

    case PING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ping_menu_option);
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

    if(furi_thread_is_suspended(furi_thread_get_id(app->thread))) {
        furi_thread_resume(furi_thread_get_id(app->thread));
    }

    is_logo_shown = true;

    // Reset Menu
    submenu_reset(app->submenu);

    // header for the  submenu
    submenu_set_header(app->submenu, "ETHERNET FUNCTIONS");

    // submenu_add_item(app->submenu, "Option 1", TESTING_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Sniffer", SNIFFING_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Read Pcaps", READ_PCAPS_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "ARP Actions", ARP_ACTIONS_OPTION, main_menu_options_callback, app);

    // By the moment the option for the Do ping will be commented because it still on development
    // submenu_add_item(app->submenu, "Do Ping", PING_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Settings", SETTINGS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "About Us", ABOUT_US, main_menu_options_callback, app);

    submenu_set_selected_item(app->submenu, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
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
