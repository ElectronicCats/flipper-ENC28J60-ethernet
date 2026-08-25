#include "app_user.h"
#include "../modules/os_detector_module.h"

static const char* os_texts[] = {"WINDOWS", "LINUX", "IOS/MAC OS", "NO DETECTED"};

typedef enum {
    VIEW_RESULTS,
    TARGET_IP,
    START,
} OS_DETECTOR_OPTIONS;

typedef enum {
    OS_DETECTOR_SCENE_MENU,
    OS_DETECTOR_SCENE_IP_INPUT,
    OS_DETECTOR_SCENE_WIDGET,
} OS_DETECTOR_SCENE_STATES;

typedef enum {
    OsDetectorEventFinished = 1,
} OsDetectorCustomEvent;

void settings_start_ip_address_os_detector(void* context) {
    App* app = context;
    app->selected_menu_index = TARGET_IP;
    scene_manager_set_scene_state(
        app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_MENU);
    app_scene_os_detector_on_enter(app);
}

void set_ip_address_os_detector(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address_os_detector, app);
    ip_assigner_set_ip_array(app->ip_assigner, app->scan_params.target_ip);
    view_dispatcher_switch_to_view(app->view_dispatcher, IpAssignerView);
}

static int32_t os_detector_thread(void* context) {
    App* app = context;
    int32_t result = os_scan(app, app->scan_params.target_ip);
    view_dispatcher_send_custom_event(app->view_dispatcher, OsDetectorEventFinished);
    return result;
}

static void os_detector_draw_results(App* app, uint32_t value) {
    if(value > NO_DETECTED) value = NO_DETECTED;

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "   OS DETECTION RESULTS\n\n");
    furi_string_cat_printf(
        app->text,
        "   Target IP: %u.%u.%u.%u\n",
        app->scan_params.target_ip[0],
        app->scan_params.target_ip[1],
        app->scan_params.target_ip[2],
        app->scan_params.target_ip[3]);

    if(value == NO_DETECTED) {
        furi_string_cat_printf(app->text, "   OS Not Detected\n\n");
    } else if(app->os_guess) {
        furi_string_cat_printf(app->text, "   Guessed OS: %s\n\n", os_texts[value]);
    } else {
        furi_string_cat_printf(app->text, "   Detected OS: %s\n\n", os_texts[value]);
    }

    furi_string_cat_printf(app->text, "   *Experimental Feature*\n");
    furi_string_cat_printf(app->text, "   (Heuristic may be wrong)\n");
    furi_string_cat_printf(app->text, "\n   Initial Source Port:\n   %u\n", app->src_port);
    furi_string_cat_printf(app->text, "\n   Scanned Ports:\n");

    for(uint8_t i = 0; i < app->ports_count && i < MAX_OS_SCAN_PORTS; i++) {
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
            break;
        }
        furi_string_cat_printf(app->text, "   %u : %s\n", app->ports[i].port, state);
    }

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->text));
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void os_detector_start(App* app) {
    if(!app->is_dora) {
        draw_dora_needed(app);
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_WIDGET);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        return;
    }

    bool started = app->enc28j60_connected;
    if(!started) {
        started = enc28j60_start(app->ethernet) != 0xff;
        app->enc28j60_connected = started;
    }
    if(!started) {
        draw_device_no_connected(app);
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_WIDGET);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        return;
    }
    if(!is_link_up(app->ethernet)) {
        draw_network_not_connected(app);
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_WIDGET);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        return;
    }

    app->os_detector_stop = false;
    FuriThread* thread = furi_thread_alloc_ex("Detect OS", 5 * 1024, os_detector_thread, app);
    if(!app_thread_claim(app, AppThreadOwnerOsDetector, thread)) return;

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_WIDGET);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);
    furi_thread_start(thread);
}

void variable_list_os_detector_callback(void* context, uint32_t index) {
    App* app = context;
    app->selected_menu_index = index;

    switch(index) {
    case VIEW_RESULTS:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);
        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
        break;
    case TARGET_IP:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_IP_INPUT);
        set_ip_address_os_detector(app);
        break;
    case START:
        os_detector_start(app);
        break;
    }
}

void app_scene_os_detector_on_enter(void* context) {
    App* app = context;
    arp_load_last_scan(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "DETECT OS");

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "Hosts [%02d|%02d|%02d-%02d:%02d]",
        app->last_scan_time.month,
        app->last_scan_time.day,
        app->last_scan_time.year % 100,
        app->last_scan_time.hour,
        app->last_scan_time.minute);
    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        VIEW_RESULTS,
        variable_list_os_detector_callback,
        app);

    if(*(uint32_t*)app->scan_params.target_ip == 0) {
        memcpy(app->scan_params.target_ip, app->ip_gateway, 4);
    }
    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "Target IP [%u.%u.%u.%u]",
        app->scan_params.target_ip[0],
        app->scan_params.target_ip[1],
        app->scan_params.target_ip[2],
        app->scan_params.target_ip[3]);
    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        TARGET_IP,
        variable_list_os_detector_callback,
        app);
    submenu_add_item(
        app->submenu, "Start Detection", START, variable_list_os_detector_callback, app);
    submenu_set_selected_item(app->submenu, app->selected_menu_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_os_detector_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == OsDetectorEventFinished &&
       app_thread_is_owned(app, AppThreadOwnerOsDetector)) {
        uint32_t value = app_thread_join_and_free(app, AppThreadOwnerOsDetector);
        if(app->os_detector_stop) {
            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_MENU);
            app_scene_os_detector_on_enter(app);
        } else {
            os_detector_draw_results(app, value);
        }
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        uint32_t state =
            scene_manager_get_scene_state(app->scene_manager, app_scene_os_detector_option);
        if(state == OS_DETECTOR_SCENE_WIDGET) {
            if(app_thread_is_owned(app, AppThreadOwnerOsDetector)) {
                app->os_detector_stop = true;
            } else {
                scene_manager_set_scene_state(
                    app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_MENU);
                app_scene_os_detector_on_enter(app);
            }
            return true;
        }
        if(state == OS_DETECTOR_SCENE_IP_INPUT) {
            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, OS_DETECTOR_SCENE_MENU);
            app_scene_os_detector_on_enter(app);
            return true;
        }
    }

    return false;
}

void app_scene_os_detector_on_exit(void* context) {
    App* app = context;
    if(app_thread_is_owned(app, AppThreadOwnerOsDetector)) {
        app->os_detector_stop = true;
        app_thread_join_and_free(app, AppThreadOwnerOsDetector);
    }
}
