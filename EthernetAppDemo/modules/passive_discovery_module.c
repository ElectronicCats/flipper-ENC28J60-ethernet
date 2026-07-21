#include "passive_discovery_module.h"
#include "passive_protocol_handler.h"
#include "lldp_module.h"
#include "cdp_module.h"
#include "eapol_module.h"
#include <stdio.h>

// Forward declaration of the thread worker function
static int32_t passive_discovery_thread(void* context);

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
        furi_delay_ms(1500);
        return 0;
    }

    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        furi_delay_ms(1500);
        return 0;
    }

    rx_dispatch_init(app);

    scanner_session_t session;
    scanner_session_init(&session, app);

    const PassiveProtocolHandler* handler = get_handler(app->passive_discovery.protocol);

    if(handler && handler->init) {
        handler->init(app);
    }

    while(!app->passive_discovery_stop) {
        //FURI_LOG_I("PASSIVE", "Protocol=%u", app->passive_discovery.protocol);
        if(handler && handler->run) {
            bool result = handler->run(&session, 500);
            if(result) {
                //FURI_LOG_I("PASSIVE", "Packet processed by active handler");
            }
        } else {
            furi_delay_ms(100);
        }

        neighbor_source_t source;

        switch(app->passive_discovery.protocol) {
        case PassiveProtocolLLDP:
            source = NEIGHBOR_SOURCE_LLDP;
            break;

        case PassiveProtocolCDP:
            source = NEIGHBOR_SOURCE_CDP;
            break;

        case PassiveProtocolEAPOL:
            source = NEIGHBOR_SOURCE_EAPOL;
            break;

        default:
            source = 0;
            break;
        }

        uint16_t count = neighbor_db_count_by_source(source);

        if(count != app->passive_neighbor_count) {
            app->passive_neighbor_count = count;
            view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        }
    }

    if(handler && handler->cleanup) {
        handler->cleanup(app);
    }

    scanner_session_deinit(&session);

    return 0;
}

// --- Public APIs implementation ---

void passive_discovery_module_start(App* app) {
    if(app->thread_alternative) {
        return;
    }

    app->passive_discovery_stop = false;
    app->thread_alternative =
        furi_thread_alloc_ex("Passive Discovery", 4096, passive_discovery_thread, app);
    furi_thread_start(app->thread_alternative);
}

void passive_discovery_module_stop(App* app) {
    app->passive_discovery_stop = true;

    if(app->thread_alternative) {
        furi_thread_join(app->thread_alternative);
        furi_thread_free(app->thread_alternative);
        app->thread_alternative = NULL;
    }
}

size_t passive_discovery_module_get_protocol_count(void) {
    return PassiveProtocolCount;
}

const char* passive_discovery_module_get_protocol_name(passive_protocol_t protocol) {
    switch(protocol) {
    case PassiveProtocolLLDP:
        return "LLDP";

    case PassiveProtocolCDP:
        return "CDP";

    case PassiveProtocolEAPOL:
        return "EAPOL";

    case PassiveProtocolClearAll:
        return "Clear All";

    default:
        return "UNKNOWN";
    }
}

neighbor_t* passive_discovery_module_get_neighbor(size_t index) {
    return neighbor_db_get(index);
}

size_t passive_discovery_module_get_neighbor_count(void) {
    return neighbor_db_count();
}
