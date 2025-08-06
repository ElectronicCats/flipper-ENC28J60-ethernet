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

    // comming soon
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget, 64, 25, AlignCenter, AlignCenter, FontPrimary, "COMING\nSOON...");

    widget_add_icon_element(app->widget, 57, 40, &I_icon_for_coming_soon);

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

/* Code for specific functions to integrate*/

/**
 * Thread for the Testing Scene
 */

int32_t testing_thread(void* context) {
    App* app = (App*)context;
    UNUSED(app);
    return 0;
}
