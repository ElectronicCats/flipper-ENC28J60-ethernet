#include "../app_user.h"

uint8_t router_ip[4] = {192, 168, 0, 1};

// Function for the testing scene on enter
void app_scene_get_ip_scene_on_enter(void* context) {
    App* app = (App*)context;

    // Reset the widget and switch view
    widget_reset(app->widget);

    // Send flag
    furi_thread_flags_set(furi_thread_get_id(app->thread), flag_dhcp_dora);

    // Change view
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

// Function for the testing scene on event
bool app_scene_get_ip_scene_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case IS_NOT_LINK_UP:
            printf("ETHERNET NO CONECTADO\n");
            draw_network_not_connected(app);
            break;

        case wait_ip_event:
            draw_waiting_for_ip(app);
            break;

        case ip_no_gotten_event:
            draw_ip_not_got_it(app);
            break;

        case ip_gotten_event:
            draw_your_ip_is(app);
            break;

        default:
            break;
        }
    }
    return consumed;
}

// Function for the testing scene on exit
void app_scene_get_ip_scene_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);
}
