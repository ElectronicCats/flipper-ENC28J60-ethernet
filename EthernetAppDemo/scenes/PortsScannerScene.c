#include "../app_user.h"

#define TARGET_TEXT "Target"
#define RANGE_TEXT  "Range"
#define PORT_TEXT   " Port:"

#define TARGET_PORT_TEXT TARGET_TEXT PORT_TEXT
#define RANGE_PORT_TEXT  RANGE_TEXT PORT_TEXT

uint8_t target_ip[4] = {0};
uint16_t target_port = 22;
uint16_t range_port = 1000;

uint8_t target_port_bytes[2] = {0x00, 0x50};
uint8_t range_port_bytes[2] = {0x00, 0x01};

typedef enum {
    VIEW_IP_LIST,
    TARGET_IP,
    TARGET_PORT,
    SOURCE_PORT,
    PROTOCOL,
    START,
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

void number_input_ports_callback(void* context, int32_t value) {
    App* app = context;

    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, app_scene_ports_scanner_option);

    if(state == TARGET_PORT) {
        target_port = value;
    } else if(state == SOURCE_PORT) {
        range_port = value;
    }

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
    app_scene_ports_scanner_on_enter(app);
}

//  Callback for the Input
void settings_start_ip_address_ports_scanner(void* context) {
    App* app = (App*)context;
    //scene_manager_previous_scene(app->scene_manager);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
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
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

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
        tcp_syn_scan(app, target_ip, target_port, range_port);
        break;

    case PORTS_SCANNER_UDP:
        udp_port_scan(app, target_ip, target_port, range_port);
        break;
    }

    return value;
}

void variable_list_ports_scanner_callback(void* context, uint32_t index) {
    App* app = context;

    switch(index) {
    case VIEW_IP_LIST:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);

        break;

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
    case SOURCE_PORT: {
        int32_t current = (index == TARGET_PORT) ? target_port : range_port;

        number_input_set_header_text(
            app->number_input, index == TARGET_PORT ? "Set Target Port" : "Set Range");

        number_input_set_result_callback(
            app->number_input,
            number_input_ports_callback,
            app,
            current,
            1, // min
            65535 // max
        );

        scene_manager_set_scene_state(app->scene_manager, app_scene_ports_scanner_option, index);

        view_dispatcher_switch_to_view(app->view_dispatcher, NumberInputView);
        break;
    }
    }
}

void variable_item_change_protocol_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, protocols[index]);
    protocols_index = index;
}

void app_scene_ports_scanner_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "PORT SCANNER");

    // VIEW IP LIST
    submenu_add_item(
        app->submenu, "View scanned IPs", VIEW_IP_LIST, variable_list_ports_scanner_callback, app);

    // TARGET IP
    if(*(uint32_t*)target_ip == 0) memcpy(target_ip, app->ip_gateway, 4);

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text,
        "Target IP [%u.%u.%u.%u]",
        target_ip[0],
        target_ip[1],
        target_ip[2],
        target_ip[3]);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        TARGET_IP,
        variable_list_ports_scanner_callback,
        app);

    // TARGET PORT
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Target Port [%u]", target_port);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        TARGET_PORT,
        variable_list_ports_scanner_callback,
        app);

    // RANGE PORT
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Range [%u]", range_port);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        SOURCE_PORT,
        variable_list_ports_scanner_callback,
        app);

    // PROTOCOL
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Protocol [%s]", protocols[protocols_index]);

    submenu_add_item(
        app->submenu,
        furi_string_get_cstr(app->text),
        PROTOCOL,
        variable_list_ports_scanner_callback,
        app);

    // START
    submenu_add_item(
        app->submenu, "Start Scanner", START, variable_list_ports_scanner_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
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
            view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

            consumed = true;

            break;
        }
    }
    return consumed;
}

void app_scene_ports_scanner_on_exit(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->varList);
}
