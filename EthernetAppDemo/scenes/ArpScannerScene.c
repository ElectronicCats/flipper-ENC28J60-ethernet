#include "../app_user.h"

/**
 * This file contains the functions to display and work with the ARP SCANNER
 * It shows the IP list and saves it in a array
 */

// Function for the thread
int32_t arp_scanner_thread(void* context);

// function on enter for the arp scanner scene
void app_scene_arp_scanner_on_enter(void* context) {
    App* app = (App*)context;
    app->thread = furi_thread_alloc_ex("ARP SCANNER", 10 * 1024, arp_scanner_thread, app);
    furi_thread_start(app->thread);
}

// function on event for the arp scanner scene
bool app_scene_arp_scanner_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// function on exit for the arp scanner scene
void app_scene_arp_scanner_on_exit(void* context) {
    App* app = (App*)context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
}

/**
 * Thread for the ARP Scanner
 */

int32_t arp_scanner_thread(void* context) {
    App* app = (App*)context;

    UNUSED(app);

    return 0;
}
