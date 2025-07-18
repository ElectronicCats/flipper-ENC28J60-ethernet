#include "../app_user.h"

void app_scene_test_addon_scene_on_enter(void* context) {
    furi_assert(context);
    App* app = (App*)context;

    UNUSED(app);
}

bool app_scene_test_addon_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    App* app = (App*)context;
    bool consumed = false;

    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void app_scene_test_addon_scene_on_exit(void* context) {
    UNUSED(context);
}