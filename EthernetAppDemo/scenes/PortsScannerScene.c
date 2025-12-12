#include "../app_user.h"

#define TARGET_TEXT "Target"
#define RANGE_TEXT  "Range"
#define PORT_TEXT   " Port:"

#define TARGET_PORT_TEXT TARGET_TEXT PORT_TEXT
#define RANGE_PORT_TEXT  RANGE_TEXT PORT_TEXT

FuriString* text;

uint8_t target_ip[4] = {0};
uint16_t target_port = 80;
uint16_t range_port = 1;

uint8_t target_port_bytes[2] = {0x00, 0x50};
uint8_t range_port_bytes[2] = {0x00, 0x01};

typedef enum {
    PORT_OPEN,
    PORT_CLOSED,
} PORTS_SCANNER_STATE;

typedef enum {
    START,
    TARGET_IP,
    TARGET_PORT,
    SOURCE_PORT,
} PORTS_SCANNER_OPTIONS;

typedef enum {
    PORTS_SCANNER_TCP,
    PORTS_SCANNER_UDP,
} PORTS_SCANNER_PROTOCOLS;

typedef enum {
    PORTS_SCANNER_SCENE_MENU,
    PORTS_SCANNER_SCENE_BYTE_INPUT,
    PORTS_SCANNER_SCENE_IP_INPUT,
    PORTS_SCANNER_SCENE_WIDGET,
    PORTS_SCANNER_SCENE_SHOW_PORTS,
} PORTS_SCANNER_SCENE_STATES;

const char* protocols[] = {"TCP", "UDP"};
uint8_t protocols_index = PORTS_SCANNER_TCP;

//  Callback for the Input
void settings_start_ip_address_ports_scanner(void* context) {
    App* app = (App*)context;
    //scene_manager_previous_scene(app->scene_manager);
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
    app_scene_ports_scanner_on_enter(app);
}

// Function to set the IP address
void set_ip_address_ports_scanner(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address_ports_scanner, app);
    ip_assigner_set_ip_array(app->ip_assigner, target_ip);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, IpAssignerView); // Switch to the input byte view
}

void byte_input_ports_scanner_callback(void* context) {
    App* app = context;

    bytes_to_uint(&range_port, range_port_bytes, sizeof(uint16_t));
    bytes_to_uint(&target_port, target_port_bytes, sizeof(uint16_t));

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);

    app_scene_ports_scanner_on_enter(app);
}

void byte_change_ports_scanner(void* context) {
    UNUSED(context);
}

int32_t ports_scanner_thread(void* context) {
    App* app = context;

    uint8_t value = PORT_CLOSED;

    switch(protocols_index) {
    case PORTS_SCANNER_TCP:
        //value = tcp_handshake_process(app, target_ip, source_port, target_port) ? PORT_OPEN :
        //                                                                          PORT_CLOSED;
        tcp_syn_scan(app, target_ip, target_port, range_port);
        break;

    case PORTS_SCANNER_UDP:
        value = udp_check_port(
                    app,
                    app->ethernet->mac_address,
                    app->ethernet->ip_address,
                    target_ip,
                    range_port,
                    target_port) ?
                    PORT_OPEN :
                    PORT_CLOSED;
        break;
    }

    return value;
}

void variable_list_ports_scanner_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case START:

        if(app->is_dora) {
            furi_thread_suspend(app->thread);

            submenu_reset(app->submenu);
            submenu_set_header(app->submenu, "PORTS OPEN");

            app->thread_alternative =
                furi_thread_alloc_ex("Ports Sacanner", 5 * 1024, ports_scanner_thread, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            //furi_thread_set_priority(app->thread_alternative, FuriThreadPriorityNormal);

            furi_thread_start(app->thread_alternative);

            //furi_thread_join(app->thread_alternative);

            /*uint32_t value = furi_thread_get_return_code(app->thread_alternative);
            if(value == PORT_OPEN)
                draw_port_open(app);
            else if(value == PORT_CLOSED)
                draw_port_not_open(app);
            */

            //furi_thread_free(app->thread_alternative);

            //furi_thread_resume(app->thread);

            scene_manager_set_scene_state(
                app->scene_manager,
                app_scene_ports_scanner_option,
                PORTS_SCANNER_SCENE_SHOW_PORTS);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

        } else {
            draw_dora_needed(app);

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_WIDGET);
            view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        }

        //scene_manager_set_scene_state(
        //    app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_WIDGET);
        //view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

        break;

    case TARGET_IP:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_IP_INPUT);
        set_ip_address_ports_scanner(app);

        break;

    case TARGET_PORT:
    case SOURCE_PORT:

        furi_string_cat_printf(
            text, "%s", index == TARGET_PORT ? TARGET_PORT_TEXT : RANGE_PORT_TEXT);

        uint_to_bytes(&range_port, range_port_bytes, sizeof(uint16_t));
        uint_to_bytes(&target_port, target_port_bytes, sizeof(uint16_t));

        byte_input_set_header_text(app->input_byte_value, furi_string_get_cstr(text));
        byte_input_set_result_callback(
            app->input_byte_value,
            byte_input_ports_scanner_callback,
            byte_change_ports_scanner,
            app,
            index == TARGET_PORT ? target_port_bytes : range_port_bytes,
            2);

        furi_string_reset(text);

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_BYTE_INPUT);
        view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);

        break;
    }
}

void variable_item_change_protocol_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, protocols[index]);
    protocols_index = index;
}

void app_scene_ports_scanner_on_enter(void* context) {
    App* app = (App*)context;

    text = furi_string_alloc();
    variable_item_list_reset(app->varList);

    VariableItem* item;

    // Add the item to scan the network
    item = variable_item_list_add(app->varList, "Start Scanner", 0, NULL, app);
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

    // Add item to set the range of the target port
    item = variable_item_list_add(app->varList, TARGET_PORT_TEXT, 0, NULL, &target_port);

    furi_string_reset(app->text); // Reset the text
    furi_string_cat_printf(
        app->text, "%u", target_port); // Set the text with the total number of IP addresses

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->text)); // Set the varible item text

    // Add item to set the range of the source port
    item = variable_item_list_add(app->varList, RANGE_PORT_TEXT, 0, NULL, &range_port);

    furi_string_reset(app->text); // Reset the text
    furi_string_cat_printf(
        app->text, "%u", range_port); // Set the text with the total number of IP addresses

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->text)); // Set the varible item text

    item = variable_item_list_add(
        app->varList, "Protocol", 2, variable_item_change_protocol_callback, NULL);
    variable_item_set_current_value_index(item, protocols_index);
    variable_item_set_current_value_text(item, protocols[protocols_index]);

    //Set the callback for the variable item list
    variable_item_list_set_enter_callback(app->varList, variable_list_ports_scanner_callback, app);

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);

    // Switch to the variable list view
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_ports_scanner_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        switch(scene_manager_get_scene_state(app->scene_manager, app_scene_ports_scanner_option)) {
        case PORTS_SCANNER_SCENE_SHOW_PORTS:
            furi_thread_join(app->thread_alternative);
            furi_thread_free(app->thread_alternative);
            furi_thread_resume(app->thread);
            /* fall through */
        case PORTS_SCANNER_SCENE_BYTE_INPUT:
        case PORTS_SCANNER_SCENE_IP_INPUT:
        case PORTS_SCANNER_SCENE_WIDGET:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);
            app_scene_ports_scanner_on_enter(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);

            consumed = true;

            break;
        }
    }
    return consumed;
}

void app_scene_ports_scanner_on_exit(void* context) {
    App* app = (App*)context;
    furi_string_reset(text);
    text = NULL;
    variable_item_list_reset(app->varList);
}
