#include "../app_user.h"
#include "../modules/ping_module.h"
#include "../modules/arp_module.h"

// Ping to by default, it does ping to google
uint8_t ip_ping[4] = {8, 8, 8, 8};

// counter for messages sent
uint16_t messages_sent = 0;

// counter for ping responses
uint16_t ping_responses = 0;

// Function for the thread
int32_t ping_thread(void* context);

/**
 * Ping Menu Scene
 * This scene is to set the IP for the ping option
 * It will show a menu with the options to set the IP
 * and then it will switch to the ping scene
 */

void menu_ping_options_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    if(index == 0) {
        // Switch to the ping scene
        scene_manager_next_scene(app->scene_manager, app_scene_ping_option);
    }

    if(index == 1) {
        // Switch to the ping set IP scene
        scene_manager_next_scene(app->scene_manager, app_scene_ping_set_ip_option);
    }
}

// Function for the testing scene on enter
void app_scene_ping_menu_scene_on_enter(void* context) {
    App* app = (App*)context;

    // reset submenu and switch view
    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "PING TO 8.8.8.8");
    submenu_add_item(app->submenu, "Ping", 0, menu_ping_options_callback, app);

    furi_string_reset(app->text);

    furi_string_cat_printf(
        app->text, "IP to do ping %u:%u:%u:%u", ip_ping[0], ip_ping[1], ip_ping[2], ip_ping[3]);

    submenu_add_item(app->submenu, furi_string_get_cstr(app->text), 1, NULL, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the testing scene on event
bool app_scene_ping_menu_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_ping_menu_scene_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * Ping Set IP Scene
 * This scene is to set the IP for the ping option
 * It will show a byte input view here you will set the IP
 */

void draw_ping_packet_count(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Ping to");

    furi_string_reset(app->text);
    furi_string_cat_printf(
        app->text, "%u:%u:%u:%u", ip_ping[0], ip_ping[1], ip_ping[2], ip_ping[3]);

    widget_add_string_element(
        app->widget,
        64,
        30,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Responses %u of %u", ping_responses, messages_sent);

    widget_add_string_element(
        app->widget,
        64,
        50,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

// Callback for the input byte callback in the ping scene
void input_bytes_for_ip_to_ping_callback(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// Function for the testing scene on enter
void app_scene_ping_set_ip_scene_on_enter(void* context) {
    App* app = (App*)context;

    byte_input_set_header_text(app->input_byte_value, "IP TO PING");
    byte_input_set_result_callback(
        app->input_byte_value, input_bytes_for_ip_to_ping_callback, NULL, app, ip_ping, 4);
    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

// Function for ping scene on event
bool app_scene_ping_set_ip_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for ping scene on exit
void app_scene_ping_set_ip_scene_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * DO THE PING
 * This scene is to test the ping module and the enc28j60
 * It will ping to a default IP (google) and it will show the result
 * of the ping in the screen
 */

// Function for ping scene on enter
void app_scene_ping_scene_on_enter(void* context) {
    App* app = (App*)context;

    furi_thread_suspend(app->thread);

    // Allocate and start the thread
    app->thread_alternative = furi_thread_alloc_ex("PING", 10 * 1024, ping_thread, app);
    furi_thread_start(app->thread_alternative);

    // Reset the widget and switch view
    widget_reset(app->widget);

    // switch to the widget view
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for  ping scene on event
bool app_scene_ping_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 0:
            // Draw the device is not connected
            draw_device_no_connected(app);
            break;
        case 1:
            // Draw the device is connected
            draw_network_not_connected(app);
            break;

        case 2:
            // Draw the device is connected but the process Dora failed
            draw_dora_failed(app);
            break;

        case 5:
            // Draw the ping packet count
            draw_ping_packet_count(app);
            break;

        default:
            break;
        }
    }

    return consumed;
}

// Function for ping scene on exit
void app_scene_ping_scene_on_exit(void* context) {
    App* app = (App*)context;

    // Join and free the thread
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);

    furi_thread_resume(app->thread);
}

/**
 * Thread for the ping scene
 */

int32_t ping_thread(void* context) {
    App* app = (App*)context;

    // Message to send
    char* ping_data = "hello from flipper";

    // data lenght for the ping data
    uint16_t data_len = strlen(ping_data);

    enc28j60_t* ethernet = app->ethernet;
    uint8_t* packet_to_send = ethernet->tx_buffer;
    uint16_t packet_size = 0;

    uint8_t* packet_to_receive = ethernet->rx_buffer;
    uint16_t packet_receive_len = 0;

    bool is_connected = app->enc28j60_connected;

    // Variable to start the process
    bool start_ping = false;

    // Array to get the MAC for the GATEWAY
    uint8_t mac_to_send[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    // reset the counters
    messages_sent = 0;
    ping_responses = 0;

    // sequence
    uint16_t sequence = 1;

    // To know if the enc28j60 is connected
    if(!is_connected) {
        is_connected = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = is_connected; // Update the connection status
    }

    // Get time
    uint32_t last_time_ping = furi_get_tick();

    // Change view to disconnected device
    if(!is_connected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        goto finalize;
    }

    // Get link up to the LAN
    while(((furi_get_tick() - last_time_ping) < 1000) && !start_ping && is_connected) {
        start_ping = is_link_up(ethernet);
    }

    // Change view to network not connected
    if(!start_ping) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        goto finalize;
    }

    // Do process Dora to get the IP gateway, and set our IP if we didnt have the IP
    if(!app->is_static_ip) {
        start_ping = process_dora(ethernet, app->ethernet->ip_address, app->ip_gateway);
    }

    // If the process Dora failed, we will not continue
    if(!start_ping) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 2);
        goto finalize;
    }

    // Get the MAC gateway
    if(!arp_get_specific_mac(
           ethernet, app->ethernet->ip_address, app->ip_gateway, app->mac_gateway) &&
       start_ping && is_connected) {
        start_ping = false;
    } else {
        memcpy(mac_to_send, app->mac_gateway, 6);
    }

    // Here is where gonna make the ping
    while(start_ping && is_connected && furi_hal_gpio_read(&gpio_button_back)) {
        packet_receive_len = receive_packet(ethernet, packet_to_receive, MAX_FRAMELEN);

        if((furi_get_tick() - last_time_ping) > 1000) {
            packet_size = create_flipper_ping_packet(
                packet_to_send,
                mac_to_send,
                app->mac_gateway,
                app->ethernet->ip_address,
                ip_ping,
                1,
                sequence,
                (uint8_t*)ping_data,
                data_len);

            send_packet(ethernet, packet_to_send, packet_size);

            if(sequence == 0xffff) sequence = 0;
            sequence++;
            messages_sent++;
            view_dispatcher_send_custom_event(app->view_dispatcher, 5); // Update the ping count
            last_time_ping = furi_get_tick();
        }

        if(packet_receive_len) {
            if(ping_packet_replied(packet_to_receive, ip_ping)) {
                ping_responses++;
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, 5); // Update the ping count
            }
        }
    }
    furi_delay_ms(1);

finalize:

    return 0;
}
