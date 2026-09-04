#include "passive_discovery_module.h"
#include "passive_discovery_handler.h"
#include "cdp_module.h"
#include "lldp_module.h"
#include <stdio.h>

// Forward declaration of the thread worker function
static int32_t passive_discovery_thread(void* context);

// --- Protocol Registry Lookup Table ---

static const PassiveProtocolHandler* const protocol_handlers[PassiveProtocolCount] = {
    [PassiveProtocolLLDP] = &lldp_protocol_handler,
    [PassiveProtocolCDP] = &cdp_protocol_handler,
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
        draw_device_no_connected(app);
        furi_delay_ms(300);
        return 0;
    }

    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        furi_delay_ms(300);
        return 0;
    }

    scanner_session_t session;
    scanner_session_init(&session, app);
    scanner_session_set_cancel_flag(&session, &app->passive_discovery_stop);

    passive_protocol_t selected_protocol = app->passive_discovery.protocol;

    enable_multicast(ethernet);
    passive_discovery_handlers_init(app, selected_protocol);

    app->passive_neighbor_count = passive_discovery_neighbor_count(selected_protocol);
    view_dispatcher_send_custom_event(app->view_dispatcher, 1);

    while(!app->passive_discovery_stop && !scanner_cancel_requested(&session)) {
        uint16_t length = 0;
        bool result = scanner_wait_for_packet(
            &session,
            passive_discovery_dispatch_frame,
            &selected_protocol,
            NULL,
            NULL,
            &length,
            500);
        if(result) {
            FURI_LOG_I("PASSIVE", "Packet processed by selected handler");
        }

        uint16_t count = passive_discovery_neighbor_count(selected_protocol);
        if(count != app->passive_neighbor_count) {
            app->passive_neighbor_count = count;
            view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        }
    }

    passive_discovery_handlers_cleanup(app, selected_protocol);
    disable_multicast(ethernet);
    scanner_session_deinit(&session);

    return 0;
}

// --- Public APIs implementation ---

void passive_discovery_module_start(App* app) {
    if(app_thread_is_owned(app, AppThreadOwnerPassiveDiscovery)) {
        return;
    }

    app->passive_discovery_stop = false;
    FuriThread* thread =
        furi_thread_alloc_ex("Passive Discovery", 4096, passive_discovery_thread, app);
    if(app_thread_claim(app, AppThreadOwnerPassiveDiscovery, thread)) {
        furi_thread_start(thread);
    }
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
