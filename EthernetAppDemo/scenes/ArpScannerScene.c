#include "../app_user.h"

/**
 * This file contains the functions to display and work with the ARP SCANNER
 * It shows the IP list and saves it in a array
 */

// Variables for the ARP Scanner
uint8_t ip_start[4] = {192, 168, 0, 3}; // The IP address to start the scan
uint8_t range_ip = 1; // The count of the IP addresses to scan

/**
 * Menu To select the options and range of the Scanner
 */

// Callback for to enter when the variable item is pressed
void variable_list_enter_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    if(index == 2) {
        return;
    }

    if(index == 0 &&
       scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_menu_option) == 2) {
        index = 3;
    }

    // Set the state of the scene to the index
    scene_manager_set_scene_state(app->scene_manager, app_scene_arp_scanner_option, index);

    // Go to the next arp scanner
    scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);
}

// Callback for the variable item
void change_value_callback(VariableItem* item) {
    App* app = (App*)variable_item_get_context(item);
    uint32_t value = variable_item_get_current_value_index(item);

    if(value == 0) value = 255; // If the value is minor of 1
    if(value > 255) value = 1; // If the value is mayor of 255

    range_ip = value;

    // Set the value in the Element
    variable_item_set_current_value_index(item, range_ip);

    // Set the text
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "%u", range_ip);

    // Set the text in the element
    variable_item_set_current_value_text(item, furi_string_get_cstr(app->text));
}

// function for the arp menu scene on enter
void app_scene_arp_scanner_menu_on_enter(void* context) {
    App* app = (App*)context;

    variable_item_list_reset(app->varList);

    VariableItem* item;

    // Add the item to scan the network
    item = variable_item_list_add(app->varList, "Start Scanner", 0, NULL, app);
    variable_item_set_current_value_text(item, "START");

    // Add item to set the IP address
    item = variable_item_list_add(app->varList, "Set IP Address", 0, NULL, app);

    furi_string_reset(app->text); // Reset the text
    furi_string_cat_printf(
        app->text,
        "%u.%u.%u.%u",
        ip_start[0],
        ip_start[1],
        ip_start[2],
        ip_start[3]); // Set the text with the IP address

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->text)); // Set the varible item text

    // Add item to set the range of the scan
    item = variable_item_list_add(app->varList, "Set Range", 255, change_value_callback, app);

    variable_item_set_current_value_index(item, range_ip); // Set the current value

    furi_string_reset(app->text); // Reset the text
    furi_string_cat_printf(
        app->text, "%u", range_ip); // Set the text with the total number of IP addresses

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->text)); // Set the varible item text

    // set the callback for the variable item list
    variable_item_list_set_enter_callback(app->varList, variable_list_enter_callback, app);

    // Switch to the variable list view
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

// function for the arp menu scene on event
bool app_scene_arp_scanner_menu_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// function for the arp menu scene on exit
void app_scene_arp_scanner_menu_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);
}

/**
 * Scene to show the list of the IP in the network
 */

// Function for the thread
int32_t arp_scanner_thread(void* context);

// Function to set the thread and the view
void draw_the_arp_list(App* app) {
    furi_thread_suspend(furi_thread_get_id(app->thread));

    app->thread_alternative =
        furi_thread_alloc_ex("ARP SCANNER", 10 * 1024, arp_scanner_thread, app);
    furi_thread_start(app->thread_alternative);

    // Switch the view of the flipper
    widget_reset(app->widget);
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function to draw to finished the thread
void finished_arp_thread(App* app) {
    furi_thread_join(app->thread_alternative);
    furi_thread_free(app->thread_alternative);
    furi_thread_resume(furi_thread_get_id(app->thread));
}

//  Callback for the Input
void settings_start_ip_address(void* context) {
    App* app = (App*)context;
    scene_manager_previous_scene(app->scene_manager);
}

// Function to set the IP address
void set_ip_address(App* app) {
    // Set the header text
    byte_input_set_header_text(app->input_byte_value, "Set IP Address");

    byte_input_set_result_callback(
        app->input_byte_value,
        settings_start_ip_address,
        NULL,
        app,
        ip_start,
        4); // Set the callback for the input IP address

    view_dispatcher_switch_to_view(
        app->view_dispatcher, InputByteView); // Switch to the input byte view
}

// Function to show the list of IP
void show_current_arp_list(App* app) {
    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// function on enter for the arp scanner scene
void app_scene_arp_scanner_on_enter(void* context) {
    App* app = (App*)context;

    switch(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option)) {
    case 0:
    case 3:
        // If the scene is in state 0, starts the thread and show ip list
        draw_the_arp_list(app);
        break;

    case 1:
        // Starts the set IP view (input byte view)
        set_ip_address(app);
        break;

    case 2:
        // Show the IP list in this scene
        show_current_arp_list(app);
        break;

    default:
        break;
    }
}

// function on event for the arp scanner scene
bool app_scene_arp_scanner_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// function on exit for the arp scanner scene
void app_scene_arp_scanner_on_exit(void* context) {
    App* app = (App*)context;

    // If scene is in state 0 it finished the
    switch(scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option)) {
    case 0:
    case 3:
        finished_arp_thread(app);
        break;

    default:
        break;
    }
}

/**
 * Callback for the options in the ip list
 */

// For only arp scanner
void ip_list_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    // Set the scene to get the index of the ip in the IP array list
    scene_manager_set_scene_state(app->scene_manager, app_scene_arp_ip_show_details_option, index);

    // Go to show details scene
    scene_manager_next_scene(app->scene_manager, app_scene_arp_ip_show_details_option);
}

// For the arp spoofing
void ip_list_spoofing_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    // copy the IP
    memcpy(app->ip_helper, app->ip_list[index].ip, 4);

    // Return to the last view that is the spoofing
    scene_manager_previous_scene(app->scene_manager);
}

/**
 * This scene is to show the MAC address from the IP
 */

// Function to get the IP list
void app_scene_arp_ip_show_details_on_enter(void* context) {
    App* app = (App*)context;

    // Change state of the previous scene to only show the Submenu view
    scene_manager_set_scene_state(app->scene_manager, app_scene_arp_scanner_option, 2);

    // Get the index in the arp ip list
    uint32_t index =
        scene_manager_get_scene_state(app->scene_manager, app_scene_arp_ip_show_details_option);

    // Reset furi string text
    furi_string_reset(app->text);

    // Point to the current ip indexed
    uint8_t* ip_showed = app->ip_list[index].ip;

    // Point to the current mac
    uint8_t* mac_showed = app->ip_list[index].mac;

    // Get if the IP is duplicated
    bool is_duplicated = is_duplicated_ip(ip_showed, app->ip_list, app->ip_counter);

    // Set the text to show IP address with it MAC address
    furi_string_printf(
        app->text, "IP: %u.%u.%u.%u ", ip_showed[0], ip_showed[1], ip_showed[2], ip_showed[3]);

    // add duplicated if the ip is
    if(is_duplicated) {
        furi_string_cat_printf(app->text, "\n(Duplicated)");
    }

    // Set MAC in the text
    furi_string_cat_printf(
        app->text,
        "\nMAC: %02x:%02x:%02x:%02x:%02x:%02x",
        mac_showed[0],
        mac_showed[1],
        mac_showed[2],
        mac_showed[3],
        mac_showed[4],
        mac_showed[5]);

    // reset Widget
    widget_reset(app->widget);

    // Set Header manually
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "IP ADDRESS DETAILS");

    // Set Text for the detailes of the IP
    widget_add_string_multiline_element(
        app->widget,
        10,
        30,
        AlignLeft,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));

    // Switch to widget view
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function to get the ip list
bool app_scene_arp_ip_show_details_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the ip list
void app_scene_arp_ip_show_details_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);
}

/**
 * Thread for the ARP Scanner
 */

int32_t arp_scanner_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    bool start = app->enc28j60_connected;

    uint32_t selection =
        scene_manager_get_scene_state(app->scene_manager, app_scene_arp_scanner_option);

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff; // Start the enc28j60
        app->enc28j60_connected = start; // Update the connection status
    }

    if(!start) {
        draw_device_no_connected(app);
    }

    if(!is_the_network_connected(ethernet) && start) {
        draw_network_not_connected(app);
        start = false;
    }

    if(start) {
        arp_scan_network(ethernet, app->ip_list, ip_start, &app->ip_counter, range_ip);

        submenu_reset(app->submenu);
        view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

        submenu_set_header(app->submenu, "IP LIST");

        for(uint8_t i = 0; i < app->ip_counter; i++) {
            furi_string_reset(app->text);
            furi_string_cat_printf(
                app->text,
                "%u.%u.%u.%u",
                app->ip_list[i].ip[0],
                app->ip_list[i].ip[1],
                app->ip_list[i].ip[2],
                app->ip_list[i].ip[3]);

            if(is_duplicated_ip(app->ip_list[i].ip, app->ip_list, app->ip_counter)) {
                furi_string_cat_printf(app->text, " (D)");
            }
            if(selection == 0) {
                submenu_add_item(
                    app->submenu, furi_string_get_cstr(app->text), i, ip_list_callback, app);
            } else if(selection == 3) {
                submenu_add_item(
                    app->submenu,
                    furi_string_get_cstr(app->text),
                    i,
                    ip_list_spoofing_callback,
                    app);
            }
        }
    }

    return 0;
}
