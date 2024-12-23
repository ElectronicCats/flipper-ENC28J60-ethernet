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

    udp_message_t message;

    udp_set_enc_device(&message, enc);
    udp_set_mac_address(&message, MAC, broadcast);
    udp_set_ip_address(&message, ip_origin, ip_destination);
    udp_set_port(&message, 0, 80);

    uint8_t payload[] = "Hello Kitty significa: Hola demonio";

    uint16_t payload_length = sizeof(payload);

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

            if(furi_hal_gpio_read(&gpio_button_ok)) {
                udp_send_packet(message, payload, payload_length);
                furi_delay_ms(250);
            }
        }
    }

    enc28j60_deinit(enc);
    free_enc28j60(enc);

    return 0;
}
