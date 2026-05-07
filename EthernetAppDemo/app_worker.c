#include "app_user.h"

int32_t ethernet_thread(void* context) {
    App* app = (App*)context;
    enc28j60_t* ethernet = app->ethernet;

    while(true) {
        uint32_t event = furi_thread_flags_wait(ALL_FLAGS, FuriFlagWaitAny, FuriWaitForever);

        if(event == MASK_FLAGS) continue;

        if(event & flag_stop) {
            break;
        }

        if(event & flag_dhcp_dora) {
            view_dispatcher_send_custom_event(app->view_dispatcher, wait_ip_event);

            // F0.4a — pause rx_dispatch around DORA so DHCP OFFER/ACK
            // packets reach the worker's own receive_packet calls
            // inside flipper_process_dora_with_host_name. Without this
            // pause, rx_dispatch consumes the OFFER and drops it (no
            // registered handler matches DHCP), making DORA never
            // complete. F0.4b replaces this with a DHCP handler so the
            // worker no longer reads the chip directly.
            rx_dispatch_pause();
            bool got_ip = flipper_process_dora_with_host_name(
                ethernet,
                ethernet->ip_address,
                app->ip_gateway,
                ethernet->subnet_mask,
                "Flippa 0");
            rx_dispatch_resume();

            if(got_ip) {
                app->is_dora = true;
                send_arp_gratuitous(ethernet, ethernet->mac_address, ethernet->ip_address);
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_gotten_event);
                app->is_static_ip = true;
            } else {
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_no_gotten_event);
            }
            furi_thread_flags_clear(flag_dhcp_dora);
        }
    }

    return 0;
}
