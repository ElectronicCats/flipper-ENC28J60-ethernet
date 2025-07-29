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

    UNUSED(app);
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
