#include "app_user.h"
#include "modules/dhcp_protocol.h"
#include "modules/arp_module.h"

#define DEBUG_FILE true

// Instance for the ENC28j60
enc28j60_t* enc = NULL;

// MAC Address
uint8_t MAC[6] = {0xba, 0x3f, 0x91, 0xc2, 0x7e, 0x5d}; // BA:3F:91:C2:7E:5D

// The ip for the client and the server
uint8_t MY_IP[4] = {192, 168, 0, 185};
uint8_t router_ip[4] = {192, 168, 0, 1};

// IP to start
uint8_t ip_to_start[4] = {192, 168, 0, 100};

uint8_t count_of_list = 0;

#define devices_to_search 100

// Just to help to see the IP
void show_ip(uint8_t* data) {
    for(uint8_t i = 0; i < 4; i++) {
        printf("%u:", data[i]);
    }
    printf("\n");
}

/**
 * Function to get the connection
 */
bool connection() {
    uint32_t prev_time = furi_get_tick();

    while(!process_dora(enc, MY_IP, router_ip)) {
        if(furi_get_tick() > (prev_time + 5000)) return false;
    }

#if DEBUG_FILE
    printf("MY IP: \n");
    show_ip(MY_IP);
    printf("ROUTER IP: \n");
    show_ip(router_ip);
#endif

    return true;
}

int app_main(void* p) {
    UNUSED(p);

    uint8_t buffer[1500] = {0};

    uint16_t lenght = 0;

    UNUSED(buffer);

    UNUSED(lenght);

    bool start = false;

    enc = enc28j60_alloc(MAC);
    if(enc28j60_start(enc) != 0xff) {
        start = connection();
    }

    arp_list list_ip_to_find[devices_to_search] = {0};

    while(furi_hal_gpio_read(&gpio_button_back) && start) {
        if(furi_hal_gpio_read(&gpio_button_ok)) {
            arp_scan_network(
                enc, list_ip_to_find, MAC, MY_IP, ip_to_start, &count_of_list, devices_to_search);

            printf("IP after the scan\n");

            for(uint8_t i = 0; i < count_of_list; i++) {
                printf(
                    "%u:  %u:%u:%u:%u   with MAC: %x:%x:%x:%x:%x:%x\n",
                    i,
                    list_ip_to_find[i].ip[0],
                    list_ip_to_find[i].ip[1],
                    list_ip_to_find[i].ip[2],
                    list_ip_to_find[i].ip[3],
                    list_ip_to_find[i].mac[0],
                    list_ip_to_find[i].mac[1],
                    list_ip_to_find[i].mac[2],
                    list_ip_to_find[i].mac[3],
                    list_ip_to_find[i].mac[4],
                    list_ip_to_find[i].mac[5]);
            }
            furi_delay_ms(200);
        }

        furi_delay_ms(1);
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
