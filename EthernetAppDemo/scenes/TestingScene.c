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
    uint8_t ip_ping[4] = {8, 8, 8, 8};

    // Message to send
    char* ping_data = "hello from flipper";

    // data lenght for the ping data
    uint16_t data_len = strlen(ping_data);

    enc28j60_t* ethernet = app->ethernet;
    uint8_t* packet_to_send = ethernet->tx_buffer;
    uint16_t packet_size = 0;

    bool is_connected = enc28j60_start(ethernet) != 0xff;

    // Variable to start the process
    bool start_ping = false;

    // Array to get the MAC for the GATEWAY
    uint8_t mac_to_send[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    // IP of the gateway
    uint8_t ip_gate_way[4] = {192, 168, 0, 1};

    // Get time
    uint32_t last_time = furi_get_tick();

    // Get link up to the LAN
    while(((furi_get_tick() - last_time) < 1000) && !start_ping && is_connected) {
        start_ping = is_link_up(ethernet);
    }

    // Get the MAC gateway
    if(!arp_get_specific_mac(ethernet, app->ip_device, ip_gate_way, mac_to_send) && start_ping &&
       is_connected) {
        start_ping = false;
    }

    // Set the message
    if(start_ping && is_connected) {
        packet_size = create_flipper_ping_packet(
            packet_to_send,
            app->mac_device,
            mac_to_send,
            app->ip_device,
            ip_ping,
            1,
            1,
            (uint8_t*)ping_data,
            data_len);

        last_time = 0;
    }

    // Here is where gonna make the ping
    while(start_ping && is_connected && furi_hal_gpio_read(&gpio_button_back)) {
        if((furi_get_tick() - last_time) > 1000) {
            if(process_ping_response(ethernet, packet_to_send, packet_size, ip_ping)) {
                printf("Se hizo ping\n");
            } else {
                printf("No recibimos PING\n");
            }
            last_time = furi_get_tick();
        }
        furi_delay_ms(1);
    }

    // UNUSED(app);
    return 0;
}
