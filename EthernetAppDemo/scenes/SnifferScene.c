#include "../app_user.h"

// Thread for the sniffer
int32_t sniffer_thread(void* context);

// Function for the testing scene on enter
void app_scene_sniffer_on_enter(void* context) {
    App* app = (App*)context;

    // Allocate and start the thread
    app->thread = furi_thread_alloc_ex("TESTING", 10 * 1024, sniffer_thread, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_sniffer_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 1:
            // scene_manager_next_scene(app->scene_manager, );
            break;

        case 0xff:
            scene_manager_previous_scene(app->scene_manager);
            break;

        default:
            break;
        }
    }
    return consumed;
}

// Function for the testing scene on exit
void app_scene_sniffer_on_exit(void* context) {
    App* app = (App*)context;

    UNUSED(app);

    // Join and free the thread
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Function to solve paths
 * All pcaps will be named as file_dd_mm_yy_n
 * Example: file_13_05_2025_1.pcap
 */

void solve_paths(Storage* storage, FuriString* path) {
    // Get date
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    // counter
    uint16_t count_files = 0;

    do {
        furi_string_reset(path);

        furi_string_cat_printf(
            path,
            "%s/file_%02u_%02u_%i_%i.pcap",
            PATHPCAPS,
            datetime.day,
            datetime.month,
            datetime.year,
            count_files);

        count_files++;
    } while(storage_file_exists(storage, furi_string_get_cstr(path)));
}

/**
 * Some functions to draw
 */

// Draw for waiting the connection
void draw_waiting_connection(Widget* widget) {
    widget_reset(widget);
    widget_add_string_multiline_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting\nFor\nConnection");
}

// Draw for the count of the packets
void draw_count_packets(App* app, uint32_t packets) {
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 20, AlignCenter, AlignCenter, FontSecondary, "Packets Received");

    furi_string_reset(app->text);

    furi_string_cat_printf(app->text, "%lu", packets);

    widget_add_string_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(app->text));

    widget_add_button_element(widget, GuiButtonTypeCenter, "Stop", NULL, NULL);
}

// Draw if the user want to sniff packets
void draw_want_to_sniff(Widget* widget) {
    widget_reset(widget);

    widget_add_string_multiline_element(
        widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "Start\nSniffing");

    widget_add_button_element(widget, GuiButtonTypeCenter, "Start", NULL, NULL);
}

// Draw if the user wants to see the packets sniffed
void draw_want_to_show_packets(Widget* widget) {
    widget_reset(widget);

    widget_add_string_multiline_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Want to see packets?");

    widget_add_button_element(widget, GuiButtonTypeRight, "Show", NULL, NULL);
}

/**
 * Thread for the sniffing
 */

int32_t sniffer_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;
    // bool draw_once = true;
    bool show_packets = false;
    bool start = enc28j60_start(ethernet) != 0xff;

    uint8_t buffer[1500] = {0};
    uint16_t packet_len = 0;
    uint32_t packet_counter = 0;

    if(!start) {
        draw_device_no_connected(app); // Draw if the dvice is not connected
        furi_delay_ms(900);
    }

    // Message to show if want to start the sniffing
    if(start) {
        draw_want_to_sniff(app->widget);

        while(!furi_hal_gpio_read(&gpio_button_ok)) {
            if(!furi_hal_gpio_read(&gpio_button_back)) {
                start = false;
                break;
            }
        }
    }

    // Delay to avoid double click
    furi_delay_ms(300);

    // Moment to wait if the network is connected
    if(start) {
        draw_waiting_connection(app->widget);

        // Wait until the red is connected
        while(!is_link_up(ethernet)) {
            if(!furi_hal_gpio_read(&gpio_button_back)) {
                start = false;
                break;
            }
        }
    }

    // Once the network is connected it will create the file for the pcap
    if(start) {
        // Solve the path to not repeat or rewrite files
        solve_paths(app->storage, app->path);

        // Create and start pcap
        pcap_capture_init(app->file, furi_string_get_cstr(app->path));

        // Display count of packets
        draw_count_packets(app, packet_counter);
    }

    // Start sniffing packets
    while(start && furi_hal_gpio_read(&gpio_button_back)) {
        packet_len = receive_packet(ethernet, buffer, 1518);

        if(packet_len) {
            packet_counter++; // add more on the counter
            draw_count_packets(app, packet_counter); // Display count of packets
        }

        // If user pressed button ok it stops and show packets
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            show_packets = true;
            furi_delay_ms(250);
            break;
        }

        furi_delay_ms(1);
    }

    // Close the file if it started well
    if(start) {
        // Close the file
        pcap_close(app->file);
    }

    // Once it stop with the button ok shows if wants to read all packets
    if(show_packets) {
        draw_want_to_show_packets(app->widget);

        // wait until the button okay
        while(furi_hal_gpio_read(&gpio_button_right)) {
            if(!furi_hal_gpio_read(&gpio_button_back)) {
                show_packets = false;
                break;
            }
        }
    }

    // Moment to show packets
    if(show_packets) view_dispatcher_send_custom_event(app->view_dispatcher, 1);

    // To exit if the enc28j60 is not connected
    if(!start) view_dispatcher_send_custom_event(app->view_dispatcher, 0xff);

    return 0;
}
