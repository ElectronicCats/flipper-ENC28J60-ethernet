#include "../app_user.h"

void app_scene_main_menu_on_enter(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ETHERNET FUNCTIONS");
    submenu_add_item(app->submenu, "Test Submenu", 0, NULL, NULL);

    submenu_set_selected_item(app->submenu, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

void app_scene_main_menu_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
