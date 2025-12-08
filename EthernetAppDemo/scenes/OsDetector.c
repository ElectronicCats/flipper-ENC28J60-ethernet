#include "app_user.h"

#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/tcp.h"

static uint8_t target_ip[4] = {0};
const char* os_texts[] = {"WINDOWS", "LINUX", "IOS/MAC OS"};

typedef enum {
    START,
    TARGET_IP,
} OS_DETECTOR_OPTIONS;

typedef enum {
    PORTS_SCANNER_SCENE_MENU,
    PORTS_SCANNER_SCENE_IP_INPUT,
    PORTS_SCANNER_SCENE_WIDGET,
} OS_DETECTOR_SCENE_STATES;

typedef enum {
    WINDOWS,
    LINUX,
    IOS,
    NO_DETECT,
} OS_DETECTOR_OS;

//  Callback for the Input
void settings_start_ip_address_os_detector(void* context) {
    App* app = (App*)context;
    //scene_manager_previous_scene(app->scene_manager);
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
    app_scene_os_detector_on_enter(app);
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

    uint8_t value = NO_DETECT;

    if(tcp_os_detector(app, target_ip, 100, 80)) {
        ipv4_header_t ipv4_header = ipv4_get_header(app->ethernet->rx_buffer);
        tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);

        uint16_t window_size = 0;
        bytes_to_uint(&window_size, tcp_header.window_size, sizeof(uint16_t));
        printf("LLEGO EL TTL: %u\n", ipv4_header.ttl);
        printf("CON WINDOW SIZE: %u\n", window_size);
        if((ipv4_header.ttl > 64 && ipv4_header.ttl <= 128) &&
           ((window_size >= 8192 && window_size <= 65534) || window_size == 0)) {
            value = WINDOWS;
        } else if(
            (ipv4_header.ttl <= 64) &&
            ((window_size >= 5840 && window_size <= 64240) || window_size == 0)) {
            value = LINUX;
        } else if(ipv4_header.ttl <= 64 && window_size >= 65535) {
            value = IOS;
        }
    }

    return value;
}

void variable_list_os_detector_callback(void* context, uint32_t index) {
    App* app = context;
    UNUSED(app);

    switch(index) {
    case START:
        if(app->is_dora) {
            furi_thread_suspend(app->thread);

            app->thread_alternative =
                furi_thread_alloc_ex("OS Detector", 5 * 1024, os_detector_thread, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            furi_thread_start(app->thread_alternative);

            furi_thread_join(app->thread_alternative);

            uint32_t value = furi_thread_get_return_code(app->thread_alternative);
            if(value == WINDOWS || value == LINUX || value == IOS)
                draw_text(app, os_texts[value]);
            else if(value == NO_DETECT)
                draw_text(app, "NO DETECTED");

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

    variable_item_list_reset(app->varList);

    VariableItem* item;

    // Add the item to scan the network
    item = variable_item_list_add(app->varList, "Detect OS", 0, NULL, app);
    variable_item_set_current_value_text(item, "START");

    // Add item to set the IP address
    if(*(uint32_t*)target_ip == 0) memcpy(target_ip, app->ip_gateway, 4);
    item = variable_item_list_add(app->varList, "Target IP", 0, NULL, app);

    furi_string_reset(app->text); // Reset the text
    furi_string_cat_printf(
        app->text,
        "%u.%u.%u.%u",
        target_ip[0],
        target_ip[1],
        target_ip[2],
        target_ip[3]); // Set the text with the IP address

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->text)); // Set the varible item text

    //Set the callback for the variable item list
    variable_item_list_set_enter_callback(app->varList, variable_list_os_detector_callback, app);

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_os_detector_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        switch(scene_manager_get_scene_state(app->scene_manager, app_scene_os_detector_option)) {
        case PORTS_SCANNER_SCENE_IP_INPUT:
        case PORTS_SCANNER_SCENE_WIDGET:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_MENU);
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
