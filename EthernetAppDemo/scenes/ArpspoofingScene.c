#include "../app_user.h"

// ArpSpoofing on enter
void app_scene_arp_spoofing_on_enter(void* context) {
    UNUSED(context);
}

// ArpSpoofing on event
bool app_scene_arp_spoofing_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

void app_scene_arp_spoofing_on_exit(void* context) {
    App* app = (App*)context;
    UNUSED(app);
}
