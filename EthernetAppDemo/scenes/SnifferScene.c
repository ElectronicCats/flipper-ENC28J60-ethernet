#include "../app_user.h"

// Thread for the sniffer
int32_t sniffer_thread(void* context);

// Function for the testing scene on enter
void app_scene_sniffer_on_enter(void* context) {
    App* app = (App*)context;

    // Allocate and start the thread
    app->thread = furi_thread_alloc_ex("TESTING", 10 * 1024, sniffer_thread, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_sniffer_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_sniffer_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);

    // Join and free the thread
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

int32_t sniffer_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    bool start = enc28j60_start(ethernet) != 0xff;

    if(!start) {
    }

    if(start) {
        // Wait until the red is connected
        while(!is_link_up(ethernet)) {
            if(!furi_hal_gpio_read(&gpio_button_back)) {
                start = false;
                break;
            }
        }
    }

    // Starts
    while(start && furi_hal_gpio_read(&gpio_button_back)) {
        furi_delay_ms(1);
    }

    return 0;
}
