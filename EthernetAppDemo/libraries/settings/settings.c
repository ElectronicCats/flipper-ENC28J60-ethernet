#include "settings.h"
#include <flipper_format/flipper_format.h>

#define SETTINGS_FILETYPE "Flipper ENC28J60 Ethernet App Settings"
// F0.5f — bumped from 1 to 2 to add gateway/mac_gateway/is_dora.
// load() of older v1 files still works (extra fields are just absent).
#define SETTINGS_VERSION  3
#define SETTINGS_PATH     PATHAPPEXT "/settings.cfg"

static bool settings_ipv4_is_nonzero(const uint8_t address[4]) {
    return address[0] || address[1] || address[2] || address[3];
}

static bool settings_subnet_is_valid(const uint8_t subnet[4]) {
    bool found_zero = false;
    bool found_one = false;
    for(size_t byte = 0; byte < 4; byte++) {
        for(uint8_t mask = 0x80; mask != 0; mask >>= 1) {
            bool bit = (subnet[byte] & mask) != 0;
            if(bit) {
                if(found_zero) return false;
                found_one = true;
            } else {
                found_zero = true;
            }
        }
    }
    return found_one;
}

static bool settings_mac_is_valid(const uint8_t mac[6]) {
    bool all_zero = true;
    bool all_ff = true;
    for(size_t i = 0; i < 6; i++) {
        all_zero &= mac[i] == 0;
        all_ff &= mac[i] == 0xff;
    }
    return !all_zero && !all_ff && !(mac[0] & 0x01);
}

static bool settings_dora_tuple_is_valid(
    const uint8_t ip[4],
    const uint8_t subnet[4],
    const uint8_t gateway[4],
    const uint8_t gateway_mac[6]) {
    return settings_ipv4_is_nonzero(ip) && settings_subnet_is_valid(subnet) &&
           settings_ipv4_is_nonzero(gateway) && settings_mac_is_valid(gateway_mac);
}

void settings_load(App* app) {
    furi_assert(app);
    furi_assert(app->storage);
    furi_assert(app->ethernet);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t u32_tmp = 0;
    bool flag = false;
    uint8_t buf6[6];
    uint8_t saved_ip[4] = {0};
    uint8_t saved_subnet[4] = {0};
    uint8_t saved_gateway[4] = {0};
    uint8_t saved_gateway_mac[6] = {0};
    bool have_ip = false;
    bool have_subnet = false;
    bool have_gateway = false;
    bool have_gateway_mac = false;
    bool requested_dora = false;

    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_PATH)) break;
        if(!flipper_format_read_header(ff, file_type, &version)) break;
        // F0.5f — accept v1 (pre-gateway-persist) and v2 to avoid
        // resetting users' persisted MAC/IP just because the schema grew.
        if(version != 1 && version != 2 && version != 3) break;
        if(furi_string_cmp_str(file_type, SETTINGS_FILETYPE) != 0) break;

        // MAC — also push to chip registers if present. enc28j60_set_mac
        // reads from instance->mac_address, so update that first.
        if(flipper_format_read_hex(ff, "mac_address", buf6, 6)) {
            memcpy(app->ethernet->mac_address, buf6, 6);
            enc28j60_set_mac(app->ethernet);
        }

        // Static IPv4 (used only when is_static_ip == true).
        if(flipper_format_read_hex(ff, "ip_address", saved_ip, 4)) {
            have_ip = true;
            memcpy(app->ethernet->ip_address, saved_ip, 4);
            memcpy(app->ip_helper, saved_ip, 4);
        }

        // is_static_ip flag.
        if(flipper_format_read_bool(ff, "is_static_ip", &flag, 1)) {
            app->is_static_ip = flag;
        }

        // F0.5f — gateway / mac_gateway / is_dora. These were missing in
        // v1 of the schema. Without them, a user with a static-IP setup
        // had `is_static_ip=true` (auto-replies on) but `is_dora=false`
        // (Ping/Ports/OS/ArpSpoof refused to start), an inconsistent
        // half-restored state.
        if(flipper_format_read_bool(ff, "is_dora", &flag, 1)) {
            requested_dora = flag;
        }
        have_gateway = flipper_format_read_hex(ff, "ip_gateway", saved_gateway, 4);
        have_gateway_mac = flipper_format_read_hex(ff, "mac_gateway", saved_gateway_mac, 6);
        if(version >= 3) {
            have_subnet = flipper_format_read_hex(ff, "subnet_mask", saved_subnet, 4);
        }

        bool valid_dora =
            requested_dora && have_ip && have_subnet && have_gateway && have_gateway_mac &&
            settings_dora_tuple_is_valid(saved_ip, saved_subnet, saved_gateway, saved_gateway_mac);
        app->is_dora = valid_dora;
        if(valid_dora) {
            memcpy(app->ethernet->subnet_mask, saved_subnet, 4);
            memcpy(app->ip_gateway, saved_gateway, 4);
            memcpy(app->mac_gateway, saved_gateway_mac, 6);
        } else {
            memset(app->ethernet->subnet_mask, 0, 4);
            memset(app->ip_gateway, 0, 4);
            memset(app->mac_gateway, 0, 6);
        }

        // scan_params block (F0.1).
        flipper_format_read_hex(ff, "scan_target_ip", app->scan_params.target_ip, 4);

        if(flipper_format_read_uint32(ff, "scan_target_port", &u32_tmp, 1)) {
            app->scan_params.target_port = (uint16_t)u32_tmp;
        }
        if(flipper_format_read_uint32(ff, "scan_range_port", &u32_tmp, 1)) {
            app->scan_params.range_port = (uint16_t)u32_tmp;
        }
        if(flipper_format_read_uint32(ff, "scan_protocols_index", &u32_tmp, 1)) {
            app->scan_params.protocols_index = (uint8_t)u32_tmp;
        }

        flipper_format_read_hex(ff, "scan_ip_ping", app->scan_params.ip_ping, 4);
        flipper_format_read_hex(ff, "scan_ip_start", app->scan_params.ip_start, 4);

        if(flipper_format_read_uint32(ff, "scan_range_ip", &u32_tmp, 1)) {
            app->scan_params.range_ip = (uint8_t)u32_tmp;
        }
    } while(false);

    furi_string_free(file_type);
    flipper_format_free(ff);
}

void settings_save(App* app) {
    furi_assert(app);
    furi_assert(app->storage);
    furi_assert(app->ethernet);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    uint32_t u32_tmp = 0;
    bool dora_valid = app->is_dora && settings_dora_tuple_is_valid(
                                          app->ethernet->ip_address,
                                          app->ethernet->subnet_mask,
                                          app->ip_gateway,
                                          app->mac_gateway);

    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;

        if(!flipper_format_write_hex(ff, "mac_address", app->ethernet->mac_address, 6)) break;
        if(!flipper_format_write_hex(ff, "ip_address", app->ethernet->ip_address, 4)) break;
        if(!flipper_format_write_bool(ff, "is_static_ip", &app->is_static_ip, 1)) break;

        // F0.5f — paired writes for the new v2 fields (see load() for the
        // motivation: was producing a half-restored static-IP state).
        if(!flipper_format_write_bool(ff, "is_dora", &dora_valid, 1)) break;
        if(!flipper_format_write_hex(ff, "ip_gateway", app->ip_gateway, 4)) break;
        if(!flipper_format_write_hex(ff, "mac_gateway", app->mac_gateway, 6)) break;
        if(!flipper_format_write_hex(ff, "subnet_mask", app->ethernet->subnet_mask, 4)) break;

        if(!flipper_format_write_hex(ff, "scan_target_ip", app->scan_params.target_ip, 4)) break;

        u32_tmp = app->scan_params.target_port;
        if(!flipper_format_write_uint32(ff, "scan_target_port", &u32_tmp, 1)) break;
        u32_tmp = app->scan_params.range_port;
        if(!flipper_format_write_uint32(ff, "scan_range_port", &u32_tmp, 1)) break;
        u32_tmp = app->scan_params.protocols_index;
        if(!flipper_format_write_uint32(ff, "scan_protocols_index", &u32_tmp, 1)) break;

        if(!flipper_format_write_hex(ff, "scan_ip_ping", app->scan_params.ip_ping, 4)) break;
        if(!flipper_format_write_hex(ff, "scan_ip_start", app->scan_params.ip_start, 4)) break;

        u32_tmp = app->scan_params.range_ip;
        if(!flipper_format_write_uint32(ff, "scan_range_ip", &u32_tmp, 1)) break;
    } while(false);

    flipper_format_free(ff);
}
