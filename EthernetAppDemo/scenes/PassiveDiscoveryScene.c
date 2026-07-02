#include "../app_user.h"

typedef enum {
    PassiveDiscoveryStateConfig,
    PassiveDiscoveryStateListening,
} passive_discovery_state_t;

typedef enum {
    PassiveProtocolLLDP = 0,
    PassiveProtocolCount,
} passive_protocol_t;

typedef struct {
    passive_discovery_state_t state;
    passive_protocol_t protocol;
} passive_discovery_context_t;

static void passive_discovery_draw_config(App* app) {
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Passive Discovery");

    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "Protocol");

    widget_add_string_element(
        app->widget, 64, 45, AlignCenter, AlignCenter, FontPrimary, "< LLDP >");

    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Start", NULL, app);
}

void app_scene_passive_discovery_on_enter(void* context) {
    App* app = context;

    passive_discovery_draw_config(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool app_scene_passive_discovery_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void app_scene_passive_discovery_on_exit(void* context) {
    App* app = context;

    widget_reset(app->widget);
}
