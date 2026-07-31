#include "../app_user.h"
#include "../modules/lldp_module.h"

static const char* passive_protocol_names[] = {
    "LLDP",
    "EAPOL",
    "CDP",
};

int32_t passive_discovery_thread(void* context);

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context);

static void passive_discovery_draw_config(App* app) {
    widget_reset(app->widget);

    char protocol_text[24];

    snprintf(
        protocol_text,
        sizeof(protocol_text),
        "< %s >",
        passive_protocol_names[app->passive_discovery.protocol]);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Protocol");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, protocol_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "<", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Start", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, ">", passive_discovery_button_callback, app);
}

static void passive_discovery_draw_listening(App* app) {
    char neighbors_text[32];

    snprintf(neighbors_text, sizeof(neighbors_text), "Neighbors: %u", app->passive_neighbor_count);

    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Listening...");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, neighbors_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Stop", passive_discovery_button_callback, app);
}

static void passive_discovery_refresh(App* app) {
    switch(app->passive_discovery.state) {
    case PassiveDiscoveryStateConfig:
        passive_discovery_draw_config(app);
        break;

    case PassiveDiscoveryStateListening:
        passive_discovery_draw_listening(app);
        break;

    case PassiveDiscoveryStateFinished:
        app->passive_selected_neighbor = 0;

        scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_list_option);

        break;
    }
}

void app_scene_passive_discovery_on_enter(void* context) {
    App* app = context;

    if(!app || !app->widget || !app->view_dispatcher) {
        return;
    }

    app->passive_discovery.state = PassiveDiscoveryStateConfig;
    app->passive_discovery.protocol = PassiveProtocolLLDP;
    app->passive_discovery_stop = false;
    app->passive_neighbor_count = 0;

    passive_discovery_refresh(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_passive_discovery_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            app->passive_discovery_stop = true;

            if(app->thread_alternative) {
                furi_thread_join(app->thread_alternative);

                furi_thread_free(app->thread_alternative);

                app->thread_alternative = NULL;
            }
        }

        return scene_manager_previous_scene(app->scene_manager);
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            passive_discovery_refresh(app);

            return true;
        }
    }

    return false;
}

void app_scene_passive_discovery_on_exit(void* context) {
    App* app = context;

    if(!app) {
        return;
    }

    app->passive_discovery_stop = true;

    widget_reset(app->widget);
}

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;

    if(input_type != InputTypeShort) {
        return;
    }

    switch(type) {
    case GuiButtonTypeLeft:
        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            if(app->passive_discovery.protocol == 0) {
                app->passive_discovery.protocol = PassiveProtocolCount - 1;
            } else {
                app->passive_discovery.protocol--;
            }

            passive_discovery_refresh(app);
        }
        break;

    case GuiButtonTypeRight:
        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            app->passive_discovery.protocol =
                (app->passive_discovery.protocol + 1) % PassiveProtocolCount;

            passive_discovery_refresh(app);
        }
        break;

    case GuiButtonTypeCenter:
        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            app->passive_discovery.state = PassiveDiscoveryStateListening;

            passive_discovery_refresh(app);

            app->thread_alternative =
                furi_thread_alloc_ex("Passive Discovery", 4096, passive_discovery_thread, app);

            furi_thread_start(app->thread_alternative);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            app->passive_discovery_stop = true;

            if(app->thread_alternative) {
                furi_thread_join(app->thread_alternative);

                furi_thread_free(app->thread_alternative);

                app->thread_alternative = NULL;
            }

            app->passive_discovery.state = PassiveDiscoveryStateFinished;

            passive_discovery_refresh(app);
        } else if(app->passive_discovery.state == PassiveDiscoveryStateFinished) {
            app->passive_selected_neighbor = 0;

            scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_list_option);
        }
        break;

    default:
        break;
    }
}

int32_t passive_discovery_thread(void* context) {
    printf("PASSIVE THREAD ENTERED\n");
    App* app = context;
    enc28j60_t* ethernet = app->ethernet;

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff;
        app->enc28j60_connected = start;
    }

    if(!start) {
        draw_device_no_connected(app);
        furi_delay_ms(1500);
        return 0;
    }

    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        furi_delay_ms(1500);
        return 0;
    }

    scanner_session_t session;

    scanner_session_init(&session, app);

    switch(app->passive_discovery.protocol) {
    case PassiveProtocolLLDP:
        enable_multicast(ethernet);
        lldp_module_init();
        break;

    case PassiveProtocolEAPOL:
        break;

    case PassiveProtocolCDP:
        break;

    default:
        break;
    }

    while(!app->passive_discovery_stop) {
        switch(app->passive_discovery.protocol) {
        case PassiveProtocolLLDP:

            bool result = lldp_module_run(&session, 500);

            if(result) {
                FURI_LOG_I("PASSIVE", "LLDP packet processed");
            }

            uint16_t count = neighbor_db_count();

            if(count != app->passive_neighbor_count) {
                app->passive_neighbor_count = count;

                if(!app->passive_discovery_stop) {
                    view_dispatcher_send_custom_event(app->view_dispatcher, 1);
                }
            }

            break;

        case PassiveProtocolEAPOL:
            break;

        case PassiveProtocolCDP:
            break;

        default:
            break;
        }
    }

    scanner_session_deinit(&session);

    return 0;
}
