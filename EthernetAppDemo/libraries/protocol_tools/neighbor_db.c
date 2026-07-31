#include "neighbor_db.h"

static neighbor_t neighbors[NEIGHBOR_DB_MAX_ENTRIES];

void neighbor_db_clear(void) {
    memset(neighbors, 0, sizeof(neighbors));
}

void neighbor_db_init(void) {
    neighbor_db_clear();
}

void neighbor_db_load(void) {
    // TODO: Storage persistence
}

void neighbor_db_save(void) {
    // TODO: Storage persistence
}

size_t neighbor_db_count_by_source(uint8_t source) {
    size_t count = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) continue;

        if(neighbors[i].discovery_sources & source) count++;
    }

    return count;
}

neighbor_t* neighbor_db_get_by_source(uint8_t source, size_t position) {
    size_t current = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) continue;

        if(!(neighbors[i].discovery_sources & source)) continue;

        if(current == position) return &neighbors[i];

        current++;
    }

    return NULL;
}

void neighbor_db_clear_by_source(uint8_t source) {
    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) continue;

        neighbors[i].discovery_sources &= ~source;

        if(neighbors[i].discovery_sources == 0) {
            memset(&neighbors[i], 0, sizeof(neighbor_t));
        }
    }
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

neighbor_t* neighbor_db_get_by_position(size_t position) {
    size_t current = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            continue;
        }

        if(current == position) {
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
        return neighbor_db_update(neighbor);
    }

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            neighbors[i] = *neighbor;
            neighbors[i].occupied = true;

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

    memcpy(existing->mac, neighbor->mac, sizeof(existing->mac));

    existing->last_seen_source = neighbor->last_seen_source;

    existing->discovery_sources |= neighbor->discovery_sources;

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

#undef MERGE_STRING

    if(neighbor->chassis_subtype) existing->chassis_subtype = neighbor->chassis_subtype;

    if(neighbor->port_subtype) existing->port_subtype = neighbor->port_subtype;

    if(neighbor->ttl) existing->ttl = neighbor->ttl;

    if(neighbor->capabilities) existing->capabilities = neighbor->capabilities;

    if(neighbor->has_vlan) {
        existing->has_vlan = true;
        existing->vlan_id = neighbor->vlan_id;
    }

    if(neighbor->has_poe) {
        existing->has_poe = true;
        existing->poe_power_mw = neighbor->poe_power_mw;
        existing->poe_type = neighbor->poe_type;
        existing->poe_source = neighbor->poe_source;
        existing->poe_priority = neighbor->poe_priority;
    }

    existing->occupied = true;

    return true;
}
