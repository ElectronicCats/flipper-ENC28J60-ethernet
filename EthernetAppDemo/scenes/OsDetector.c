#include "app_user.h"

#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/arp.h"

#define packet_count 20

static uint8_t target_ip[4] = {0};
const char* os_texts[] = {"WINDOWS", "LINUX", "IOS/MAC OS", "NO DETECTED"};

typedef enum {
    START,
    TARGET_IP,
} OS_DETECTOR_OPTIONS;

typedef enum {
    PORTS_SCANNER_SCENE_MENU,
    PORTS_SCANNER_SCENE_IP_INPUT,
    PORTS_SCANNER_SCENE_WIDGET,
} OS_DETECTOR_SCENE_STATES;

typedef enum {
    WINDOWS,
    LINUX,
    IOS,
    NO_DETECTED,
} OS_DETECTOR_OS;

//  Callback for the Input
void settings_start_ip_address_os_detector(void* context) {
    App* app = (App*)context;
    //scene_manager_previous_scene(app->scene_manager);
    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
    app_scene_os_detector_on_enter(app);
}

// Function to set the IP address
void set_ip_address_os_detector(App* app) {
    ip_assigner_reset(app->ip_assigner);
    ip_assigner_set_header(app->ip_assigner, "Set Ip Address");
    ip_assigner_callback(app->ip_assigner, settings_start_ip_address_os_detector, app);
    ip_assigner_set_ip_array(app->ip_assigner, target_ip);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, IpAssignerView); // Switch to the input byte view
}

// diferencia de IPID con manejo de wrap-around de 16 bits
int32_t ipid_diff(uint16_t prev, uint16_t curr) {
    int32_t diff = (int32_t)curr - (int32_t)prev;

    // corrección por wrap-around
    if(diff < -32768) {
        diff += 65536;
    } else if(diff > 32767) {
        diff -= 65536;
    }
    return diff;
}

double varianza(int32_t* d, int len) {
    double suma = 0.0, suma2 = 0.0;
    for(int i = 0; i < len; i++) {
        suma += d[i];
        suma2 += (double)d[i] * (double)d[i];
    }
    double media = suma / len;
    return (suma2 / len) - (media * media);
}

void clasificar_ipid(uint16_t* id, int n, uint8_t* value_ptr) {
    int32_t d[100];
    int positivos = 0, negativos = 0;

    for(int i = 0; i < n - 1; i++) {
        d[i] = ipid_diff(id[i], id[i + 1]);

        if(d[i] > 0)
            positivos++;
        else
            negativos++;
    }

    double var = varianza(d, n - 1);

    // regla simple para clasificar
    if(positivos >= 0.75 * (n - 1) && var < 5000) {
        *value_ptr = LINUX;
    } else {
        *value_ptr = IOS;
    }
}

int32_t os_detector_thread(void* context) {
    App* app = context;

    uint8_t value = NO_DETECTED;

    uint16_t ids[packet_count] = {0};
    bool respuestas[packet_count] = {0};
    uint16_t ids_an[packet_count] = {0};

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

    uint8_t attemp = 0;
    ipv4_header_t ipv4_header;
    while(attemp != packet_count) {
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
                        ipv4_header = ipv4_get_header(app->ethernet->rx_buffer);

                        uint16_t id;

                        bytes_to_uint(&id, ipv4_header.identification, sizeof(uint16_t));

                        ids[attemp] = id;
                        respuestas[attemp] = true;
                        break;
                    }
                }
            }
        }
        attemp++;
    }

    if(ipv4_header.ttl > 64 && ipv4_header.ttl <= 128) {
        value = WINDOWS;
    } else {
        uint8_t sum_true = 0;
        uint8_t an_index = 0;
        for(uint8_t i = 0; i < packet_count; i++) {
            sum_true += respuestas[i] ? 1 : 0;
            if(respuestas[i]) {
                ids_an[an_index] = ids[i];
                an_index++;
            }
        }

        clasificar_ipid(ids_an, sum_true, &value);
    }

    return value;
}

void variable_list_os_detector_callback(void* context, uint32_t index) {
    App* app = context;
    UNUSED(app);

    switch(index) {
    case START:
        if(app->is_dora) {
            furi_thread_suspend(app->thread);

            app->thread_alternative =
                furi_thread_alloc_ex("OS Detector", 5 * 1024, os_detector_thread, app);

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            furi_thread_start(app->thread_alternative);
            furi_thread_join(app->thread_alternative);

            uint32_t value = furi_thread_get_return_code(app->thread_alternative);
            draw_text(app, os_texts[value]);

            furi_thread_free(app->thread_alternative);

            furi_thread_resume(app->thread);
        } else {
            draw_dora_needed(app);
        }

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_WIDGET);
        view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
        break;

    case TARGET_IP:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_IP_INPUT);
        set_ip_address_os_detector(app);

        break;
    }
}

void app_scene_os_detector_on_enter(void* context) {
    App* app = context;

    variable_item_list_reset(app->varList);

    VariableItem* item;

    // Add the item to scan the network
    item = variable_item_list_add(app->varList, "Detect OS", 0, NULL, app);
    variable_item_set_current_value_text(item, "START");

    // Add item to set the IP address
    if(*(uint32_t*)target_ip == 0) memcpy(target_ip, app->ip_gateway, 4);
    item = variable_item_list_add(app->varList, "Target IP", 0, NULL, app);

    furi_string_reset(app->text); // Reset the text
    furi_string_cat_printf(
        app->text,
        "%u.%u.%u.%u",
        target_ip[0],
        target_ip[1],
        target_ip[2],
        target_ip[3]); // Set the text with the IP address

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->text)); // Set the varible item text

    //Set the callback for the variable item list
    variable_item_list_set_enter_callback(app->varList, variable_list_os_detector_callback, app);

    scene_manager_set_scene_state(
        app->scene_manager, app_scene_ports_scanner_option, PORTS_SCANNER_SCENE_MENU);

    view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);
}

bool app_scene_os_detector_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        switch(scene_manager_get_scene_state(app->scene_manager, app_scene_os_detector_option)) {
        case PORTS_SCANNER_SCENE_IP_INPUT:
        case PORTS_SCANNER_SCENE_WIDGET:

            scene_manager_set_scene_state(
                app->scene_manager, app_scene_os_detector_option, PORTS_SCANNER_SCENE_MENU);
            app_scene_os_detector_on_enter(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, VarListView);

            consumed = true;

            break;
        }
    }
    return consumed;
}

void app_scene_os_detector_on_exit(void* context) {
    UNUSED(context);
}
