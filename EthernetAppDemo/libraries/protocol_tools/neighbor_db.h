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
    /* Common neighbor identity */
    uint8_t mac[6];

    char chassis_id[64];
    char name[64];
    char description[128];
    char port[64];
    char management_address[48];

    /* Standard LLDP information */
    uint16_t ttl;

    uint16_t capabilities;
    uint16_t enabled_capabilities;

    /* Discovery source bitmask */
    uint8_t discovery_sources;

    /* EAPOL observation metadata */
    uint8_t eapol_version;
    uint8_t eapol_packet_type;
    uint8_t eap_code;
    uint8_t eap_type;

    /* LLDP VLAN information */
    uint16_t vlan_id;
    uint16_t pvid;
    char vlan_name[64];

    uint16_t network_policy_vlan;

    bool has_pvid;
    bool has_vlan_name;
    bool has_network_policy;

    /* LLDP PoE information */
    bool poe_supported;

    uint8_t poe_power_pair;
    uint8_t poe_power_class;

    uint8_t poe_type_source_priority;

    uint16_t poe_requested_power;
    uint16_t poe_allocated_power;

    uint16_t poe_power_watts;
    uint16_t poe_requested_power_watts;
    uint16_t poe_allocated_power_watts;

    uint8_t poe_power_type;
    uint8_t poe_power_source;
    uint8_t poe_power_priority;

    bool has_poe;
    bool has_poe_power_values;

    /* Database state */
    bool occupied;

} neighbor_t;

bool neighbor_db_acquire(void);

void neighbor_db_release(void);

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

neighbor_t* neighbor_db_get(size_t index);

neighbor_t* neighbor_db_get_by_position(size_t position);

size_t neighbor_db_count(void);

void neighbor_db_load(void);

void neighbor_db_save(void);

size_t neighbor_db_count_by_source(uint8_t source);

neighbor_t* neighbor_db_get_by_source(uint8_t source, size_t position);

void neighbor_db_clear_by_source(uint8_t source);

neighbor_t* neighbor_db_find_by_source(const uint8_t mac[6], uint8_t source);

#endif
