#include "../app_user.h"

/**
 * In this C file is where be the scenes for the Settings Option
 * for example to set the IP via manually or set the IP with the
 * DORA Process, you can set the MAC Address Via manually or
 * generate randomly the MAC Address
 */

// Function of the settings scene on enter
void app_scene_settings_on_enter(void* context) {
    App* app = (App*)context;
    UNUSED(app);
}

// Function for the settings scene on event
bool app_scene_settings_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the settings on exit
void app_scene_settings_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);
}
