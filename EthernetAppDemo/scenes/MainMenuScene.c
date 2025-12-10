#include "../app_user.h"

#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/protocol_tools/arp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../modules/tcp_module.h"
#include "../modules/udp_module.h"
#include "../modules/arp_module.h"
/**
 * The main menu is the first scene to see in the Ethernet App
 * here the user selects an option that wants to do.
 */

// Time to show the LOGO
const uint32_t time_showing = 1000;
static uint8_t target_ip[4] = {192, 168, 2, 100}; /* OS DETECTOR TEST */
// List for the menu options
enum {
    SNIFFING_OPTION,
    READ_PCAPS_OPTION,
    ARP_ACTIONS_OPTION,
    TESTING_OPTION,
    PING_OPTION,
    PORTS_SCANNER_OPTION,
    OS_DETECTOR_OPTION,
    SETTINGS_OPTION,
    ABOUT_US
} main_menu_options;

// Function to display init at the start of the app
void draw_start(App* app) {
    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 40, 1, &I_EC48x26);
    widget_add_string_element(
        app->widget, 64, 40, AlignCenter, AlignCenter, FontPrimary, APP_NAME);
    widget_add_string_element(
        app->widget, 64, 55, AlignCenter, AlignCenter, FontSecondary, "Electronic Cats");

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);

    furi_delay_ms(time_showing);
}

//  Callback for the Options on the main menu
void main_menu_options_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case TESTING_OPTION:

        furi_thread_suspend(furi_thread_get_id(app->thread));

        /* OS DETECTOR TEST */

        uint16_t ids[5] = {0};
        bool respuestas[5] = {0};

        uint16_t ids_an[5] = {0};

        uint8_t target_mac[6] = {0};

        arp_get_specific_mac(
            app->ethernet,
            app->ethernet->ip_address,
            (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                    (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
                target_ip :
                app->ip_gateway,
            app->ethernet->mac_address,
            target_mac);

        uint32_t sequence = 1;
        uint32_t ack_number = 0;

        uint32_t last_time;

        printf("IP[3]: %u\n", target_ip[3]);

        uint8_t attemp = 0;
        while(attemp != 5) {
            tcp_send_syn(
                app->ethernet,
                app->ethernet->mac_address,
                app->ethernet->ip_address,
                target_mac,
                target_ip,
                5005,
                80,
                sequence,
                ack_number);

            last_time = furi_get_tick();
            while(!(furi_get_tick() - last_time > 3000)) {
                uint16_t packen_len = 0;

                packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

                if(packen_len) {
                    if(is_arp(app->ethernet->rx_buffer)) {
                        arp_reply_requested(
                            app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                    } else if(is_tcp(app->ethernet->rx_buffer)) {
                        if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                            *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                           (*(uint32_t*)app->ethernet->mac_address ==
                            *(uint32_t*)app->ethernet->rx_buffer)) {
                            ipv4_header_t ipv4_header = ipv4_get_header(app->ethernet->rx_buffer);
                            tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);

                            uint16_t id;
                            uint16_t window_size;

                            bytes_to_uint(&id, ipv4_header.identification, sizeof(uint16_t));
                            bytes_to_uint(&window_size, tcp_header.window_size, sizeof(uint16_t));

                            printf("EL IP ID DE ESTE PAQUETE ES: 0x%04X = %u\n", id, id);
                            printf("EL TTL DE ESTE PAQUETE ES: %u\n", ipv4_header.ttl);
                            printf("EL WINDOWS SIZE ES: %u\n", window_size);

                            ids[attemp] = id;
                            respuestas[attemp] = true;
                            printf("RECIBIDO: ");
                            for(uint16_t i = 0; i < packen_len; i++) {
                                printf(
                                    "%02X%c",
                                    app->ethernet->rx_buffer[i],
                                    i == (packen_len - 1) ? '\n' : ' ');
                            }
                            break;
                        }
                    }
                }
            }
            attemp++;
        }

        uint8_t sum_true = 0;
        for(uint8_t i = 0; i < 5; i++) {
            printf("[%u] => ID: %u\tRESP: %s\n", i, ids[i], respuestas[i] ? "true" : "false");
            sum_true += respuestas[i] ? 1 : 0;
        }

        uint8_t an_index = 0;
        for(uint8_t i = 0; i < 5; i++) {
            if(respuestas[i]) {
                ids_an[an_index] = ids[i];
                an_index++;
            }
        }

        uint32_t sum_dif = 0;
        for(uint8_t i = 0; i < (sum_true - 1); i++) {
            printf("%u - %u = %u\n", ids_an[i + 1], ids_an[i], ids_an[i + 1] - ids_an[i]);
            sum_dif += ids_an[i + 1] - ids_an[i];
        }

        printf("LA SUMA DIFERENCIAL DEL IP ID ES: %lu\n", sum_dif);
        printf("LA SUMA DE TRUES: %u\n", sum_true);
        if(sum_true == 0)
            printf("PUERTO CERRADO\n");
        else if(sum_dif == 4)
            printf("LA RESPUESTA PARECE SER DE WINDOWS\n");
        else if(sum_dif == 0)
            printf("LA RESPUESTA PARECER SER DE IOS\n");
        else if(sum_dif != 4)
            printf("LA RESPUESTA PARECER DE LINUX\n");

        if(target_ip[3] == 100)
            target_ip[3] = 102;
        else if(target_ip[3] == 102)
            target_ip[3] = 1;
        else if(target_ip[3] == 1)
            target_ip[3] = 100;

        /* OS DETECTOR TEST */

        furi_thread_resume(furi_thread_get_id(app->thread));

        break;

    case SNIFFING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_sniffer_option);
        break;

    case READ_PCAPS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_browser_pcaps_option);
        break;

    case ARP_ACTIONS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_arp_action_menu_option);
        break;

    case PING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ping_menu_option);
        break;

    case PORTS_SCANNER_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ports_scanner_option);
        break;

    case OS_DETECTOR_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_os_detector_option);
        break;

    case SETTINGS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_settings_option);
        break;

    case ABOUT_US:
        scene_manager_next_scene(app->scene_manager, app_scene_about_us_option);
        break;

    default:
        break;
    }
}

// Function for the main menu on enter
void app_scene_main_menu_on_enter(void* context) {
    App* app = (App*)context;

    // Variable used to show the EC logo once
    static bool is_logo_shown = false;
    if(!is_logo_shown) draw_start(app);

    if(furi_thread_is_suspended(furi_thread_get_id(app->thread))) {
        furi_thread_resume(furi_thread_get_id(app->thread));
    }

    is_logo_shown = true;

    // Reset Menu
    submenu_reset(app->submenu);

    // header for the  submenu
    submenu_set_header(app->submenu, "ETHERNET FUNCTIONS");

    submenu_add_item(app->submenu, "Sniffer", SNIFFING_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "ARP Actions", ARP_ACTIONS_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Read Pcaps", READ_PCAPS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Ping", PING_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Ports Scanner", PORTS_SCANNER_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "OS Detector", OS_DETECTOR_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "...", TESTING_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "Settings", SETTINGS_OPTION, main_menu_options_callback, app);

    submenu_add_item(app->submenu, "About Us", ABOUT_US, main_menu_options_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the main menu on event
bool app_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the main menu on exit
void app_scene_main_menu_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
