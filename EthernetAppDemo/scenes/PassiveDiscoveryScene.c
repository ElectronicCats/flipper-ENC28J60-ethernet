#include "../app_user.h"
#include "../modules/lldp_module.h"
#include "../modules/passive_discovery_module.h"
#include "../libraries/protocol_tools/neighbor_db.h"
#include "../libraries/protocol_tools/passive_history.h"

static void
    passive_discovery_button_callback(GuiButtonType type, InputType input_type, void* context);

static PassiveHistoryProtocol passive_history_filter(passive_protocol_t protocol) {
    switch(protocol) {
    case PassiveProtocolLLDP:
        return PassiveHistoryProtocolLldp;
    case PassiveProtocolCDP:
        return PassiveHistoryProtocolCdp;
    case PassiveProtocolEAPOL:
        return PassiveHistoryProtocolEapol;
    default:
        return PassiveHistoryProtocolAll;
    }
}

static void passive_discovery_finish_live(App* app) {
    passive_discovery_module_stop(app);
    if(app->passive_capture_operational) {
        /* Merge only this session's selected protocols, after RX has stopped. */
        passive_history_merge_live(
            app->storage, passive_history_filter(app->passive_discovery.protocol));
        app->passive_capture_operational = false;
    }
}

static bool passive_protocol_is_selectable(passive_protocol_t protocol) {
    return protocol == PassiveProtocolALL || protocol == PassiveProtocolLLDP ||
           protocol == PassiveProtocolCDP || protocol == PassiveProtocolEAPOL;
}

static const char* passive_protocol_name(passive_protocol_t protocol) {
    switch(protocol) {
    case PassiveProtocolALL:
        return "Discover All";

    case PassiveProtocolLLDP:
        return "LLDP";

    case PassiveProtocolCDP:
        return "CDP";

    case PassiveProtocolEAPOL:
        return "EAPOL";

    default:
        return "Discover All";
    }
}

static passive_protocol_t passive_protocol_next(passive_protocol_t protocol) {
    switch(protocol) {
    case PassiveProtocolALL:
        return PassiveProtocolLLDP;

    case PassiveProtocolLLDP:
        return PassiveProtocolCDP;

    case PassiveProtocolCDP:
        return PassiveProtocolEAPOL;

    case PassiveProtocolEAPOL:
        return PassiveProtocolALL;

    default:
        return PassiveProtocolALL;
    }
}

static void passive_discovery_draw_config(App* app) {
    widget_reset(app->widget);

    char protocol_text[32];

    passive_protocol_t protocol = app->passive_discovery.protocol;
    if(!passive_protocol_is_selectable(protocol)) {
        protocol = PassiveProtocolALL;
        app->passive_discovery.protocol = protocol;
    }

    snprintf(protocol_text, sizeof(protocol_text), "%s", passive_protocol_name(protocol));

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Protocol");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, protocol_text);

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Saved", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Start", passive_discovery_button_callback, app);

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Other", passive_discovery_button_callback, app);
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

static void passive_discovery_draw_status(App* app, const char* status, bool can_retry) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_multiline_element(
        app->widget, 64, 35, AlignCenter, AlignCenter, FontSecondary, status);

    if(can_retry) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Retry", passive_discovery_button_callback, app);
    }
}

static void passive_discovery_refresh(App* app) {
    switch(app->passive_discovery.state) {
    case PassiveDiscoveryStateConfig:

        passive_discovery_draw_config(app);
        break;

    case PassiveDiscoveryStateStarting:

        passive_discovery_draw_status(app, "Starting...", false);
        break;

    case PassiveDiscoveryStateListening:

        passive_discovery_draw_listening(app);
        break;

    case PassiveDiscoveryStateFinished:

        app->passive_selected_neighbor = 0;

        scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_list_option);

        break;

    case PassiveDiscoveryStateErrorDbMemory:

        passive_discovery_draw_status(
            app, "Not enough memory\nDatabase unavailable\nClose active services", true);
        break;

    case PassiveDiscoveryStateErrorWorkerMemory:

        passive_discovery_draw_status(
            app, "Not enough memory\nWorker unavailable\nClose active services", true);
        break;

    case PassiveDiscoveryStateErrorScannerMemory:

        passive_discovery_draw_status(
            app, "Not enough memory\nScanner unavailable\nClose active services", true);
        break;

    case PassiveDiscoveryStateErrorBusy:

        passive_discovery_draw_status(app, "Another operation\nis still active", true);
        break;

    case PassiveDiscoveryStateErrorDevice:

        passive_discovery_draw_status(app, "Device not\nconnected", true);
        break;

    case PassiveDiscoveryStateErrorLink:

        passive_discovery_draw_status(app, "Network not\ndetected", true);
        break;

    case PassiveDiscoveryStateErrorRxUnavailable:

        passive_discovery_draw_status(app, "Receiver unavailable\nTry again", true);
        break;

    default:
        break;
    }
}

void app_scene_passive_discovery_on_enter(void* context) {
    App* app = context;

    if(!app) {
        return;
    }

    if(app->passive_neighbor_source == PassiveNeighborSourceSaved) {
        passive_history_free(app->passive_history);
        app->passive_history = NULL;
        app->passive_neighbor_source = PassiveNeighborSourceLive;
    }

    app->passive_discovery.state = PassiveDiscoveryStateConfig;

    if(!passive_protocol_is_selectable(app->passive_discovery.protocol)) {
        app->passive_discovery.protocol = PassiveProtocolALL;
    }

    app->passive_discovery_stop = false;

    app->passive_neighbor_count = neighbor_db_count();

    passive_discovery_refresh(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_passive_discovery_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->passive_discovery.state == PassiveDiscoveryStateStarting ||
           app->passive_discovery.state == PassiveDiscoveryStateListening) {
            passive_discovery_finish_live(app);

            app->passive_discovery.state = PassiveDiscoveryStateConfig;

            passive_discovery_refresh(app);

            return true;
        }

        passive_discovery_finish_live(app);
        passive_history_free(app->passive_history);
        app->passive_history = NULL;
        neighbor_db_release();

        return scene_manager_previous_scene(app->scene_manager);
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PassiveDiscoveryEventRefresh) {
            passive_discovery_refresh(app);

            return true;
        }

        if(event.event == PassiveDiscoveryEventStarted) {
            if(app->passive_discovery.state == PassiveDiscoveryStateStarting) {
                app->passive_discovery.state = PassiveDiscoveryStateListening;
                passive_discovery_refresh(app);
            }
            return true;
        }

        passive_discovery_state_t error_state;
        switch(event.event) {
        case PassiveDiscoveryEventScannerLowMemory:
            error_state = PassiveDiscoveryStateErrorScannerMemory;
            break;
        case PassiveDiscoveryEventDeviceUnavailable:
            error_state = PassiveDiscoveryStateErrorDevice;
            break;
        case PassiveDiscoveryEventLinkUnavailable:
            error_state = PassiveDiscoveryStateErrorLink;
            break;
        case PassiveDiscoveryEventRxUnavailable:
            error_state = PassiveDiscoveryStateErrorRxUnavailable;
            break;
        default:
            return false;
        }

        passive_discovery_finish_live(app);
        app->passive_discovery.state = error_state;
        passive_discovery_refresh(app);
        return true;
    }

    return false;
}

void app_scene_passive_discovery_on_exit(void* context) {
    App* app = context;

    if(!app) {
        return;
    }

    app->passive_discovery_stop = true;

    passive_discovery_finish_live(app);

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
            neighbor_db_release();
            passive_history_free(app->passive_history);
            app->passive_history = passive_history_alloc(app->storage);
            app->passive_neighbor_source = PassiveNeighborSourceSaved;
            app->passive_selected_neighbor = 0;
            scene_manager_next_scene(app->scene_manager, app_scene_passive_neighbor_list_option);
        }

        break;

    case GuiButtonTypeRight:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig) {
            app->passive_discovery.protocol =
                passive_protocol_next(app->passive_discovery.protocol);
            passive_discovery_refresh(app);
        }

        break;

    case GuiButtonTypeCenter:

        if(app->passive_discovery.state == PassiveDiscoveryStateConfig ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorDbMemory ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorWorkerMemory ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorScannerMemory ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorBusy ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorDevice ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorLink ||
           app->passive_discovery.state == PassiveDiscoveryStateErrorRxUnavailable) {
            PassiveDiscoveryStartResult result;
            if(!neighbor_db_acquire()) {
                app->passive_discovery.state = PassiveDiscoveryStateErrorDbMemory;
                passive_discovery_refresh(app);
                break;
            } else {
                app->passive_neighbor_source = PassiveNeighborSourceLive;
                result = passive_discovery_module_start(app);
            }

            switch(result) {
            case PassiveDiscoveryStartPending:
                app->passive_discovery.state = PassiveDiscoveryStateStarting;
                break;
            case PassiveDiscoveryStartWorkerLowMemory:
                app->passive_discovery.state = PassiveDiscoveryStateErrorWorkerMemory;
                break;
            case PassiveDiscoveryStartOwnerBusy:
                app->passive_discovery.state = PassiveDiscoveryStateErrorBusy;
                break;
            default:
                app->passive_discovery.state = PassiveDiscoveryStateErrorBusy;
                break;
            }

            passive_discovery_refresh(app);

        } else if(app->passive_discovery.state == PassiveDiscoveryStateListening) {
            passive_discovery_finish_live(app);

            app->passive_discovery.state = PassiveDiscoveryStateFinished;

            passive_discovery_refresh(app);
        }

        break;

    default:
        break;
    }
}
