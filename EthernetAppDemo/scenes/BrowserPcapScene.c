#include "../app_user.h"
#include "../libraries/functions/startup_guard.h"

#define PCAP_BROWSER_WORKER_STACK_BYTES   2048U
#define PCAP_BROWSER_SDK_FIXED_HEAP_BYTES 424U
#define PCAP_BROWSER_RESULT_PATH_CAPACITY 320U
#define PCAP_BROWSER_ROOT_TEXT_CAPACITY   64U

/**
 * This scene it is only to load and get pcap paths to display
 */

void file_browser_callback(void* context) {
    App* app = (App*)context;

    app->read_pcap_from_sniffer = false;
    scene_manager_next_scene(app->scene_manager, app_scene_read_pcap_option);
}

static void pcap_browser_start(void* context) {
    App* app = (App*)context;

    startup_guard_clear(app);
    widget_reset(app->widget);

    const size_t result_path_block =
        startup_guard_allocation_block_size(PCAP_BROWSER_RESULT_PATH_CAPACITY);
    const size_t root_text_block =
        startup_guard_allocation_block_size(PCAP_BROWSER_ROOT_TEXT_CAPACITY);
    StartupGuardRequirements requirements = startup_guard_thread_requirements(
        PCAP_BROWSER_WORKER_STACK_BYTES,
        PCAP_BROWSER_SDK_FIXED_HEAP_BYTES + result_path_block + root_text_block,
        result_path_block);
    if(startup_guard_check(requirements) != StartupGuardReady) {
        startup_guard_show_low_memory(
            app,
            "View packets unavailable\nClose active services\nand try again",
            pcap_browser_start);
        return;
    }

    /* Keep FileBrowser-owned result writes inside a preflighted buffer. */
    furi_string_reserve(app->path, PCAP_BROWSER_RESULT_PATH_CAPACITY);
    furi_string_reserve(app->text, PCAP_BROWSER_ROOT_TEXT_CAPACITY);

    file_browser_configure(app->file_browser, ".pcap", PATHPCAPS, true, true, NULL, true);
    file_browser_set_callback(app->file_browser, file_browser_callback, app);

    furi_string_reset(app->text);
    furi_string_cat(app->text, PATHPCAPS);
    file_browser_start(app->file_browser, app->text);
    app->pcap_browser_started = true;

    view_dispatcher_switch_to_view(app->view_dispatcher, FileBrowserView);
}

// Function for the testing scene on enter
void app_scene_browser_pcaps_on_enter(void* context) {
    App* app = context;
    app->pcap_browser_started = false;
    pcap_browser_start(app);
}

// Function for the testing scene on event
bool app_scene_browser_pcaps_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_browser_pcaps_on_exit(void* context) {
    App* app = (App*)context;
    startup_guard_clear(app);
    if(app->pcap_browser_started) {
        file_browser_stop(app->file_browser);
        app->pcap_browser_started = false;
    }
}
