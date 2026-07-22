#include "passive_discovery_module.h"
#include "passive_protocol_handler.h"
#include "lldp_module.h"
#include "cdp_module.h"
#include "eapol_module.h"
#include <stdio.h>

// Forward declaration of the thread worker function
static int32_t passive_discovery_thread(void* context);

static const PassiveProtocolHandler* const protocol_handlers[PassiveProtocolCount] = {
    [PassiveProtocolALL] = NULL, // ALL is virtual
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

static bool all_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);

    bool processed = false;

    // Recorre automáticamente todos los protocolos registrados
    for(size_t i = 0; i < PassiveProtocolCount; i++) {
        if(i == PassiveProtocolALL || i == PassiveProtocolClearAll) {
            continue;
        }

        const PassiveProtocolHandler* handler = protocol_handlers[i];

        if(handler && handler->process_frame) {
            if(handler->process_frame((uint8_t*)frame, len)) {
                processed = true;
            }
        }
    }

    return processed;
}

static bool all_run(scanner_session_t* session, uint32_t timeout_ms) {
    uint16_t length = 0;

    return scanner_wait_for_packet(
        session, all_packet_predicate, NULL, NULL, NULL, &length, timeout_ms);
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

    if(app->passive_discovery.protocol == PassiveProtocolALL) {
        /* Inicializar todos los protocolos registrados */
        for(size_t i = 0; i < PassiveProtocolCount; i++) {
            if(i == PassiveProtocolALL || i == PassiveProtocolClearAll) {
                continue;
            }

            const PassiveProtocolHandler* h = protocol_handlers[i];

            if(h && h->init) {
                h->init(app);
            }
        }
    } else if(handler && handler->init) {
        handler->init(app);
    }

    while(!app->passive_discovery_stop) {
        //FURI_LOG_I("PASSIVE", "Protocol=%u", app->passive_discovery.protocol);
        if(app->passive_discovery.protocol == PassiveProtocolALL) {
            all_run(&session, 500);
        } else if(handler && handler->run) {
            handler->run(&session, 500);
        } else {
            furi_delay_ms(100);
        }

        if(app->passive_discovery.protocol == PassiveProtocolALL) {
            all_run(&session, 500);
        } else if(handler && handler->run) {
            handler->run(&session, 500);
        } else {
            furi_delay_ms(100);
        }

        uint16_t count;

        if(app->passive_discovery.protocol == PassiveProtocolALL) {
            count = neighbor_db_count();
        } else if(handler && handler->get_neighbor_count) {
            count = handler->get_neighbor_count();
        } else {
            count = 0;
        }

        if(count != app->passive_neighbor_count) {
            app->passive_neighbor_count = count;
            view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        }
    }

    if(app->passive_discovery.protocol == PassiveProtocolALL) {
        /* Limpiar todos los protocolos registrados */
        for(size_t i = 0; i < PassiveProtocolCount; i++) {
            if(i == PassiveProtocolALL || i == PassiveProtocolClearAll) {
                continue;
            }

            const PassiveProtocolHandler* h = protocol_handlers[i];

            if(h && h->cleanup) {
                h->cleanup(app);
            }
        }
    } else if(handler && handler->cleanup) {
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
    case PassiveProtocolALL:
        return "Discover All";

    case PassiveProtocolClearAll:
        return "Clear All";

    case PassiveProtocolLLDP:
        return "LLDP";

    case PassiveProtocolCDP:
        return "CDP";

    case PassiveProtocolEAPOL:
        return "EAPOL";

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
