#include "../app_user.h"

/**
 * Scene for the menu to select some options in the arp spoofing to an specific IP
 */

void arp_spoofing_menu_to_ip_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case 0:
    case 2:
        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_spoofing_specific_ip_option, index);
        scene_manager_next_scene(app->scene_manager, app_scene_arp_spoofing_specific_ip_option);
        break;

    case 1:
        scene_manager_set_scene_state(app->scene_manager, app_scene_arp_scanner_menu_option, 2);
        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_menu_option);
        break;

    default:
        break;
    }
}

// Function for the menu arp spoofing scene on enter
void app_scene_arp_spoofing_specific_ip_menu_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);

    furi_string_reset(app->text);

    furi_string_printf(
        app->text,
        "Attack IP [%u.%u.%u.%u]",
        app->ip_helper[0],
        app->ip_helper[1],
        app->ip_helper[2],
        app->ip_helper[3]);

    submenu_set_header(app->submenu, "ARP Spoofing To IP");

    // Option to run the ARP spoofing IP
    submenu_add_item(
        app->submenu, furi_string_get_cstr(app->text), 0, arp_spoofing_menu_to_ip_callback, app);

    // Option Scan an IP
    submenu_add_item(app->submenu, "Scan for IP", 1, arp_spoofing_menu_to_ip_callback, app);

    // Option set an IP by manual
    submenu_add_item(app->submenu, "Set IP manually", 2, arp_spoofing_menu_to_ip_callback, app);

    // switch view to menu
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the menu arp spoofing scene on event
bool app_scene_arp_spoofing_specific_ip_menu_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the menu arp spoofing scene on exit
void app_scene_arp_spoofing_specific_ip_menu_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * Scene for the spoofing IP
 */

// Thread for worker
int32_t thread_for_spoofing_specific_ip(void* context);

// Callback to set the IP manually
void set_ip_to_spoof_manually(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// to set the ip
void set_ip_to_spoof(App* app) {
    // Set the header text
    byte_input_set_header_text(app->input_byte_value, "Set IP Address");

    byte_input_set_result_callback(
        app->input_byte_value,
        set_ip_to_spoof_manually,
        NULL,
        app,
        app->ip_helper,
        4); // Set the callback for the input IP address

    view_dispatcher_switch_to_view(
        app->view_dispatcher, InputByteView); // Switch to the input byte view
}

// Set the spoofing and the scene and start the alternative thread
void spoofing_specific_ip(App* app) {
    furi_thread_suspend(furi_thread_get_id(app->thread));

    // Start the other thread
    app->thread_alternative = furi_thread_alloc_ex(
        "ARP SPOOF SPECIFIC IP", 10 * 1024, thread_for_spoofing_specific_ip, app);
    furi_thread_start(app->thread_alternative);

    // Switch the view of the flipper
    widget_reset(app->widget);

    // By the moment in development
    draw_in_development(app);

    // switch to widget
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// When the thread is exit, resume the other thread
void finish_spoofing_specific_ip_thread(App* app) {
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
    furi_thread_resume(furi_thread_get_id(app->thread));
}

// Scene on enter for spoofing
void app_scene_arp_spoofing_specific_ip_on_enter(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(
        app->scene_manager, app_scene_arp_spoofing_specific_ip_option)) {
    case 0:
        spoofing_specific_ip(app);
        break;

    case 2:
        // Set the ip manually
        set_ip_to_spoof(app);
        break;

    default:
        break;
    }
}

// Function for the spoofing scene on event
bool app_scene_arp_spoofing_specific_ip_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the arp spoofing scene on exit
void app_scene_arp_spoofing_specific_ip_on_exit(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(
        app->scene_manager, app_scene_arp_spoofing_specific_ip_option)) {
    case 0:
        finish_spoofing_specific_ip_thread(app);
        break;

    default:
        break;
    }
}

/**
 * Thread for the arp spoofing specific IP
 */

int32_t thread_for_spoofing_specific_ip(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    // frames to send
    // uint8_t buffer_to_gateway[MAX_FRAMELEN] = {0}; // Frame to send to the gateway
    // uint8_t buffer_to_ip[MAX_FRAMELEN] = {0}; // Frame to send to the specific IP

    // Just an alternative IP
    uint8_t ip_alternative[4] = {192, 168, 0, 1};

    // for mac handle
    uint8_t last_mac[6] = {0}; // To save the last mac

    // Save last mac
    memcpy(last_mac, ethernet->mac_address, 6);

    // generate a random mac
    generate_random_mac(ethernet->mac_address);

    bool attack = false; // variable for the attacking

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = start; // Update the connection status
    }

    bool program_loop = start; // This variable will help for the loop

    if(!is_the_network_connected(ethernet) && start) {
        // draw_network_not_connected(app);
        printf("Network not connected\n");
        program_loop = false;
    }

    // draw the waiting attack
    if(program_loop) {
        // Set the mac in the chip
        enc28j60_set_mac(ethernet);

        // Get the ip gateway and then it mac address
        process_dora(ethernet, ip_alternative, app->ip_gateway);

        // then get mac gateway
        arp_get_specific_mac(ethernet, ip_alternative, app->ip_gateway, app->mac_gateway);
    }

    while(furi_hal_gpio_read(&gpio_button_back) && program_loop) {
        // Waiting to read the gpio ok to attack or stop to attack
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            attack = !attack;
            furi_delay_ms(200);
        }

        // When the attacks start
        if(attack) {
        }

        furi_delay_ms(1);
    }

    // If the device is not connected
    if(!start) {
        // draw_device_no_connected(app);
        printf("Device not connected\n");
    }

    return 0;
}
