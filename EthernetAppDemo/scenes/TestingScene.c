/**
 * This files only has the purpose to try code for the functionality
 * of the aplication and ethernet messages
 */

#include "../app_user.h"

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
    UNUSED(app);
    return 0;
}
