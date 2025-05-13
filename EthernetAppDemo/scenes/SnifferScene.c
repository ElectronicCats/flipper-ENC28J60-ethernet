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
    UNUSED(app);
    UNUSED(event);
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
 *
 */

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
}

/**
 * Thread for the sniffing
 */

int32_t sniffer_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;
    bool draw_once = true;
    bool start = enc28j60_start(ethernet) != 0xff;

    uint8_t buffer[1500] = {0};
    uint16_t packet_len = 0;
    uint32_t packet_counter = 0;

    if(!start) {
        draw_device_no_connected(app); // Draw if the dvice is not connected
    }

    if(start) {
        // Wait until the red is connected
        while(!is_link_up(ethernet)) {
            if(draw_once) {
                draw_waiting_connection(app->widget);
                draw_once = false;
            }

            if(!furi_hal_gpio_read(&gpio_button_back)) {
                start = false;
                break;
            }
        }
    }

    if(start) {
        // Solve the path to not repeat or rewrite files
        solve_paths(app->storage, app->path);

        // Create and start pcap
        pcap_capture_init(app->file, furi_string_get_cstr(app->path));

        // Display count of packets
        draw_count_packets(app, packet_counter);
    }

    // Starts
    while(start && furi_hal_gpio_read(&gpio_button_back)) {
        packet_len = receive_packet(ethernet, buffer, 1500);

        if(packet_len) {
            packet_counter++; // add more on the counter
            draw_count_packets(app, packet_counter); // Display count of packets
        }

        furi_delay_ms(1);
    }

    if(start) {
        // Close the file
        pcap_capture_close(app->file);
    }

    return 0;
}
