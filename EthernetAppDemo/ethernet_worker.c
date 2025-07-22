#include "app_user.h"

uint8_t ip_gateway_test[4] = {0};

int32_t ethernet_thread(void* context) {
    App* app = (App*)context;
    bool running = true;

    UNUSED(app);

    uint32_t event = 0;

    enc28j60_t* ethernet = app->ethernet;
    // uint8_t* buffer = ethernet->tx_buffer;

    while(running) {
        furi_delay_ms(1);

        event = furi_thread_flags_wait(ALL_FLAGS, FuriFlagWaitAny, 1);

        if(event == MASK_FLAGS) event = 0;

        // First check if the app gonna stop
        if(event == flag_stop) {
            running = false;
        }

        // Check if the ethernet is link up
        if(is_link_up(ethernet)) {
            // Pass by the moment
        } else {
            if(event) view_dispatcher_send_custom_event(app->view_dispatcher, IS_NOT_LINK_UP);
            event = 0;
        }

        // If the Option is DORA
        if(event == flag_dhcp_dora) {
            if(process_dora(ethernet, ethernet->ip_address, app->ip_gateway)) {
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_gotten_event);
            } else {
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_no_gotten_event);
            }
        }

        // Here I need to put this thread on pause
        if(event == flag_sniffer) {
        }

        // furi_thread_flags_clear(ALL_FLAGS);
    }

    return 0;
}
