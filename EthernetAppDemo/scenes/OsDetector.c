#include "app_user.h"
#include "../modules/os_detector_module.h"

static uint8_t target_ip[4] = {0};
const char* os_texts[] = {"WINDOWS", "LINUX", "IOS/MAC OS", "NO DETECTED"};

typedef enum {
    VIEW_RESULTS,
    TARGET_IP,
    START,
} OS_DETECTOR_OPTIONS;

typedef enum {
    PORTS_SCANNER_SCENE_MENU,
    PORTS_SCANNER_SCENE_IP_INPUT,
    PORTS_SCANNER_SCENE_WIDGET,
} OS_DETECTOR_SCENE_STATES;

//  Callback for the Input
void settings_start_ip_address_os_detector(void* context) {
    App* app = (App*)context;

    app->selected_menu_index = TARGET_IP;

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_MENU);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// Function to set the IP address
void set_ip_address_os_detector(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address_os_detector, app);
    ip_assigner_set_ip_array(app->ip_assigner, target_ip);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, IpAssignerView); // Switch to the input byte view
}

int32_t os_detector_thread(void* context) {
    App* app = context;

    return os_scan(app, target_ip);
}

void variable_list_os_detector_callback(void* context, uint32_t index) {
    App* app = context;
    UNUSED(app);

    app->selected_menu_index = index;

    switch(index) {
    case VIEW_RESULTS:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
        break;

    case START:
        if(app->is_dora) {
            furi_thread_suspend(app->thread);

            app->thread_alternative =
                furi_thread_alloc_ex("OS Detector", 5 * 1024, os_detector_thread, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            furi_thread_start(app->thread_alternative);
            furi_thread_join(app->thread_alternative);

            uint32_t value = furi_thread_get_return_code(app->thread_alternative);

            furi_string_reset(app->text);

            furi_string_cat_printf(app->text, "OS DETECTOR RESULT:\n\n");

            furi_string_cat_printf(
                app->text,
                "Target: %u.%u.%u.%u\n\n",
                target_ip[0],
                target_ip[1],
                target_ip[2],
                target_ip[3]);

            if(value == NO_DETECTED) {
                furi_string_cat_printf(app->text, "*OS NOT DETECTED*\n");

            } else if(app->os_guess) {
                furi_string_cat_printf(app->text, "OS Guessed:%s\n", os_texts[value]);

            } else {
                furi_string_cat_printf(app->text, "OS Detected:%s\n", os_texts[value]);
            }

            furi_string_cat_printf(app->text, "\nBase Source Port:\n%u\n", app->src_port);

            furi_string_cat_printf(app->text, "\nPorts Scanned:\n");

            for(uint8_t i = 0; i < app->ports_count && i < 11; i++) {
                const char* state = "UNKNOWN";

                switch(app->ports[i].state) {
                case PORT_OPEN:
                    state = "OPEN";
                    break;

                case PORT_CLOSED:
                    state = "CLOSED";
                    break;

                case PORT_FILTERED:
                    state = "FILTERED";
                    break;

                case PORT_UNKNOWN:
                default:
                    state = "UNKNOWN";
                    break;
                }

                furi_string_cat_printf(app->text, "%u : %s\n", app->ports[i].port, state);
            }

            widget_reset(app->widget);

            widget_add_text_scroll_element(
                app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->text));

            view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

            furi_thread_free(app->thread_alternative);

            furi_thread_resume(app->thread);
        } else {
            draw_dora_needed(app);
        }

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_WIDGET);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        break;

    case TARGET_IP:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_IP_INPUT);

        set_ip_address_os_detector(app);
        break;
    }
}

void app_scene_os_detector_on_enter(void* context) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_os_detector_option);

    if(state == PORTS_SCANNER_SCENE_IP_INPUT) {
        set_ip_address_os_detector(app);
        return;
    }

    if(state == PORTS_SCANNER_SCENE_WIDGET) {
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        return;
    }

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_MENU);

    variable_item_list_reset(app->varList);

    VariableItem* item;

    item = variable_item_list_add(app->varList, "View scanned IPs", 0, NULL, app);

    if(*(uint32_t*)target_ip == 0) memcpy(target_ip, app->ip_gateway, 4);

    item = variable_item_list_add(app->varList, "Target IP", 0, NULL, app);

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text, "%u.%u.%u.%u", target_ip[0], target_ip[1], target_ip[2], target_ip[3]);

    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));

    item = variable_item_list_add(app->varList, "Detect OS", 0, NULL, app);
    variable_item_set_current_value_text(item, "START");

    variable_item_list_set_enter_callback(app->varList, variable_list_os_detector_callback, app);

    variable_item_list_set_selected_item(app->varList, app->selected_menu_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_os_detector_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        switch(scene_manager_get_scene_state(app->scene_manager, app_scene_os_detector_option)) {
        case PORTS_SCANNER_SCENE_WIDGET:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_MENU);
            view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);

            consumed = true;

            break;

        case PORTS_SCANNER_SCENE_IP_INPUT:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_MENU);

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
