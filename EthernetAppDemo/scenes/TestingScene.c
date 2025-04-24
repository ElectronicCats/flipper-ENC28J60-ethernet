/**
 * This files only has the purpose to try code for the functionality
 * of the aplication and ethernet messages
 */

#include "../app_user.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/generals/ethernet_generals.h"

#include "../modules/capture_module.h"

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

    enc28j60_t* ethernet = app->ethernet;

    pcap_capture_init(app->file, "Hola mundo");

    // Try to start the ENC28J60
    bool start = enc28j60_start(ethernet) != 0xff;

    if(!start) {
        // Device not connected
        printf("No conectado\n");
    }

    if(!is_the_network_connected(ethernet) && start) {
        // Network not connected
        printf("La red no esta conectada\n");
        start = false;
    }

    // Main loop
    while(start && furi_hal_gpio_read(&gpio_button_back)) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            furi_delay_ms(500);
        }
        furi_delay_ms(1); // Small delay to prevent CPU hogging
    }

    return 0;
}
