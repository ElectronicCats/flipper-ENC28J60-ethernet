/**
 * This files only has the purpose to try code for the functionality
 * of the aplication and ethernet messages
 */

#include "../app_user.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/generals/ethernet_generals.h"

#include "../modules/capture_module.h"

// Function for the thread
int32_t testing_thread(void* context);

// Function for the testing scene on enter
void app_scene_testing_scene_on_enter(void* context) {
    App* app = (App*)context;

    // Allocate and start the thread
    app->thread = furi_thread_alloc_ex("TESTING", 10 * 1024, testing_thread, app);
    furi_thread_start(app->thread);

    // Reset the widget and switch view
    widget_reset(app->widget);

    // Draw the in development message
    draw_in_development(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_testing_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_testing_scene_on_exit(void* context) {
    App* app = (App*)context;

    // Join and free the thread
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Functions to test
 */

/**
 * Thread for the Testing Scene
 */
int32_t testing_thread(void* context) {
    App* app = (App*)context;

    uint8_t example_packet[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x8c, 0xb5, 0xb3,
                                0xd7, 0x8c, 0x8,  0x6,  0x0,  0x1,  0x8,  0x0,  0x6,  0x4,
                                0x0,  0x1,  0x78, 0x8c, 0xb5, 0xb3, 0xd7, 0x8c, 0xc0, 0xa8,
                                0x0,  0x1,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0xc0, 0xa8,
                                0x0,  0xbb, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
                                0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0xc1, 0xcc, 0x55, 0xc2};

    uint8_t example_packet2[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x6c, 0x40, 0x8,  0xac,
                                 0xc9, 0x94, 0x8,  0x6,  0x0,  0x1,  0x8,  0x0,  0x6,  0x4,
                                 0x0,  0x1,  0x6c, 0x40, 0x8,  0xac, 0xc9, 0x94, 0xc0, 0xa8,
                                 0x0,  0xc2, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0xc0, 0xa8,
                                 0x0,  0xb6, 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
                                 0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0};

    uint16_t packet_len = sizeof(example_packet);

    // enc28j60_t* ethernet = app->ethernet;

    // Try to start the ENC28J60
    // bool start = enc28j60_start(ethernet) != 0xff;

    create_pcap_name(app->path, PATHPCAPS, "File2");

    printf("%s\n", furi_string_get_cstr(app->path));

    pcap_capture_init(app->file, furi_string_get_cstr(app->path));

    printf("File opened\n");

    pcap_capture_add_packet(app->file, example_packet, (uint32_t)packet_len);

    furi_delay_ms(1000);

    packet_len = sizeof(example_packet2);

    pcap_capture_add_packet(app->file, example_packet2, (uint32_t)packet_len);

    // if(!start) {
    //     // Device not connected
    //     printf("No conectado\n");
    // }

    // if(!is_the_network_connected(ethernet) && start) {
    //     // Network not connected
    //     printf("La red no esta conectada\n");
    //     start = false;
    // }

    // // Main loop
    // while(start && furi_hal_gpio_read(&gpio_button_back)) {
    //     if(furi_hal_gpio_read(&gpio_button_ok)) {
    //         furi_delay_ms(500);
    //     }
    //     furi_delay_ms(1);
    // }

    pcap_capture_close(app->file);

    printf("File closed\n");

    return 0;
}
