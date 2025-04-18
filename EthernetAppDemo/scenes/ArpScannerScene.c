#include "../app_user.h"

/**
 * This file contains the functions to display and work with the ARP SCANNER
 * It shows the IP list and saves it in a array
 */

// Variables for the
uint8_t ip_start[4] = {192, 168, 0, 3};
uint8_t total_ip = 0;

// Function for the thread
int32_t arp_scanner_thread(void* context);

// function on enter for the arp scanner scene
void app_scene_arp_scanner_on_enter(void* context) {
    App* app = (App*)context;
    app->thread = furi_thread_alloc_ex("ARP SCANNER", 10 * 1024, arp_scanner_thread, app);
    furi_thread_start(app->thread);

    // Switch the view of the flipper
    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
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
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
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

    // while(furi_hal_gpio_read(&gpio_button_back) && start){
    //     furi_delay_ms()
    // }

    return 0;
}
