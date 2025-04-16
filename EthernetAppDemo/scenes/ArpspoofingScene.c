#include "../app_user.h"

/**
 * In this file you will find the ARP Spoofing scene and it worker,
 * for the usage of I played with the view dispatcher and the thread to show different
 * views for not use many scenes and avoid to define many
 */

// ArpSpoofing thread
int32_t arpspoofing_thread(void* context);

// ArpSpoofing on enter
void app_scene_arp_spoofing_on_enter(void* context) {
    App* app = (App*)context;
    app->thread = furi_thread_alloc_ex("ArpSpoofing", 2 * 1024, arpspoofing_thread, app);
}

// ArpSpoofing on event
bool app_scene_arp_spoofing_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// ArpSpoofing on exit
void app_scene_arp_spoofing_on_exit(void* context) {
    App* app = (App*)context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Main Thread for the ArpSpoofing
 * Here is where the main code comes for the worker
 */
int32_t arpspoofing_thread(void* context) {
    App* app = (App*)context;

    enc28j60_t* ethernet = app->ethernet;

    widget_reset(app->widget);

    bool start = enc28j60_start(ethernet) != 0xff;

    UNUSED(start);

    return 0;
}
