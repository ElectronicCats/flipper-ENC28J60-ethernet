#include "startup_guard.h"

#include <stdint.h>

static size_t startup_guard_add_saturating(size_t left, size_t right) {
    return left > SIZE_MAX - right ? SIZE_MAX : left + right;
}

static size_t startup_guard_align_up(size_t value) {
    if(value > SIZE_MAX - (STARTUP_GUARD_HEAP_ALIGNMENT_BYTES - 1U)) return SIZE_MAX;
    return (value + STARTUP_GUARD_HEAP_ALIGNMENT_BYTES - 1U) &
           ~(STARTUP_GUARD_HEAP_ALIGNMENT_BYTES - 1U);
}

size_t startup_guard_allocation_block_size(size_t payload_bytes) {
    return startup_guard_align_up(
        startup_guard_add_saturating(payload_bytes, STARTUP_GUARD_HEAP_BLOCK_HEADER_BYTES));
}

StartupGuardRequirements startup_guard_thread_requirements(
    size_t stack_bytes,
    size_t additional_total_bytes,
    size_t additional_max_block_bytes) {
    const size_t stack_heap_bytes = startup_guard_allocation_block_size(stack_bytes);
    const size_t thread_heap_bytes =
        startup_guard_add_saturating(STARTUP_GUARD_THREAD_METADATA_HEAP_BYTES, stack_heap_bytes);

    StartupGuardRequirements requirements = {
        .required_total_free = startup_guard_add_saturating(
            startup_guard_add_saturating(thread_heap_bytes, additional_total_bytes),
            STARTUP_GUARD_RESERVE_BYTES),
        .required_max_block = thread_heap_bytes > additional_max_block_bytes ?
                                  thread_heap_bytes :
                                  additional_max_block_bytes,
    };
    return requirements;
}

StartupGuardResult startup_guard_check(StartupGuardRequirements requirements) {
    /* Preflight is advisory: another service may allocate after this check. */
    if(memmgr_get_free_heap() < requirements.required_total_free) {
        return StartupGuardInsufficientTotal;
    }
    if(memmgr_heap_get_max_free_block() < requirements.required_max_block) {
        return StartupGuardInsufficientBlock;
    }
    return StartupGuardReady;
}

static void startup_guard_retry_button(GuiButtonType type, InputType input_type, void* context) {
    App* app = context;
    if(type != GuiButtonTypeCenter || input_type != InputTypeShort ||
       !app->startup_retry_callback) {
        return;
    }

    startup_retry_callback_t retry_callback = app->startup_retry_callback;
    app->startup_retry_callback = NULL;
    retry_callback(app);
}

static void startup_guard_show_status(
    App* app,
    const char* title,
    const char* reason,
    startup_retry_callback_t retry_callback) {
    app->startup_retry_callback = retry_callback;
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignCenter, FontPrimary, title);
    widget_add_string_multiline_element(
        app->widget, 64, 35, AlignCenter, AlignCenter, FontSecondary, reason);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Retry", startup_guard_retry_button, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WidgetView);
}

bool startup_guard_thread_slot_available(App* app, startup_retry_callback_t retry_callback) {
    if(!app->thread_alternative && app->thread_alternative_owner == AppThreadOwnerNone) {
        return true;
    }

    startup_guard_show_status(
        app, "Feature unavailable", "Another operation\nis still active", retry_callback);
    return false;
}

void startup_guard_show_low_memory(
    App* app,
    const char* reason,
    startup_retry_callback_t retry_callback) {
    startup_guard_show_status(app, "Not enough memory", reason, retry_callback);
}

void startup_guard_clear(App* app) {
    if(app) app->startup_retry_callback = NULL;
}
