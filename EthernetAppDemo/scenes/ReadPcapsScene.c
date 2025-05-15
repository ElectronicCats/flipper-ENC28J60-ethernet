#include "../app_user.h"

// Function for the thread
int32_t thread_read_pcaps(void* context);

// Function for the testing scene on enter
void app_scene_read_pcap_on_enter(void* context) {
    App* app = (App*)context;

    // Temporary time
    // furi_string_reset(app->path);
    // furi_string_cat_printf(app->path, "/ext/apps_data/ethernet/files/file_15_05_2025_1.pcap");

    // Allocate and start the thread
    app->thread = furi_thread_alloc_ex("TESTING", 10 * 1024, thread_read_pcaps, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    // Draw the in development message
    draw_in_development(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_read_pcap_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_read_pcap_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);

    // Join and free the thread
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Thread for read the packets from pcap
 */

int32_t thread_read_pcaps(void* context) {
    App* app = (App*)context;

    printf("%s\n", furi_string_get_cstr(app->path));

    uint32_t packet_count = 0;
    uint64_t packet_positions[2000];

    packet_count = pcap_scan(app->file, furi_string_get_cstr(app->path), packet_positions);

    if(packet_count) {
        printf("Packets: %lu\n", packet_count);

        // pcap_get_packet(app->file, buffer, &packet_len);

        printf("\n");
        for(uint32_t i = 0; i < packet_count; i++) {
            printf("Packet [%lu]: %llu \n", i, packet_positions[i]);
        }

        // for(uint32_t i = 0; i < packet_len; i++) {
        //     printf("%02x\t", buffer[i]);
        // }

        printf("\n");
    }

    return 0;
}
