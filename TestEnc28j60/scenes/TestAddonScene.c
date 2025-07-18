#include "../app_user.h"

// counter for messages sent
uint16_t messages_sent = 0;

// counter for ping responses
uint16_t ping_responses = 0;

// MAC for the device
uint8_t mac_device[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d};

// IP for the device
uint8_t ip_device[4] = {192, 168, 0, 2};

// Ping to by default, it does ping to google
uint8_t router_ip[4] = {192, 168, 0, 1};

//  Thread to test the enc28j60
int32_t testing_thread(void* context);

void app_scene_test_addon_scene_on_enter(void* context) {
    furi_assert(context);
    App* app = (App*)context;

    app->thread = furi_thread_alloc_ex("PING Thread", 10 * 1024, testing_thread, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    // switch to the widget view
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_test_addon_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    App* app = (App*)context;
    bool consumed = false;

    UNUSED(event);
    UNUSED(app);

    return consumed;
}

void app_scene_test_addon_scene_on_exit(void* context) {
    App* app = (App*)context;

    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

// Thread to test functions
int32_t testing_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = enc28j60_alloc(mac_device);

    bool ip_gotten = false;

    bool start = enc28j60_start(ethernet) != 0xff; // Start the enc28j60

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
        ip_gotten = process_dora(ethernet, ip_device, router_ip); // start the process DORA
    }

    if(ip_gotten && start) {
        draw_your_ip_is(app, ip_device);
    }

    if(!ip_gotten && start) {
        draw_ip_not_got_it(app, ip_device);
    }

    return 0;
}
