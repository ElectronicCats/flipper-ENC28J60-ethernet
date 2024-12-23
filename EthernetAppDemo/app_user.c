#include "app_user.h"

uint8_t MAC[] = {0, 0, 0x56, 0x65, 0x55, 0x88};
uint8_t broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

uint8_t ip_origin[] = {192, 168, 1, 1};
uint8_t ip_destination[] = {255, 255, 255, 255};

int app_main(void* p) {
    UNUSED(p);
    UNUSED(MAC);

    enc28j60_t* enc = enc28j60_alloc(MAC);
    enc28j60_start(enc);

    uint8_t payload[] = "Holamundo!";

    uint16_t payload_length = sizeof(payload);

    uint8_t buffer[1520];

    memset(buffer, 0, sizeof(buffer));

    set_ethernet_header(buffer, MAC, broadcast, 0x0800);

    set_ipv4_header(
        buffer + sizeof(ethernet_header_t),
        0x11,
        payload_length + sizeof(udp_header_t),
        ip_origin,
        ip_destination);

    set_udp_header(
        buffer + sizeof(ethernet_header_t) + sizeof(ipv4_header_t),
        0,
        80,
        payload_length + sizeof(udp_header_t));

    memcpy(
        buffer + sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t),
        payload,
        payload_length);

    uint16_t total_length =
        sizeof(ethernet_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + payload_length;

    uint8_t buffer_to_received[1500];
    uint16_t length_buffer = 0;

    while(furi_hal_gpio_read(&gpio_button_back)) {
        if(is_link_up(enc)) {
            length_buffer = receive_packet(enc, buffer_to_received, 1500);
            if(length_buffer) {
                printf(
                    "-------------------------------------------------------------------------------------------- \n");
                for(uint16_t i = 0; i < length_buffer; i++) {
                    printf("%x ", buffer_to_received[i]);
                }
                printf("\n");
            }
        }

        if(furi_hal_gpio_read(&gpio_button_ok)) {
            printf("Send Frame: ===========================================\n");
            for(uint16_t i = 0; i < total_length; i++) {
                printf("%x ", buffer[i]);
            }
            printf("\n");
            send_packet(enc, buffer, total_length);
            furi_delay_ms(250);
        }
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
