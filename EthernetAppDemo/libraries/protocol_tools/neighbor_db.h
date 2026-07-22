#ifndef NEIGHBOR_DB_H_
#define NEIGHBOR_DB_H_

#include <furi.h>
#include <furi_hal.h>

#define NEIGHBOR_DB_MAX_ENTRIES 32

/**
 * @brief Discovery protocols that identified a neighbor.
 *
 * Multiple values may be combined as a bitmask when a device
 * is discovered through more than one protocol.
 */

typedef enum {
    NEIGHBOR_SOURCE_LLDP = (1 << 0),
    NEIGHBOR_SOURCE_CDP = (1 << 1),
    NEIGHBOR_SOURCE_EAPOL = (1 << 2),
} neighbor_source_t;

/**
 * @brief Represents a discovered network neighbor.
 *
 * Stores the information collected from one or more passive
 * discovery protocols (LLDP, CDP or EAPOL).
 */

typedef struct {
    uint8_t mac[6];

    char chassis_id[64];
    uint8_t chassis_subtype;
    uint8_t port_subtype;
    char name[64];
    char description[128];
    char port[64];
    char port_description[64];
    char management_address[48];

    uint16_t ttl;
    uint16_t capabilities;
    neighbor_source_t last_seen_source;

    uint16_t discovery_sources;

    uint8_t eapol_version;
    uint8_t eapol_packet_type;

    uint8_t eap_code;
    uint8_t eap_type;

    bool occupied;

    char eap_identity[64];

    uint16_t vlan_id;
    bool has_vlan;

    uint16_t poe_power_mw;

    uint8_t poe_type;
    uint8_t poe_source;
    uint8_t poe_priority;

    bool has_poe;
} neighbor_t;

void neighbor_db_init(void);

void neighbor_db_clear(void);

/**
 * @brief Searches a neighbor by MAC address.
 *
 * @param mac Neighbor MAC address.
 *
 * @return Pointer to the neighbor if found.
 * @return NULL otherwise.
 */
neighbor_t* neighbor_db_find(const uint8_t mac[6]);

bool neighbor_db_add(const neighbor_t* neighbor);

bool neighbor_db_update(const neighbor_t* neighbor);

/**
 * @brief Saves the current neighbor database to persistent storage.
 */
void neighbor_db_save(void);
void neighbor_db_clear_by_source(neighbor_source_t source);
void lldp_format_capabilities(uint16_t capabilities, char* output, size_t size);

/**
 * @brief Loads the last neighbor database from persistent storage.
 *
 * @return true if a database was restored.
 * @return false if no valid database exists.
 */
bool neighbor_db_load(void);

neighbor_t* neighbor_db_get(size_t index);

size_t neighbor_db_count(void);

size_t neighbor_db_count_by_source(neighbor_source_t source);

neighbor_t* neighbor_db_get_by_source(neighbor_source_t source, size_t index);

#endif
