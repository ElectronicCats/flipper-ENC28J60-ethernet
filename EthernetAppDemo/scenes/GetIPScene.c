#include "../app_user.h"

uint8_t router_ip[4] = {192, 168, 0, 1};

// Function for the thread
int32_t get_ip_thread(void* context);

// Function for the testing scene on enter
void app_scene_get_ip_scene_on_enter(void* context) {
    App* app = (App*)context;

    // Allocate and start the thread
    app->thread = furi_thread_alloc_ex("TESTING", 10 * 1024, get_ip_thread, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_get_ip_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_get_ip_scene_on_exit(void* context) {
    App* app = (App*)context;

    // Join and free the thread
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Thread for to get IP
 */

int32_t get_ip_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    bool ip_gotten = false;

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = start; // Update the connection status
    }

    if(!start) {
        draw_device_no_connected(app); // Display if the device is not connected
    }

    if(!is_the_network_connected(ethernet) && start) {
        draw_network_not_connected(app); // Display if the network is not connected
        start = false;
    }

    if(start) {
        draw_waiting_for_ip(app);
        furi_delay_ms(10);
        ip_gotten =
            process_dora(ethernet, app->ethernet->ip_address, router_ip); // start the process DORA
    }

    if(ip_gotten && start) {
        draw_your_ip_is(app);
    }

    if(!ip_gotten && start) {
        draw_ip_not_got_it(app);
    }

    return 0;
}
