#include "../app_user.h"
#include "../libraries/functions/startup_guard.h"

#define PACKET_POSITIONS_MAX           2000U
#define READ_PCAP_STACK_BYTES          4096U
#define READ_PCAP_RENDER_TEXT_CAPACITY 512U

static uint32_t packet_count = 0;

typedef enum {
    ReadPcapEventTextUpdated = 1,
    ReadPcapEventFileUnavailable,
    ReadPcapEventReadUnavailable,
} ReadPcapCustomEvent;

static int32_t thread_read_pcaps(void* context);
static void read_pcap_start(void* context);

static void read_pcap_release_index(App* app) {
    free(app->packet_positions);
    app->packet_positions = NULL;
    packet_count = 0;
}

static void read_pcap_show_error(App* app, const char* message) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 16, AlignCenter, AlignCenter, FontPrimary, "PCAP unavailable");
    widget_add_string_multiline_element(
        app->widget, 64, 39, AlignCenter, AlignCenter, FontSecondary, message);
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

static void read_pcap_show_low_memory(App* app) {
    startup_guard_show_diagnostic(app, read_pcap_start);
}

static void read_pcap_start(void* context) {
    App* app = context;

    startup_guard_clear(app);
    widget_reset(app->widget);

    if(furi_string_empty(app->path) ||
       !storage_file_exists(app->storage, furi_string_get_cstr(app->path))) {
        read_pcap_show_error(app, "File unavailable");
        return;
    }

    if(!startup_guard_thread_slot_available(app, read_pcap_start)) return;

    const size_t index_payload = PACKET_POSITIONS_MAX * sizeof(*app->packet_positions);
    const size_t index_block = startup_guard_allocation_block_size(index_payload);
    const size_t render_text_block =
        startup_guard_allocation_block_size(READ_PCAP_RENDER_TEXT_CAPACITY);

    /*
     * The index remains resident while the reader thread is allocated. Check
     * the complete startup sequence before the fatal-only index allocation.
     */
    StartupGuardRequirements full_requirements = startup_guard_thread_requirements(
        READ_PCAP_STACK_BYTES, index_block + render_text_block, index_block);
    if(startup_guard_check_capture(
           &app->startup_diagnostic, full_requirements, StartupDiagnosticBoundaryReadPcapStart) !=
       StartupGuardReady) {
        read_pcap_show_low_memory(app);
        return;
    }

    furi_assert(app->packet_positions == NULL);
    app->packet_positions = malloc(index_payload);
    if(!app->packet_positions) {
        startup_guard_capture_allocation_failure(
            &app->startup_diagnostic, full_requirements, StartupDiagnosticBoundaryReadPcapStart);
        read_pcap_show_low_memory(app);
        return;
    }

    packet_count = pcap_scan(
        app->file, furi_string_get_cstr(app->path), app->packet_positions, PACKET_POSITIONS_MAX);
    if(packet_count == 0) {
        read_pcap_release_index(app);
        read_pcap_show_error(app, "Invalid or empty PCAP");
        return;
    }

    /* pcap_scan may run long enough for another service to change heap state. */
    StartupGuardRequirements remaining_requirements = startup_guard_thread_requirements(
        READ_PCAP_STACK_BYTES, render_text_block, render_text_block);
    if(startup_guard_check_capture(
           &app->startup_diagnostic,
           remaining_requirements,
           StartupDiagnosticBoundaryReadPcapPostIndex) != StartupGuardReady) {
        read_pcap_release_index(app);
        read_pcap_show_low_memory(app);
        return;
    }

    furi_string_reserve(app->text, READ_PCAP_RENDER_TEXT_CAPACITY);

    StartupGuardRequirements thread_requirements =
        startup_guard_thread_requirements(READ_PCAP_STACK_BYTES, 0U, 0U);
    if(startup_guard_check_capture(
           &app->startup_diagnostic,
           thread_requirements,
           StartupDiagnosticBoundaryReadPcapWorker) != StartupGuardReady) {
        read_pcap_release_index(app);
        read_pcap_show_low_memory(app);
        return;
    }

    FuriThread* thread =
        furi_thread_alloc_ex("PCAP reader", READ_PCAP_STACK_BYTES, thread_read_pcaps, app);
    if(!thread) {
        startup_guard_capture_allocation_failure(
            &app->startup_diagnostic,
            thread_requirements,
            StartupDiagnosticBoundaryReadPcapWorker);
        read_pcap_release_index(app);
        read_pcap_show_low_memory(app);
        return;
    }
    if(!app_thread_claim(app, AppThreadOwnerReadPcaps, thread)) {
        read_pcap_release_index(app);
        startup_guard_thread_slot_available(app, read_pcap_start);
        return;
    }

    text_box_reset(app->text_box);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_font(app->text_box, TextBoxFontText);
    furi_string_reset(app->text);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text));
    view_dispatcher_switch_to_view(app->view_dispatcher, TextBoxView);

    furi_thread_start(thread);
}

void app_scene_read_pcap_on_enter(void* context) {
    read_pcap_start(context);
}

bool app_scene_read_pcap_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack && app->read_pcap_from_sniffer) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, app_scene_pentest_menu_option);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ReadPcapEventTextUpdated:
            text_box_set_text(app->text_box, furi_string_get_cstr(app->text));
            return true;
        case ReadPcapEventFileUnavailable:
            app_thread_join_and_free(app, AppThreadOwnerReadPcaps);
            read_pcap_release_index(app);
            read_pcap_show_error(app, "File unavailable");
            return true;
        case ReadPcapEventReadUnavailable:
            app_thread_join_and_free(app, AppThreadOwnerReadPcaps);
            read_pcap_release_index(app);
            read_pcap_show_error(app, "PCAP read error");
            return true;
        default:
            break;
        }
    }

    return false;
}

void app_scene_read_pcap_on_exit(void* context) {
    App* app = context;

    startup_guard_clear(app);
    app->thread_shutdown_requested = true;
    app_thread_join_and_free(app, AppThreadOwnerReadPcaps);
    read_pcap_release_index(app);
    app->read_pcap_from_sniffer = false;
}

static int32_t thread_read_pcaps(void* context) {
    App* app = context;
    uint8_t* buffer = app->ethernet->tx_buffer;
    const size_t buffer_capacity = MAX_FRAMELEN;
    uint32_t counter = 0;
    bool write_once = true;
    bool read_failed = false;

    if(!storage_file_open(
           app->file, furi_string_get_cstr(app->path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ReadPcapEventFileUnavailable);
        return 0;
    }

    while(!app->thread_shutdown_requested && furi_hal_gpio_read(&gpio_button_back)) {
        if(!furi_hal_gpio_read(&gpio_button_left)) {
            while(!app->thread_shutdown_requested && !furi_hal_gpio_read(&gpio_button_left)) {
                furi_delay_ms(1);
            }
            if(counter > 0) counter--;
            write_once = true;
        }

        if(!furi_hal_gpio_read(&gpio_button_right)) {
            while(!app->thread_shutdown_requested && !furi_hal_gpio_read(&gpio_button_right)) {
                furi_delay_ms(1);
            }
            if(counter + 1U < packet_count) counter++;
            write_once = true;
        }

        if(write_once) {
            const uint32_t len = pcap_get_specific_packet(
                app->file, buffer, buffer_capacity, app->packet_positions[counter]);
            if(len == 0) {
                read_failed = true;
                break;
            }

            furi_string_reset(app->text);
            furi_string_cat_printf(
                app->text, "<== Packet %lu of %lu ==>\n", counter + 1, packet_count);
            print_packet_info(app->text, buffer, len);
            view_dispatcher_send_custom_event(app->view_dispatcher, ReadPcapEventTextUpdated);
            write_once = false;
        }

        furi_delay_ms(1);
    }

    storage_file_close(app->file);
    if(read_failed && !app->thread_shutdown_requested) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ReadPcapEventReadUnavailable);
    }

    return 0;
}
