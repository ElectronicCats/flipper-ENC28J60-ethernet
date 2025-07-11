/**
 * This files only has the purpose to try code for the functionality
 * of the aplication and ethernet messages
 */

#include "../app_user.h"
#include "../modules/ping_module.h"
#include "../modules/arp_module.h"

// Function for the thread
int32_t testing_thread(void* context);

// Function for the testing scene on enter
void app_scene_testing_scene_on_enter(void* context) {
    App* app = (App*)context;

    // Allocate and start the thread
    app->thread = furi_thread_alloc_ex("TESTING", 10 * 1024, testing_thread, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    // Draw the in development message
    draw_in_development(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_testing_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_testing_scene_on_exit(void* context) {
    App* app = (App*)context;

    // Join and free the thread
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Functions to test
 */

/**
 * Thread for the Testing Scene
 */

int32_t testing_thread(void* context) {
    App* app = (App*)context;

    // Ping to google
    // uint8_t ip_ping[4] = {8, 8, 8, 8};

    // Message
    // const char* ping_data = "hello from flipper";

    // data len
    // uint16_t data_len = strlen(ping_data);

    enc28j60_t* ethernet = app->ethernet;
    // uint8_t packet_to_send[1500] = {0};

    enc28j60_start(ethernet);

    // Get time
    uint32_t last_time = furi_get_tick();

    // Variable to start the process
    bool start_ping = false;

    // Get link up to the LAN
    while(((furi_get_tick() - last_time) < 1000) && !start_ping) {
        start_ping = is_link_up(ethernet);
    }

    // Array to get the MAC for the GATEWAY
    uint8_t mac_to_send[6] = {0};

    // IP of the gateway
    uint8_t ip_gate_way[4] = {192, 168, 0, 1};

    // Get the MAC gateway
    if(!arp_get_specific_mac(ethernet, app->ip_device, ip_gate_way, mac_to_send) && start_ping) {
        start_ping = false;
    }

    // Here is where gonna make the ping

    UNUSED(app);
    return 0;
}
