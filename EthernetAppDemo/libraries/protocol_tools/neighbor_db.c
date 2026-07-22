#include "neighbor_db.h"
#include <storage/storage.h>

#define NEIGHBOR_DB_FILE EXT_PATH("apps_data/ethernet/neighbors.db")

static neighbor_t neighbors[NEIGHBOR_DB_MAX_ENTRIES];

void neighbor_db_clear(void) {
    memset(neighbors, 0, sizeof(neighbors));
}

void neighbor_db_init(void) {
    neighbor_db_clear();
}

void neighbor_db_clear_by_source(neighbor_source_t source) {
    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            continue;
        }

        if(neighbors[i].discovery_sources & source) {
            memset(&neighbors[i], 0, sizeof(neighbor_t));
        }
    }

    neighbor_db_save();
}

void neighbor_db_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, NEIGHBOR_DB_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, neighbors, sizeof(neighbors));
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

bool neighbor_db_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);

    bool ok = false;

    if(storage_file_open(file, NEIGHBOR_DB_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, neighbors, sizeof(neighbors)) == sizeof(neighbors)) {
            ok = true;
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return ok;
}

size_t neighbor_db_count(void) {
    size_t count = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(neighbors[i].occupied) {
            count++;
        }
    }

    return count;
}

size_t neighbor_db_count_by_source(neighbor_source_t source) {
    size_t count = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            continue;
        }

        if(neighbors[i].discovery_sources & source) {
            count++;
        }
    }

    return count;
}

neighbor_t* neighbor_db_get_by_source(neighbor_source_t source, size_t index) {
    size_t current = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            continue;
        }

        if(!(neighbors[i].discovery_sources & source)) {
            continue;
        }

        if(current == index) {
            return &neighbors[i];
        }

        current++;
    }

    return NULL;
}

neighbor_t* neighbor_db_get(size_t index) {
    if(index >= NEIGHBOR_DB_MAX_ENTRIES) {
        return NULL;
    }

    if(!neighbors[index].occupied) {
        return NULL;
    }

    return &neighbors[index];
}

neighbor_t* neighbor_db_find(const uint8_t mac[6]) {
    if(!mac) {
        return NULL;
    }

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            continue;
        }

        if(memcmp(neighbors[i].mac, mac, 6) == 0) {
            return &neighbors[i];
        }
    }

    return NULL;
}

bool neighbor_db_add(const neighbor_t* neighbor) {
    if(!neighbor) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find(neighbor->mac);

    if(existing) {
        *existing = *neighbor;
        existing->occupied = true;

        neighbor_db_save();

        return true;
    }

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            neighbors[i] = *neighbor;
            neighbors[i].occupied = true;

            neighbor_db_save();

            return true;
        }
    }

    return false;
}

bool neighbor_db_update(const neighbor_t* neighbor) {
    if(!neighbor) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find(neighbor->mac);

    if(!existing) {
        return false;
    }

    /* Keep identity */
    memcpy(existing->mac, neighbor->mac, sizeof(existing->mac));

    /* Merge discovery sources */
    existing->discovery_sources |= neighbor->discovery_sources;

    /* Record which protocol updated the entry most recently */
    if(neighbor->last_seen_source != 0) {
        existing->last_seen_source = neighbor->last_seen_source;
    }

/* Merge strings only when the new value is not empty */
#define MERGE_STRING(field)                                                         \
    do {                                                                            \
        if(neighbor->field[0] != '\0') {                                            \
            strncpy(existing->field, neighbor->field, sizeof(existing->field) - 1); \
            existing->field[sizeof(existing->field) - 1] = '\0';                    \
        }                                                                           \
    } while(0)

    MERGE_STRING(chassis_id);
    MERGE_STRING(name);
    MERGE_STRING(description);
    MERGE_STRING(port);
    MERGE_STRING(port_description);
    MERGE_STRING(management_address);
    MERGE_STRING(eap_identity);

#undef MERGE_STRING

    /* Merge numeric fields only when the new value is meaningful */
    if(neighbor->chassis_subtype != 0) existing->chassis_subtype = neighbor->chassis_subtype;
    if(neighbor->port_subtype != 0) existing->port_subtype = neighbor->port_subtype;
    if(neighbor->ttl != 0) existing->ttl = neighbor->ttl;
    if(neighbor->capabilities != 0) existing->capabilities = neighbor->capabilities;

    if(neighbor->eapol_version != 0) existing->eapol_version = neighbor->eapol_version;
    if(neighbor->eapol_packet_type != 0) existing->eapol_packet_type = neighbor->eapol_packet_type;
    if(neighbor->eap_code != 0) existing->eap_code = neighbor->eap_code;
    if(neighbor->eap_type != 0) existing->eap_type = neighbor->eap_type;

    /* VLAN */
    if(neighbor->has_vlan) {
        existing->vlan_id = neighbor->vlan_id;
        existing->has_vlan = true;
    }

    /* PoE */
    if(neighbor->has_poe) {
        existing->poe_power_mw = neighbor->poe_power_mw;
        existing->poe_type = neighbor->poe_type;
        existing->poe_source = neighbor->poe_source;
        existing->poe_priority = neighbor->poe_priority;
        existing->has_poe = true;
    }

    existing->occupied = true;

    return true;

    *existing = *neighbor;
    existing->occupied = true;

    neighbor_db_save();

    return true;
}
