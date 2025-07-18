#include "app_user.h"

static bool app_scene_costum_callback(void* context, uint32_t costum_event) {
    furi_assert(context);
    App* app = (App*)context;
    return scene_manager_handle_custom_event(app->scene_manager, costum_event);
}

static bool app_scene_back_event(void* context) {
    furi_assert(context);
    App* app = (App*)context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void app_tick_event(void* context) {
    furi_assert(context);
    App* app = (App*)context;
    UNUSED(app);
}

// Alloc app instance
App* app_alloc() {
    App* app = malloc(sizeof(App));

    app->scene_manager = scene_manager_alloc(&app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_scene_costum_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_scene_back_event);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, app_tick_event, 100);

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, WidgetView, widget_get_view(app->widget));

    app->text = furi_string_alloc();
    app->thread = furi_thread_alloc();

    return app;
}

void free_app(App* app) {
    view_dispatcher_remove_view(app->view_dispatcher, WidgetView);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    widget_free(app->widget);

    furi_string_free(app->text);

    free(app);
}

int32_t testing_app_app(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    UNUSED(app);
    return 0;
}
