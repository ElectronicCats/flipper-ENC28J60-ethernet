#include "app_user.h"

// Get Arp Request
bool arp_requested = true;

int32_t ethernet_thread(void* context) {
    App* app = (App*)context;
    bool running = true;

    UNUSED(app);

    uint32_t event = 0;
    uint16_t packet_len = 0;

    UNUSED(packet_len);

    enc28j60_t* ethernet = app->ethernet;
    uint8_t* buffer = ethernet->rx_buffer;

    while(running) {
        furi_delay_ms(1);

        event = furi_thread_flags_wait(ALL_FLAGS, FuriFlagWaitAny, 1);

        if(event == MASK_FLAGS) event = 0;

        // First check if the app gonna stop
        if(event == flag_stop) {
            break;
        }

        // Check if the ethernet is link up
        if(is_link_up(ethernet)) {
            packet_len = receive_packet(ethernet, buffer, MAX_FRAMELEN);
        } else {
            if(event) view_dispatcher_send_custom_event(app->view_dispatcher, IS_NOT_LINK_UP);
            event = 0;
        }

        if(packet_len) {
            // When the message is an ARP request
            arp_reply_requested(ethernet, buffer, ethernet->ip_address);

            // When the message is an ICMP request or ping
            ping_reply_to_request(ethernet, buffer, packet_len);
        }

        // If the Option is DORA
        if(event == flag_dhcp_dora) {
            view_dispatcher_send_custom_event(app->view_dispatcher, wait_ip_event);
            if(flipper_process_dora(ethernet, ethernet->ip_address, app->ip_gateway)) {
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_gotten_event);
                app->is_static_ip = true;
            } else {
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_no_gotten_event);
            }
        }
    }

    return 0;
}
