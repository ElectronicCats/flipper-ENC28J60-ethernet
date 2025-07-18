#include "../app_user.h"

void app_scene_start_scene_on_enter(void* context) {
    furi_assert(context);
    App* app = (App*)context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Welcome to the Test App!");
}

bool app_scene_start_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    App* app = (App*)context;
    bool consumed = false;

    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void app_scene_start_scene_on_exit(void* context) {
    UNUSED(context);
}