#include "passive_discovery_module.h"
#include "passive_discovery_handler.h"
#include "cdp_module.h"
#include "eapol_module.h"
#include "lldp_module.h"
#include "../libraries/functions/startup_guard.h"
#include <stdio.h>

#define PASSIVE_DISCOVERY_STACK_BYTES 3072U

/*
 * Heap blocks used by furi_thread_alloc_ex() on API 87.1:
 *   align_up(stack payload + 8-byte allocator header, 8-byte alignment)
 *   + 224 FuriThread + 48 two FuriStrings + 24 app id + 32 thread name.
 * With the 3072-byte stack this is 3080 + 328 = 3408 bytes.
 * The first scanner semaphore is another 96-byte heap block. Preserve an
 * additional fixed 1024 bytes in total-free headroom for allocator/API
 * variation and concurrent small service allocations. The reserve is not
 * itself allocated and therefore does not belong in a max-block requirement.
 *
 * The allocator is address-ordered first-fit, and the 328 bytes of thread
 * metadata are allocated before the stack block. Requiring one combined
 * metadata-plus-stack free block covers the worst case where all metadata
 * consumes the same block that must subsequently hold the stack. The scanner
 * semaphore is checked separately immediately before each wait.
 */
// Forward declaration of the thread worker function
static int32_t passive_discovery_thread(void* context);

typedef struct {
    App* app;
    bool operational;
    bool registered;
} PassiveDiscoveryWaitState;

// --- Protocol Registry Lookup Table ---

static const PassiveProtocolHandler* const protocol_handlers[PassiveProtocolCount] = {
    [PassiveProtocolLLDP] = &lldp_protocol_handler,
    [PassiveProtocolCDP] = &cdp_protocol_handler,
    [PassiveProtocolEAPOL] = &eapol_protocol_handler,
};

static const PassiveProtocolHandler* get_handler(passive_protocol_t protocol) {
    if(protocol >= PassiveProtocolCount) {
        return NULL;
    }
    return protocol_handlers[protocol];
}

static bool passive_discovery_handler_is_selected(
    passive_protocol_t selected_protocol,
    passive_protocol_t handler_protocol) {
    return selected_protocol == PassiveProtocolALL || selected_protocol == handler_protocol;
}

static size_t passive_discovery_neighbor_count(passive_protocol_t selected_protocol) {
    switch(selected_protocol) {
    case PassiveProtocolLLDP:
        return neighbor_db_count_by_source(NEIGHBOR_SOURCE_LLDP);

    case PassiveProtocolCDP:
        return neighbor_db_count_by_source(NEIGHBOR_SOURCE_CDP);

    case PassiveProtocolEAPOL:
        return neighbor_db_count_by_source(NEIGHBOR_SOURCE_EAPOL);

    case PassiveProtocolALL:
        return neighbor_db_count();

    default:
        return 0;
    }
}

static void passive_discovery_handlers_init(App* app, passive_protocol_t selected_protocol) {
    for(passive_protocol_t protocol = PassiveProtocolLLDP; protocol < PassiveProtocolCount;
        protocol++) {
        const PassiveProtocolHandler* handler = get_handler(protocol);
        if(handler && handler->process_frame && handler->init &&
           passive_discovery_handler_is_selected(selected_protocol, protocol)) {
            handler->init(app);
        }
    }
}

static void passive_discovery_handlers_cleanup(App* app, passive_protocol_t selected_protocol) {
    for(passive_protocol_t protocol = PassiveProtocolLLDP; protocol < PassiveProtocolCount;
        protocol++) {
        const PassiveProtocolHandler* handler = get_handler(protocol);
        if(handler && handler->process_frame && handler->cleanup &&
           passive_discovery_handler_is_selected(selected_protocol, protocol)) {
            handler->cleanup(app);
        }
    }
}

static bool
    passive_discovery_dispatch_frame(const uint8_t* frame, uint16_t length, void* context) {
    passive_protocol_t selected_protocol = *(passive_protocol_t*)context;
    bool matched = false;

    for(passive_protocol_t protocol = PassiveProtocolLLDP; protocol < PassiveProtocolCount;
        protocol++) {
        const PassiveProtocolHandler* handler = get_handler(protocol);
        if(handler && handler->process_frame &&
           passive_discovery_handler_is_selected(selected_protocol, protocol)) {
            if(handler->process_frame((uint8_t*)frame, length)) {
                matched = true;
            }
        }
    }

    return matched;
}

static bool passive_discovery_has_startup_headroom(void) {
    StartupGuardRequirements requirements = startup_guard_thread_requirements(
        PASSIVE_DISCOVERY_STACK_BYTES,
        STARTUP_GUARD_SCANNER_SEMAPHORE_HEAP_BYTES,
        STARTUP_GUARD_SCANNER_SEMAPHORE_HEAP_BYTES);
    return startup_guard_check(requirements) == StartupGuardReady;
}

static bool passive_discovery_has_wait_headroom(void) {
    StartupGuardRequirements requirements = {
        .required_total_free =
            STARTUP_GUARD_SCANNER_SEMAPHORE_HEAP_BYTES + STARTUP_GUARD_RESERVE_BYTES,
        .required_max_block = STARTUP_GUARD_SCANNER_SEMAPHORE_HEAP_BYTES,
    };
    return startup_guard_check(requirements) == StartupGuardReady;
}

static void passive_discovery_rx_registered(void* context) {
    PassiveDiscoveryWaitState* state = context;
    state->registered = true;
    state->app->passive_capture_operational = true;

    if(!state->operational) {
        state->operational = true;
        view_dispatcher_send_custom_event(
            state->app->view_dispatcher, PassiveDiscoveryEventStarted);
    }
}

// --- Background Scanning Thread ---

static int32_t passive_discovery_thread(void* context) {
    printf("PASSIVE THREAD ENTERED\n");
    App* app = context;
    enc28j60_t* ethernet = app->ethernet;

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff;
        app->enc28j60_connected = start;
    }

    if(!start) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, PassiveDiscoveryEventDeviceUnavailable);
        return 0;
    }

    if(!is_link_up(ethernet)) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, PassiveDiscoveryEventLinkUnavailable);
        return 0;
    }

    scanner_session_t session;
    scanner_session_init(&session, app);
    scanner_session_set_cancel_flag(&session, &app->passive_discovery_stop);

    passive_protocol_t selected_protocol = app->passive_discovery.protocol;

    enable_multicast(ethernet);
    passive_discovery_handlers_init(app, selected_protocol);

    app->passive_neighbor_count = passive_discovery_neighbor_count(selected_protocol);
    view_dispatcher_send_custom_event(app->view_dispatcher, PassiveDiscoveryEventRefresh);

    PassiveDiscoveryWaitState wait_state = {
        .app = app,
        .operational = false,
        .registered = false,
    };
    uint32_t exit_event = 0;

    while(!app->passive_discovery_stop && !scanner_cancel_requested(&session)) {
        if(!passive_discovery_has_wait_headroom()) {
            exit_event = PassiveDiscoveryEventScannerLowMemory;
            break;
        }

        uint16_t length = 0;
        wait_state.registered = false;
        bool result = scanner_wait_for_packet(
            &session,
            passive_discovery_dispatch_frame,
            &selected_protocol,
            passive_discovery_rx_registered,
            &wait_state,
            &length,
            500);

        if(!wait_state.registered) {
            if(scanner_wait_failure_is_memory(scanner_session_get_last_wait_failure(&session))) {
                exit_event = PassiveDiscoveryEventScannerLowMemory;
            } else {
                exit_event = PassiveDiscoveryEventRxUnavailable;
            }
            break;
        }

        if(result) {
            FURI_LOG_I("PASSIVE", "Packet processed by selected handler");
        }

        uint16_t count = passive_discovery_neighbor_count(selected_protocol);
        if(count != app->passive_neighbor_count) {
            app->passive_neighbor_count = count;
            view_dispatcher_send_custom_event(app->view_dispatcher, PassiveDiscoveryEventRefresh);
        }
    }

    passive_discovery_handlers_cleanup(app, selected_protocol);
    disable_multicast(ethernet);
    scanner_session_deinit(&session);

    if(exit_event && !app->passive_discovery_stop) {
        view_dispatcher_send_custom_event(app->view_dispatcher, exit_event);
    }

    return 0;
}

// --- Public APIs implementation ---

PassiveDiscoveryStartResult passive_discovery_module_start(App* app) {
    if(!app || app->thread_alternative ||
       app_thread_is_owned(app, AppThreadOwnerPassiveDiscovery)) {
        return PassiveDiscoveryStartOwnerBusy;
    }

    if(!passive_discovery_has_startup_headroom()) {
        return PassiveDiscoveryStartWorkerLowMemory;
    }

    app->passive_discovery_stop = false;
    app->passive_capture_operational = false;
    FuriThread* thread = furi_thread_alloc_ex(
        "Passive Discovery", PASSIVE_DISCOVERY_STACK_BYTES, passive_discovery_thread, app);
    if(!thread) {
        return PassiveDiscoveryStartWorkerLowMemory;
    }
    if(!app_thread_claim(app, AppThreadOwnerPassiveDiscovery, thread)) {
        return PassiveDiscoveryStartOwnerBusy;
    }

    furi_thread_start(thread);
    return PassiveDiscoveryStartPending;
}

void passive_discovery_module_stop(App* app) {
    if(!app) {
        return;
    }

    app->passive_discovery_stop = true;

    app_thread_join_and_free(app, AppThreadOwnerPassiveDiscovery);
}

size_t passive_discovery_module_get_protocol_count(void) {
    return PassiveProtocolCount;
}

const char* passive_discovery_module_get_protocol_name(passive_protocol_t protocol) {
    if(protocol == PassiveProtocolALL) {
        return "Discover All";
    }

    const PassiveProtocolHandler* handler = get_handler(protocol);
    if(handler && handler->get_display_name) {
        return handler->get_display_name();
    }
    return "Unknown";
}

uint8_t passive_discovery_module_get_details_page_count(
    passive_protocol_t protocol,
    neighbor_t* neighbor) {
    const PassiveProtocolHandler* handler = get_handler(protocol);
    if(handler && handler->get_details_page_count) {
        return handler->get_details_page_count(neighbor);
    }
    return 1;
}

void passive_discovery_module_build_details_page(
    passive_protocol_t protocol,
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size) {
    const PassiveProtocolHandler* handler = get_handler(protocol);
    if(handler && handler->build_details_page) {
        handler->build_details_page(
            neighbor,
            page,
            line1,
            line1_size,
            line2,
            line2_size,
            line3,
            line3_size,
            line4,
            line4_size);
    } else {
        snprintf(line1, line1_size, "No handler");
        line2[0] = '\0';
        line3[0] = '\0';
        line4[0] = '\0';
    }
}

size_t passive_discovery_module_get_neighbor_count(void) {
    return neighbor_db_count();
}

neighbor_t* passive_discovery_module_get_neighbor(size_t index) {
    return neighbor_db_get(index);
}
