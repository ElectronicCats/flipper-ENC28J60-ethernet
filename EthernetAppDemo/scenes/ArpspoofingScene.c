#include "../app_user.h"

/**
 * In this file you will find the ARP Spoofing scene and it worker,
 * for the usage of I played with the view dispatcher and the thread to show different
 * views for not use many scenes and avoid to define many
 */

// The Gateway IP
uint8_t gateway_ip[4] = {192, 168, 0, 1};

// ArpSpoofing thread
int32_t arpspoofing_thread(void* context);

// ArpSpoofing on enter
void app_scene_arp_spoofing_on_enter(void* context) {
    App* app = (App*)context;
    app->thread = furi_thread_alloc_ex("ArpSpoofing", 10 * 1024, arpspoofing_thread, app);
    furi_thread_start(app->thread);
}

// ArpSpoofing on event
bool app_scene_arp_spoofing_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// ArpSpoofing on exit
void app_scene_arp_spoofing_on_exit(void* context) {
    App* app = (App*)context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Some views for the ArpSpoofing
 */

// Function to draw if the user wants to set the IP
void draw_ask_for_ip(App* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        64,
        0,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Do you want to continue with\nthe same IP\nor\ndo you want to get\nit from the network?");

    widget_add_button_element(app->widget, GuiButtonTypeLeft, "No", NULL, app);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Yes", NULL, app);
}

// Function to draw for waiting to attack
void draw_waiting_start_attack(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 40, 0, &I_SleepyCat50x50);
    // widget_add_string_element(
    //     app->widget, 64, 80, AlignCenter, AlignCenter, FontSecondary, "Start Attack??");
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Attack??", NULL, app);
}

// Function to draw when is waiting for the IP of the DORA process
void draw_waiting_for_ip(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting for IP");
}

// Function to draw when a Ip is got it
void draw_your_ip_is(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Your IP is: ");

    furi_string_reset(app->text);

    furi_string_cat_printf(
        app->text,
        "%u:%u:%u:%u",
        app->ip_device[0],
        app->ip_device[1],
        app->ip_device[2],
        app->ip_device[3]);

    widget_add_string_element(
        app->widget,
        64,
        40,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

// Function to draw when you are attacking
void draw_arpspoofing_attacking(App* app, uint8_t frame) {
    widget_reset(app->widget);

    switch(frame) {
    case 0:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation00);
        break;

    case 1:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation01);
        break;

    case 2:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation02);
        break;

    case 3:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation03);
        break;

    case 4:
        widget_add_icon_element(app->widget, 40, 0, &I_CatAttackingAnimation04);
        break;

    default:
        break;
    }

    widget_add_button_element(app->widget, GuiButtonTypeCenter, "STOP", NULL, app);
}

/**
 * Main Thread for the ArpSpoofing
 * Here is where the main code comes for the worker
 */
int32_t arpspoofing_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;
    uint8_t buffer[1500] = {0};
    uint16_t size = 0;

    uint32_t last_time = 0;

    uint8_t count_frame = 0;

    widget_reset(app->widget); // Reset the widget
    view_dispatcher_switch_to_view(
        app->view_dispatcher, WidgetView); // Switch view for the view dispatcher

    bool start = enc28j60_start(ethernet) != 0xff; // To know if the enc28j60 is connected
    bool program_loop = start; // This variable will help for the loop

    bool attack = false; // variable for the attacking

    bool show_once = true; // To display a view once

    if(!is_the_network_connected(ethernet) && start) {
        draw_network_not_connected(app);
        program_loop = false;
    }

    // This condition works to get the IP
    if(program_loop) {
        // Draw to ask for a IP
        draw_ask_for_ip(app);

        // Loop to wait the button
        while(true) {
            // If the user wants to get back
            if(!furi_hal_gpio_read(&gpio_button_back)) {
                program_loop = false;
                break;
            }

            // Get the IP
            if(!furi_hal_gpio_read(&gpio_button_right)) {
                draw_waiting_for_ip(app);
                process_dora(ethernet, app->ip_device, gateway_ip);
                break;
            }

            // Not get the IP
            if(!furi_hal_gpio_read(&gpio_button_left)) break;

            furi_delay_ms(1);
        }
    }

    // draw the waiting attack
    if(program_loop) {
        draw_your_ip_is(app);
        furi_delay_ms(1000);
        set_arp_message_for_attack_all(buffer, app->mac_device, gateway_ip, &size);
        last_time = furi_get_tick();
    }

    while(furi_hal_gpio_read(&gpio_button_back) && program_loop) {
        // Waiting to read the gpio ok to attack or stop to attack
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            attack = !attack;
            if(!attack) show_once = true;
            furi_delay_ms(200);
        }

        // To show when it is waiting to attack
        if(!attack && show_once) {
            draw_waiting_start_attack(app);
            show_once = false;
            count_frame = 0;
        }

        // When the attacks start
        if(attack) {
            send_arp_spoofing(ethernet, buffer, size);
        }

        if(attack && ((furi_get_tick() - last_time) > 200)) {
            last_time = furi_get_tick();
            draw_arpspoofing_attacking(app, count_frame);
            count_frame++;
            if(count_frame > 4) count_frame = 0;
        }

        furi_delay_ms(1);
    }

    // If the device is not connected
    if(!start) {
        draw_device_no_connected(app);
    }

    return 0;
}
