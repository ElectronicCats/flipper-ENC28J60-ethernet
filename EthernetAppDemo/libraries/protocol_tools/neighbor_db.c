#include "neighbor_db.h"

static neighbor_t* neighbors = NULL;

/*
 * API 87.1 uses an 8-byte, 8-byte-aligned heap block header. Keep one KiB
 * available after the DB allocation for the configuration widget and other
 * small scene allocations. That reserve matches 25% of the 4 KiB Passive
 * worker stack. The reserve contributes to total-free headroom, but it is not
 * part of the DB allocation and therefore need not be contiguous with it.
 */
#define NEIGHBOR_DB_HEAP_HEADER_BYTES        8U
#define NEIGHBOR_DB_HEAP_ALIGNMENT_BYTES     8U
#define NEIGHBOR_DB_POST_ALLOC_RESERVE_BYTES 1024U

static size_t neighbor_db_heap_block_size(size_t payload_size) {
    const size_t with_header = payload_size + NEIGHBOR_DB_HEAP_HEADER_BYTES;
    return (with_header + NEIGHBOR_DB_HEAP_ALIGNMENT_BYTES - 1U) &
           ~(NEIGHBOR_DB_HEAP_ALIGNMENT_BYTES - 1U);
}

static bool neighbor_db_has_allocation_headroom(void) {
    const size_t payload_size = NEIGHBOR_DB_MAX_ENTRIES * sizeof(neighbor_t);
    const size_t allocation_block = neighbor_db_heap_block_size(payload_size);
    const size_t required_total = allocation_block + NEIGHBOR_DB_POST_ALLOC_RESERVE_BYTES;
    const size_t required_max_block = allocation_block;

    return memmgr_get_free_heap() >= required_total &&
           memmgr_heap_get_max_free_block() >= required_max_block;
}

bool neighbor_db_acquire(void) {
    if(neighbors) {
        return true;
    }

    /* calloc() is fatal on OOM in the target firmware, so guard it first. */
    if(!neighbor_db_has_allocation_headroom()) {
        return false;
    }

    neighbors = calloc(NEIGHBOR_DB_MAX_ENTRIES, sizeof(neighbor_t));
    return neighbors != NULL;
}

void neighbor_db_release(void) {
    if(!neighbors) {
        return;
    }

    free(neighbors);
    neighbors = NULL;
}

void neighbor_db_clear(void) {
    if(!neighbors) {
        return;
    }

    memset(neighbors, 0, NEIGHBOR_DB_MAX_ENTRIES * sizeof(neighbor_t));
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
    if(!neighbors) {
        return 0;
    }

    size_t count = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) continue;

        if(neighbors[i].discovery_sources & source) count++;
    }

    return count;
}

neighbor_t* neighbor_db_get_by_source(uint8_t source, size_t position) {
    if(!neighbors) {
        return NULL;
    }

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
    if(!neighbors) {
        return;
    }

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) continue;

        neighbors[i].discovery_sources &= ~source;

        if(neighbors[i].discovery_sources == 0) {
            memset(&neighbors[i], 0, sizeof(neighbor_t));
        }
    }
}

size_t neighbor_db_count(void) {
    if(!neighbors) {
        return 0;
    }

    size_t count = 0;

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(neighbors[i].occupied) {
            count++;
        }
    }

    return count;
}

neighbor_t* neighbor_db_get_by_position(size_t position) {
    if(!neighbors) {
        return NULL;
    }

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
    if(!neighbors || index >= NEIGHBOR_DB_MAX_ENTRIES) {
        return NULL;
    }

    if(!neighbors[index].occupied) {
        return NULL;
    }

    return &neighbors[index];
}

neighbor_t* neighbor_db_find(const uint8_t mac[6]) {
    if(!neighbors || !mac) {
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

neighbor_t* neighbor_db_find_by_source(const uint8_t mac[6], uint8_t source) {
    if(!neighbors || !mac) {
        return NULL;
    }

    for(size_t i = 0; i < NEIGHBOR_DB_MAX_ENTRIES; i++) {
        if(!neighbors[i].occupied) {
            continue;
        }

        if(memcmp(neighbors[i].mac, mac, 6) != 0) {
            continue;
        }

        if(!(neighbors[i].discovery_sources & source)) {
            continue;
        }

        return &neighbors[i];
    }

    return NULL;
}

bool neighbor_db_add(const neighbor_t* neighbor) {
    if(!neighbors || !neighbor) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find_by_source(neighbor->mac, neighbor->discovery_sources);

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
    if(!neighbors || !neighbor) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find_by_source(neighbor->mac, neighbor->discovery_sources);

    if(!existing) {
        return false;
    }

    *existing = *neighbor;
    existing->occupied = true;

    return true;
}
