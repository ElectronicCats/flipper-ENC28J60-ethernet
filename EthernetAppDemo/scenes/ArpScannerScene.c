#include "../app_user.h"

/**
 * This file contains the functions to display and work with the ARP SCANNER
 * It shows the IP list and saves it in a array
 */

/**
 * Menu To select the options and range of the Scanner
 */

// Callback for to enter when the variable item is pressed
void variable_list_enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

// function for the arp menu scene on enter
void app_scene_arp_scanner_menu_on_enter(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->varList);

    VariableItem* item;

    UNUSED(item);

    item = variable_item_list_add(app->varList, "Start Scanner", 0, NULL, app);
    variable_item_list_set_enter_callback(app->varList, variable_list_enter_callback, app);
}

// function for the arp menu scene on event
bool app_scene_arp_scanner_menu_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// function for the arp menu scene on exit
void app_scene_arp_scanner_menu_on_exit(void* context) {
    App* app = (App*)context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Scene to show the list of the IP in the network
 */

// Variables for the
uint8_t ip_start[4] = {192, 168, 0, 3};
uint8_t total_ip = 0;

// Function for the thread
int32_t arp_scanner_thread(void* context);

// Function to set the thread and the view
void draw_the_arp_list(App* app) {
    app->thread = furi_thread_alloc_ex("ARP SCANNER", 10 * 1024, arp_scanner_thread, app);
    furi_thread_start(app->thread);

    // Switch the view of the flipper
    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function to draw to finished the thread
void finished_arp_thread(App* app) {
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

// function on enter for the arp scanner scene
void app_scene_arp_scanner_on_enter(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option)) {
    case 0:
        draw_the_arp_list(app);
        break;

    default:
        break;
    }
}

// function on event for the arp scanner scene
bool app_scene_arp_scanner_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// function on exit for the arp scanner scene
void app_scene_arp_scanner_on_exit(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option)) {
    case 0:
        finished_arp_thread(app);
        break;

    default:
        break;
    }
}

/**
 * Thread for the ARP Scanner
 */

int32_t arp_scanner_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    bool start = enc28j60_start(ethernet) != 0xff; // To know if the enc28j60 is connected

    if(!start) {
        draw_device_no_connected(app);
    }

    if(!is_the_network_connected(ethernet) && start) {
        draw_network_not_connected(app);
        start = false;
    }

    if(start) {
        arp_scan_network(
            ethernet,
            app->ip_list,
            app->mac_device,
            app->ip_device,
            ip_start,
            &total_ip,
            app->count);

        submenu_reset(app->submenu);
        view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

        submenu_set_header(app->submenu, "");

        for(uint8_t i = 0; i < total_ip; i++) {
            furi_string_reset(app->text);
            furi_string_cat_printf(
                app->text,
                "%u.%u.%u.%u",
                app->ip_list[i].ip[0],
                app->ip_list[i].ip[1],
                app->ip_list[i].ip[2],
                app->ip_list[i].ip[3]);
            submenu_add_item(app->submenu, furi_string_get_cstr(app->text), i, NULL, NULL);
        }
    }

    return 0;
}
