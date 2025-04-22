/**
 * This files only has the purpose to try code for the functionality
 * of the aplication and ethernet messages
 */

#include "../app_user.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/generals/ethernet_generals.h"

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

bool create_tcp_message(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t dest_port,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    const uint8_t* data,
    uint16_t data_length) {
    if(buffer == NULL || data == NULL || src_ip == NULL || dst_ip == NULL) return false;

    // Set up the TCP header (starts after Ethernet + IP headers)
    uint8_t* tcp_start = buffer + 14 + IP_HEADER_LEN;

    // Set up TCP header with common values
    set_tcp_header(
        tcp_start,
        source_port, // Source port
        dest_port, // Destination port
        0, // Initial sequence number
        0, // ACK number (0 for initial SYN)
        0x02, // Flags (0x02 = SYN flag)
        8192, // Window size (typical initial value)
        0 // Urgent pointer
    );

    memcpy(tcp_start + TCP_HEADER_LEN, data, data_length);

    uint16_t checksum =
        calculate_tcp_checksum(tcp_start, TCP_HEADER_LEN + data_length, src_ip, dst_ip);

    // Set the calculated checksum
    tcp_start[16] = (checksum >> 8) & 0xFF;
    tcp_start[17] = checksum & 0xFF;

    return true;
}

/**
 * Thread for the Testing Scene
 */
int32_t testing_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    uint8_t buffer[1500] = {0};
    uint16_t size = 1500;

    // Message to test TCP
    uint8_t message[] = "Hello TCP!";
    uint8_t message_size = sizeof(message);

    // Try to start the ENC28J60
    bool start = enc28j60_start(ethernet) != 0xff;

    if(!start) {
        // Device not connected
        printf("No conectado\n");
    }

    if(!is_the_network_connected(ethernet) && start) {
        // Network not connected
        printf("La red no esta conectada\n");
        start = false;
    }

    // set an ethernet header with IPv4 type
    set_ethernet_header(buffer, app->mac_device, MAC_BROADCAST, 0x0800);

    // set the ipv4 header
    set_ipv4_header(buffer + 14, 0x06, message_size + 20, app->ip_device, IP_BROADCAST);

    // Main loop
    while(start && furi_hal_gpio_read(&gpio_button_back)) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            printf("Enviando mensaje TCP\n");

            // Create a TCP message
            create_tcp_message(
                buffer, 1234, 80, app->ip_device, app->ip_list[0].ip, message, message_size);
            // Send the message through the ENC28J60
            send_packet(ethernet, buffer, size);
            furi_delay_ms(500);
        }
        furi_delay_ms(1); // Small delay to prevent CPU hogging
    }

    return 0;
}
