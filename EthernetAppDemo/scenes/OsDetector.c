#include "app_user.h"

#include "../modules/os_detector_module.h"

static uint8_t target_ip[4] = {0};

static const char* os_texts[OS_TYPE_COUNT] = {
    [OS_WINDOWS] = "WINDOWS",
    [OS_LINUX] = "LINUX",
    [OS_MACOS] = "IOS/MAC OS",
    [OS_FREEBSD] = "FREEBSD",
    [OS_ANDROID] = "ANDROID",
    [OS_NETWORK_DEVICE] = "NETWORK DEVICE",
    [OS_UNKNOWN] = "NO DETECTED",
};

typedef enum {
    START,
    TARGET_IP,
} OS_DETECTOR_OPTIONS;

typedef enum {
    OS_DETECTOR_SCENE_MENU,
    OS_DETECTOR_SCENE_IP_INPUT,
    OS_DETECTOR_SCENE_WIDGET,
} OS_DETECTOR_SCENE_STATES;

/* Thread-local storage for the scan result (returned via thread return code). */
static OsResult scan_result;

void settings_start_ip_address_os_detector(void* context) {
    App* app = (App*)context;
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
    app_scene_os_detector_on_enter(app);
}

void set_ip_address_os_detector(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address_os_detector, app);
    ip_assigner_set_ip_array(app->ip_assigner, target_ip);

    view_dispatcher_switch_to_view(app->view_dispatcher, IpAssignerView);
}

int32_t os_detector_thread(void* context) {
    App* app = context;

    os_scan(app, target_ip, &scan_result);

    return (int32_t)scan_result.type;
}

void variable_list_os_detector_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case START:
        if(app->is_dora) {
            furi_thread_suspend(app->thread);

            app->thread_alternative =
                furi_thread_alloc_ex("OS Detector", 10 * 1024, os_detector_thread, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            furi_thread_start(app->thread_alternative);
            furi_thread_join(app->thread_alternative);

            uint32_t value = furi_thread_get_return_code(app->thread_alternative);
            if(value >= OS_TYPE_COUNT) value = OS_UNKNOWN;
            draw_text(app, os_texts[value]);

            furi_thread_free(app->thread_alternative);

            furi_thread_resume(app->thread);
        } else {
            draw_dora_needed(app);
        }

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_WIDGET);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        break;

    case TARGET_IP:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_IP_INPUT);
        set_ip_address_os_detector(app);
        break;
    }
}

void app_scene_os_detector_on_enter(void* context) {
    App* app = context;

    variable_item_list_reset(app->varList);

    VariableItem* item;

    item = variable_item_list_add(app->varList, "Detect OS", 0, NULL, app);
    variable_item_set_current_value_text(item, "START");

    if(*(uint32_t*)target_ip == 0) memcpy(target_ip, app->ip_gateway, 4);
    item = variable_item_list_add(app->varList, "Target IP", 0, NULL, app);

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text, "%u.%u.%u.%u", target_ip[0], target_ip[1], target_ip[2], target_ip[3]);

    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    variable_item_list_set_enter_callback(app->varList, variable_list_os_detector_callback, app);

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_MENU);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_os_detector_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        switch(scene_manager_get_scene_state(app->scene_manager, app_scene_os_detector_option)) {
        case OS_DETECTOR_SCENE_IP_INPUT:
        case OS_DETECTOR_SCENE_WIDGET:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_MENU);
            app_scene_os_detector_on_enter(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);

            consumed = true;

            break;
        }
    }
    return consumed;
}

void app_scene_os_detector_on_exit(void* context) {
    UNUSED(context);
}
