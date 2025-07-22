/**
 * This files only has the purpose to try code for the functionality
 * of the aplication and ethernet messages
 */

#include "../app_user.h"
#include "../modules/ping_module.h"
#include "../modules/arp_module.h"

// Function for the thread
int32_t testing_thread(void* context);

// Array for gateway IP
uint8_t gateway_ip_testing[4] = {192, 168, 0, 1};

// Function for the testing scene on enter
void app_scene_testing_scene_on_enter(void* context) {
    App* app = (App*)context;

    // Suspend thread
    furi_thread_suspend(furi_thread_get_id(app->thread));

    // Init other thread
    app->thread_alternative =
        furi_thread_alloc_ex("Sniffer Therad", 10 * 1024, testing_thread, app);
    furi_thread_start(app->thread_alternative);

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

    if(event.type == SceneManagerEventTypeCustom) {
        printf(
            "IP ESTATICA: %u.%u.%u.%u\n",
            app->ethernet->ip_address[0],
            app->ethernet->ip_address[1],
            app->ethernet->ip_address[2],
            app->ethernet->ip_address[3]);
    }

    return consumed;
}

// Function for the testing scene on exit
void app_scene_testing_scene_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);

    // Join and free the thread
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
}

/**
 * Functions to test
 */

/**
 * Thread for the Testing Scene
 */

int32_t testing_thread(void* context) {
    App* app = (App*)context;
    UNUSED(app);

    uint32_t timeout = furi_get_tick();

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if((furi_get_tick() - timeout) > 1000) {
            printf("HOLA Desde Thread alternativo\n");
            timeout = furi_get_tick();
        }

        furi_delay_ms(1); // Delay to avoid busy loop
    }

    return 0;
}
