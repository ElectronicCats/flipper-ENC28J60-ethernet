#include "neighbor_db.h"

static neighbor_t neighbors[NEIGHBOR_DB_MAX_ENTRIES];

void neighbor_db_clear(void) {
    memset(neighbors, 0, sizeof(neighbors));
}

void neighbor_db_init(void) {
    neighbor_db_clear();
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

        return true;
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

    *existing = *neighbor;
    existing->occupied = true;

    return true;
}
