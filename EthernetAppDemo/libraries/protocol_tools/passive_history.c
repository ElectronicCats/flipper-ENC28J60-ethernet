#include "passive_history.h"

#include <string.h>

#define PASSIVE_HISTORY_TEMP_PATH EXT_PATH("apps_data/ethernet/passive_history.tmp")

#define HISTORY_HEADER_SIZE        20U
#define HISTORY_RECORD_HEADER_SIZE 16U
#define HISTORY_FORMAT_VERSION     1U
#define HISTORY_RECORD_VERSION     1U

#define HISTORY_LLDP_PAYLOAD_MAX  463U
#define HISTORY_CDP_PAYLOAD_MAX   308U
#define HISTORY_EAPOL_PAYLOAD_MAX 68U
#define HISTORY_PAYLOAD_MAX       HISTORY_LLDP_PAYLOAD_MAX
#define HISTORY_BODY_MAX \
    (PASSIVE_HISTORY_MAX_RECORDS * (HISTORY_RECORD_HEADER_SIZE + HISTORY_PAYLOAD_MAX))
#define HISTORY_FILE_MAX (HISTORY_HEADER_SIZE + HISTORY_BODY_MAX)

#define HISTORY_FLAG_NAME             (1U << 0)
#define HISTORY_FLAG_PORT             (1U << 1)
#define HISTORY_FLAG_MANAGEMENT       (1U << 2)
#define HISTORY_FLAG_CHASSIS          (1U << 3)
#define HISTORY_FLAG_DESCRIPTION      (1U << 4)
#define HISTORY_FLAG_VLAN_NAME_STRING (1U << 5)
#define HISTORY_FLAG_HAS_PVID         (1U << 6)
#define HISTORY_FLAG_HAS_VLAN_NAME    (1U << 7)
#define HISTORY_FLAG_HAS_POLICY       (1U << 8)
#define HISTORY_FLAG_HAS_POE_MDI      (1U << 9)
#define HISTORY_FLAG_HAS_POE          (1U << 10)
#define HISTORY_FLAG_HAS_POE_VALUES   (1U << 11)
#define HISTORY_FLAG_POE_SUPPORTED    (1U << 12)

#define HISTORY_LLDP_FLAGS_MASK 0x1FFFU
#define HISTORY_CDP_FLAGS_MASK \
    (HISTORY_FLAG_NAME | HISTORY_FLAG_PORT | HISTORY_FLAG_MANAGEMENT | HISTORY_FLAG_DESCRIPTION)
#define HISTORY_EAPOL_FLAGS_MASK HISTORY_FLAG_NAME

typedef struct {
    uint32_t offset;
    uint16_t payload_length;
    uint16_t flags;
    uint8_t mac[6];
    uint8_t protocol;
    uint8_t reserved;
} PassiveHistoryIndexEntry;

struct PassiveHistory {
    PassiveHistoryIndexEntry entries[PASSIVE_HISTORY_MAX_RECORDS];
    neighbor_t scratch;
    uint8_t count;
    uint8_t status;
};

_Static_assert(sizeof(PassiveHistoryIndexEntry) == 16U, "Passive History index size changed");

typedef struct {
    uint8_t* data;
    size_t capacity;
    size_t position;
} PayloadWriter;

typedef struct {
    const uint8_t* data;
    size_t length;
    size_t position;
} PayloadReader;

typedef struct {
    File* file;
    uint32_t body_crc;
    uint32_t body_size;
    uint8_t count;
    bool ok;
} HistoryWriter;

static const uint8_t history_magic[4] = {'P', 'S', 'H', '1'};

static uint16_t history_read_u16(const uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t history_read_u32(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void history_write_u16(uint8_t* data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void history_write_u32(uint8_t* data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static uint32_t history_crc32_update(uint32_t crc, const uint8_t* data, size_t length) {
    for(size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t) - (int32_t)(crc & 1U));
        }
    }
    return crc;
}

static uint32_t history_crc32(const uint8_t* data, size_t length) {
    return history_crc32_update(0xFFFFFFFFUL, data, length) ^ 0xFFFFFFFFUL;
}

static size_t history_string_length(const char* string, size_t capacity) {
    size_t length = 0;
    while(length < capacity && string[length])
        length++;
    return length;
}

static bool payload_put_u8(PayloadWriter* writer, uint8_t value) {
    if(writer->position >= writer->capacity) return false;
    writer->data[writer->position++] = value;
    return true;
}

static bool payload_put_u16(PayloadWriter* writer, uint16_t value) {
    if(writer->capacity - writer->position < 2U) return false;
    history_write_u16(writer->data + writer->position, value);
    writer->position += 2U;
    return true;
}

static bool payload_put_string(PayloadWriter* writer, const char* string, size_t capacity) {
    size_t length = history_string_length(string, capacity);
    if(length >= capacity || length > UINT8_MAX ||
       writer->capacity - writer->position < length + 1U) {
        return false;
    }
    writer->data[writer->position++] = (uint8_t)length;
    memcpy(writer->data + writer->position, string, length);
    writer->position += length;
    return true;
}

static bool payload_get_u8(PayloadReader* reader, uint8_t* value) {
    if(reader->position >= reader->length) return false;
    *value = reader->data[reader->position++];
    return true;
}

static bool payload_get_u16(PayloadReader* reader, uint16_t* value) {
    if(reader->length - reader->position < 2U) return false;
    *value = history_read_u16(reader->data + reader->position);
    reader->position += 2U;
    return true;
}

static bool
    payload_get_string(PayloadReader* reader, char* string, size_t capacity, bool marked_present) {
    uint8_t length;
    if(!payload_get_u8(reader, &length) || length >= capacity ||
       reader->length - reader->position < length || marked_present != (length != 0U)) {
        return false;
    }
    memcpy(string, reader->data + reader->position, length);
    string[length] = '\0';
    reader->position += length;
    return true;
}

static uint16_t
    history_presence_flags(const neighbor_t* neighbor, PassiveHistoryProtocol protocol) {
    uint16_t flags = 0;
    if(neighbor->name[0]) flags |= HISTORY_FLAG_NAME;
    if(neighbor->port[0]) flags |= HISTORY_FLAG_PORT;
    if(neighbor->management_address[0]) flags |= HISTORY_FLAG_MANAGEMENT;
    if(neighbor->description[0]) flags |= HISTORY_FLAG_DESCRIPTION;

    if(protocol == PassiveHistoryProtocolLldp) {
        if(neighbor->chassis_id[0]) flags |= HISTORY_FLAG_CHASSIS;
        if(neighbor->vlan_name[0]) flags |= HISTORY_FLAG_VLAN_NAME_STRING;
        if(neighbor->has_pvid) flags |= HISTORY_FLAG_HAS_PVID;
        if(neighbor->has_vlan_name) flags |= HISTORY_FLAG_HAS_VLAN_NAME;
        if(neighbor->has_network_policy) flags |= HISTORY_FLAG_HAS_POLICY;
        if(neighbor->has_poe_mdi) flags |= HISTORY_FLAG_HAS_POE_MDI;
        if(neighbor->has_poe) flags |= HISTORY_FLAG_HAS_POE;
        if(neighbor->has_poe_power_values) flags |= HISTORY_FLAG_HAS_POE_VALUES;
        if(neighbor->poe_supported) flags |= HISTORY_FLAG_POE_SUPPORTED;
    }
    return flags;
}

static bool history_protocol_valid(PassiveHistoryProtocol protocol) {
    return protocol >= PassiveHistoryProtocolLldp && protocol <= PassiveHistoryProtocolEapol;
}

static uint8_t history_source_for_protocol(PassiveHistoryProtocol protocol) {
    switch(protocol) {
    case PassiveHistoryProtocolLldp:
        return NEIGHBOR_SOURCE_LLDP;
    case PassiveHistoryProtocolCdp:
        return NEIGHBOR_SOURCE_CDP;
    case PassiveHistoryProtocolEapol:
        return NEIGHBOR_SOURCE_EAPOL;
    default:
        return 0;
    }
}

static bool
    history_filter_matches(PassiveHistoryProtocol filter, PassiveHistoryProtocol protocol) {
    return filter == PassiveHistoryProtocolAll || filter == protocol;
}

static bool history_flags_valid(PassiveHistoryProtocol protocol, uint16_t flags) {
    uint16_t mask = 0;
    if(protocol == PassiveHistoryProtocolLldp) {
        mask = HISTORY_LLDP_FLAGS_MASK;
    } else if(protocol == PassiveHistoryProtocolCdp) {
        mask = HISTORY_CDP_FLAGS_MASK;
    } else if(protocol == PassiveHistoryProtocolEapol) {
        mask = HISTORY_EAPOL_FLAGS_MASK;
    }
    return mask && !(flags & ~mask);
}

static bool history_encode_payload(
    PassiveHistoryProtocol protocol,
    const neighbor_t* neighbor,
    uint8_t payload[HISTORY_PAYLOAD_MAX],
    uint16_t* payload_length,
    uint16_t* flags) {
    PayloadWriter writer = {.data = payload, .capacity = HISTORY_PAYLOAD_MAX, .position = 0};
    *flags = history_presence_flags(neighbor, protocol);

    if(protocol == PassiveHistoryProtocolLldp) {
        if(!payload_put_u8(&writer, neighbor->lldp_chassis_subtype) ||
           !payload_put_u8(&writer, neighbor->lldp_port_subtype) ||
           !payload_put_u16(&writer, neighbor->ttl) ||
           !payload_put_u16(&writer, neighbor->capabilities) ||
           !payload_put_u16(&writer, neighbor->enabled_capabilities) ||
           !payload_put_u16(&writer, neighbor->vlan_id) ||
           !payload_put_u16(&writer, neighbor->pvid) ||
           !payload_put_u16(&writer, neighbor->network_policy_vlan) ||
           !payload_put_u8(&writer, neighbor->poe_power_pair) ||
           !payload_put_u8(&writer, neighbor->poe_power_class) ||
           !payload_put_u8(&writer, neighbor->poe_type_source_priority) ||
           !payload_put_u16(&writer, neighbor->poe_requested_power) ||
           !payload_put_u16(&writer, neighbor->poe_allocated_power) ||
           !payload_put_u16(&writer, neighbor->poe_power_watts) ||
           !payload_put_u16(&writer, neighbor->poe_requested_power_watts) ||
           !payload_put_u16(&writer, neighbor->poe_allocated_power_watts) ||
           !payload_put_u8(&writer, neighbor->poe_power_type) ||
           !payload_put_u8(&writer, neighbor->poe_power_source) ||
           !payload_put_u8(&writer, neighbor->poe_power_priority) ||
           !payload_put_u8(&writer, neighbor->poe_supported ? 1U : 0U) ||
           !payload_put_string(&writer, neighbor->chassis_id, sizeof(neighbor->chassis_id)) ||
           !payload_put_string(&writer, neighbor->name, sizeof(neighbor->name)) ||
           !payload_put_string(&writer, neighbor->description, sizeof(neighbor->description)) ||
           !payload_put_string(&writer, neighbor->port, sizeof(neighbor->port)) ||
           !payload_put_string(
               &writer, neighbor->management_address, sizeof(neighbor->management_address)) ||
           !payload_put_string(&writer, neighbor->vlan_name, sizeof(neighbor->vlan_name))) {
            return false;
        }
    } else if(protocol == PassiveHistoryProtocolCdp) {
        if(!payload_put_u16(&writer, neighbor->ttl) ||
           !payload_put_u16(&writer, neighbor->capabilities) ||
           !payload_put_string(&writer, neighbor->name, sizeof(neighbor->name)) ||
           !payload_put_string(&writer, neighbor->port, sizeof(neighbor->port)) ||
           !payload_put_string(
               &writer, neighbor->management_address, sizeof(neighbor->management_address)) ||
           !payload_put_string(&writer, neighbor->description, sizeof(neighbor->description))) {
            return false;
        }
    } else if(protocol == PassiveHistoryProtocolEapol) {
        if(!payload_put_u8(&writer, neighbor->eapol_version) ||
           !payload_put_u8(&writer, neighbor->eapol_packet_type) ||
           !payload_put_u8(&writer, neighbor->eap_code) ||
           !payload_put_u8(&writer, neighbor->eap_type) ||
           !payload_put_string(&writer, neighbor->name, sizeof(neighbor->name))) {
            return false;
        }
    } else {
        return false;
    }

    *payload_length = (uint16_t)writer.position;
    return true;
}

static bool history_decode_payload(
    PassiveHistoryProtocol protocol,
    uint16_t flags,
    const uint8_t* payload,
    uint16_t payload_length,
    const uint8_t mac[6],
    neighbor_t* neighbor) {
    PayloadReader reader = {.data = payload, .length = payload_length, .position = 0};
    memset(neighbor, 0, sizeof(*neighbor));
    memcpy(neighbor->mac, mac, sizeof(neighbor->mac));
    neighbor->discovery_sources = history_source_for_protocol(protocol);
    neighbor->occupied = true;

    if(protocol == PassiveHistoryProtocolLldp) {
        uint8_t poe_supported;
        if(!payload_get_u8(&reader, &neighbor->lldp_chassis_subtype) ||
           !payload_get_u8(&reader, &neighbor->lldp_port_subtype) ||
           !payload_get_u16(&reader, &neighbor->ttl) ||
           !payload_get_u16(&reader, &neighbor->capabilities) ||
           !payload_get_u16(&reader, &neighbor->enabled_capabilities) ||
           !payload_get_u16(&reader, &neighbor->vlan_id) ||
           !payload_get_u16(&reader, &neighbor->pvid) ||
           !payload_get_u16(&reader, &neighbor->network_policy_vlan) ||
           !payload_get_u8(&reader, &neighbor->poe_power_pair) ||
           !payload_get_u8(&reader, &neighbor->poe_power_class) ||
           !payload_get_u8(&reader, &neighbor->poe_type_source_priority) ||
           !payload_get_u16(&reader, &neighbor->poe_requested_power) ||
           !payload_get_u16(&reader, &neighbor->poe_allocated_power) ||
           !payload_get_u16(&reader, &neighbor->poe_power_watts) ||
           !payload_get_u16(&reader, &neighbor->poe_requested_power_watts) ||
           !payload_get_u16(&reader, &neighbor->poe_allocated_power_watts) ||
           !payload_get_u8(&reader, &neighbor->poe_power_type) ||
           !payload_get_u8(&reader, &neighbor->poe_power_source) ||
           !payload_get_u8(&reader, &neighbor->poe_power_priority) ||
           !payload_get_u8(&reader, &poe_supported) || poe_supported > 1U ||
           !payload_get_string(
               &reader,
               neighbor->chassis_id,
               sizeof(neighbor->chassis_id),
               flags & HISTORY_FLAG_CHASSIS) ||
           !payload_get_string(
               &reader, neighbor->name, sizeof(neighbor->name), flags & HISTORY_FLAG_NAME) ||
           !payload_get_string(
               &reader,
               neighbor->description,
               sizeof(neighbor->description),
               flags & HISTORY_FLAG_DESCRIPTION) ||
           !payload_get_string(
               &reader, neighbor->port, sizeof(neighbor->port), flags & HISTORY_FLAG_PORT) ||
           !payload_get_string(
               &reader,
               neighbor->management_address,
               sizeof(neighbor->management_address),
               flags & HISTORY_FLAG_MANAGEMENT) ||
           !payload_get_string(
               &reader,
               neighbor->vlan_name,
               sizeof(neighbor->vlan_name),
               flags & HISTORY_FLAG_VLAN_NAME_STRING)) {
            return false;
        }
        neighbor->has_pvid = flags & HISTORY_FLAG_HAS_PVID;
        neighbor->has_vlan_name = flags & HISTORY_FLAG_HAS_VLAN_NAME;
        neighbor->has_network_policy = flags & HISTORY_FLAG_HAS_POLICY;
        neighbor->has_poe_mdi = flags & HISTORY_FLAG_HAS_POE_MDI;
        neighbor->has_poe = flags & HISTORY_FLAG_HAS_POE;
        neighbor->has_poe_power_values = flags & HISTORY_FLAG_HAS_POE_VALUES;
        neighbor->poe_supported = poe_supported != 0U;
        if(neighbor->poe_supported != ((flags & HISTORY_FLAG_POE_SUPPORTED) != 0U)) return false;
    } else if(protocol == PassiveHistoryProtocolCdp) {
        if(!payload_get_u16(&reader, &neighbor->ttl) ||
           !payload_get_u16(&reader, &neighbor->capabilities) ||
           !payload_get_string(
               &reader, neighbor->name, sizeof(neighbor->name), flags & HISTORY_FLAG_NAME) ||
           !payload_get_string(
               &reader, neighbor->port, sizeof(neighbor->port), flags & HISTORY_FLAG_PORT) ||
           !payload_get_string(
               &reader,
               neighbor->management_address,
               sizeof(neighbor->management_address),
               flags & HISTORY_FLAG_MANAGEMENT) ||
           !payload_get_string(
               &reader,
               neighbor->description,
               sizeof(neighbor->description),
               flags & HISTORY_FLAG_DESCRIPTION)) {
            return false;
        }
    } else if(protocol == PassiveHistoryProtocolEapol) {
        if(!payload_get_u8(&reader, &neighbor->eapol_version) ||
           !payload_get_u8(&reader, &neighbor->eapol_packet_type) ||
           !payload_get_u8(&reader, &neighbor->eap_code) ||
           !payload_get_u8(&reader, &neighbor->eap_type) ||
           !payload_get_string(
               &reader, neighbor->name, sizeof(neighbor->name), flags & HISTORY_FLAG_NAME)) {
            return false;
        }
    } else {
        return false;
    }

    return reader.position == reader.length;
}

static bool
    history_record_read(File* file, const PassiveHistoryIndexEntry* entry, neighbor_t* neighbor) {
    uint8_t header[HISTORY_RECORD_HEADER_SIZE];
    uint8_t payload[HISTORY_PAYLOAD_MAX];
    if(!storage_file_seek(file, entry->offset, true) ||
       storage_file_read(file, header, sizeof(header)) != sizeof(header)) {
        return false;
    }
    uint16_t payload_length = history_read_u16(header + 4);
    if(header[0] != HISTORY_RECORD_VERSION || header[1] != entry->protocol ||
       history_read_u16(header + 2) != entry->flags || payload_length != entry->payload_length ||
       memcmp(header + 6, entry->mac, 6) != 0 || payload_length > sizeof(payload) ||
       storage_file_read(file, payload, payload_length) != payload_length ||
       history_crc32(payload, payload_length) != history_read_u32(header + 12)) {
        return false;
    }
    return history_decode_payload(
        (PassiveHistoryProtocol)entry->protocol,
        entry->flags,
        payload,
        payload_length,
        entry->mac,
        neighbor);
}

static bool history_key_exists(
    const PassiveHistory* history,
    const uint8_t mac[6],
    PassiveHistoryProtocol protocol) {
    for(size_t i = 0; i < history->count; i++) {
        if(history->entries[i].protocol == protocol &&
           memcmp(history->entries[i].mac, mac, 6) == 0) {
            return true;
        }
    }
    return false;
}

static void history_load(PassiveHistory* history, Storage* storage) {
    history->count = 0;
    history->status = PassiveHistoryStatusInvalid;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, PASSIVE_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        storage_file_free(file);
        history->status = PassiveHistoryStatusMissing;
        return;
    }

    uint8_t header[HISTORY_HEADER_SIZE];
    uint64_t file_size = storage_file_size(file);
    bool valid = file_size >= HISTORY_HEADER_SIZE && file_size <= HISTORY_FILE_MAX &&
                 storage_file_read(file, header, sizeof(header)) == sizeof(header);
    uint8_t record_count = valid ? header[6] : 0;
    uint32_t body_size = valid ? history_read_u32(header + 8) : 0;
    if(!valid || memcmp(header, history_magic, sizeof(history_magic)) != 0 ||
       header[4] != HISTORY_FORMAT_VERSION || header[5] != HISTORY_HEADER_SIZE || header[7] != 0 ||
       record_count > PASSIVE_HISTORY_MAX_RECORDS || body_size > HISTORY_BODY_MAX ||
       file_size != HISTORY_HEADER_SIZE + body_size ||
       history_crc32(header, 16) != history_read_u32(header + 16)) {
        valid = false;
    }

    uint8_t chunk[128];
    uint32_t body_crc = 0xFFFFFFFFUL;
    uint32_t remaining = body_size;
    while(valid && remaining) {
        size_t amount = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        if(storage_file_read(file, chunk, amount) != amount) {
            valid = false;
            break;
        }
        body_crc = history_crc32_update(body_crc, chunk, amount);
        remaining -= amount;
    }
    if(valid && (body_crc ^ 0xFFFFFFFFUL) != history_read_u32(header + 12)) valid = false;

    uint32_t offset = HISTORY_HEADER_SIZE;
    for(uint8_t i = 0; valid && i < record_count; i++) {
        uint8_t record_header[HISTORY_RECORD_HEADER_SIZE];
        if(body_size - (offset - HISTORY_HEADER_SIZE) < HISTORY_RECORD_HEADER_SIZE ||
           !storage_file_seek(file, offset, true) ||
           storage_file_read(file, record_header, sizeof(record_header)) !=
               sizeof(record_header)) {
            valid = false;
            break;
        }

        PassiveHistoryProtocol protocol = (PassiveHistoryProtocol)record_header[1];
        uint16_t flags = history_read_u16(record_header + 2);
        uint16_t payload_length = history_read_u16(record_header + 4);
        if(record_header[0] != HISTORY_RECORD_VERSION || !history_protocol_valid(protocol) ||
           !history_flags_valid(protocol, flags) || payload_length > HISTORY_PAYLOAD_MAX ||
           payload_length >
               body_size - (offset - HISTORY_HEADER_SIZE) - HISTORY_RECORD_HEADER_SIZE) {
            valid = false;
            break;
        }

        PassiveHistoryIndexEntry entry = {
            .offset = offset,
            .payload_length = payload_length,
            .flags = flags,
            .protocol = (uint8_t)protocol,
        };
        memcpy(entry.mac, record_header + 6, sizeof(entry.mac));
        if(history_key_exists(history, entry.mac, protocol) ||
           !history_record_read(file, &entry, &history->scratch)) {
            valid = false;
            break;
        }
        history->entries[history->count++] = entry;
        offset += HISTORY_RECORD_HEADER_SIZE + payload_length;
    }

    if(valid && (history->count != record_count || offset != HISTORY_HEADER_SIZE + body_size)) {
        valid = false;
    }
    storage_file_close(file);
    storage_file_free(file);

    if(valid) {
        history->status = PassiveHistoryStatusReady;
    } else {
        history->count = 0;
        memset(history->entries, 0, sizeof(history->entries));
        memset(&history->scratch, 0, sizeof(history->scratch));
    }
}

PassiveHistory* passive_history_alloc(Storage* storage) {
    if(!storage) return NULL;
    if(memmgr_get_free_heap() < sizeof(PassiveHistory) + 512U ||
       memmgr_heap_get_max_free_block() < sizeof(PassiveHistory)) {
        return NULL;
    }
    PassiveHistory* history = malloc(sizeof(PassiveHistory));
    if(!history) return NULL;
    memset(history, 0, sizeof(*history));
    history_load(history, storage);
    return history;
}

void passive_history_free(PassiveHistory* history) {
    free(history);
}

PassiveHistoryStatus passive_history_get_status(const PassiveHistory* history) {
    return history ? (PassiveHistoryStatus)history->status : PassiveHistoryStatusInvalid;
}

size_t passive_history_count(const PassiveHistory* history, PassiveHistoryProtocol filter) {
    if(!history || history->status == PassiveHistoryStatusInvalid) return 0;
    size_t count = 0;
    for(size_t i = 0; i < history->count; i++) {
        if(history_filter_matches(filter, (PassiveHistoryProtocol)history->entries[i].protocol)) {
            count++;
        }
    }
    return count;
}

bool passive_history_get_key(
    const PassiveHistory* history,
    PassiveHistoryProtocol filter,
    size_t position,
    uint8_t mac[6],
    PassiveHistoryProtocol* protocol) {
    if(!history || !mac || !protocol || history->status == PassiveHistoryStatusInvalid)
        return false;
    size_t current = 0;
    for(size_t i = 0; i < history->count; i++) {
        PassiveHistoryProtocol candidate = (PassiveHistoryProtocol)history->entries[i].protocol;
        if(!history_filter_matches(filter, candidate)) continue;
        if(current++ == position) {
            memcpy(mac, history->entries[i].mac, 6);
            *protocol = candidate;
            return true;
        }
    }
    return false;
}

neighbor_t* passive_history_decode(
    PassiveHistory* history,
    Storage* storage,
    const uint8_t mac[6],
    PassiveHistoryProtocol protocol) {
    if(!history || !storage || !mac || history->status != PassiveHistoryStatusReady) return NULL;
    const PassiveHistoryIndexEntry* entry = NULL;
    for(size_t i = 0; i < history->count; i++) {
        if(history->entries[i].protocol == protocol &&
           memcmp(history->entries[i].mac, mac, 6) == 0) {
            entry = &history->entries[i];
            break;
        }
    }
    if(!entry) return NULL;

    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, PASSIVE_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
              history_record_read(file, entry, &history->scratch);
    storage_file_close(file);
    storage_file_free(file);
    return ok ? &history->scratch : NULL;
}

static bool history_writer_begin(HistoryWriter* writer, Storage* storage) {
    memset(writer, 0, sizeof(*writer));
    writer->file = storage_file_alloc(storage);
    writer->body_crc = 0xFFFFFFFFUL;
    uint8_t placeholder[HISTORY_HEADER_SIZE] = {0};
    writer->ok = storage_file_open(
                     writer->file, PASSIVE_HISTORY_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                 storage_file_write(writer->file, placeholder, sizeof(placeholder)) ==
                     sizeof(placeholder);
    return writer->ok;
}

static bool history_writer_add(
    HistoryWriter* writer,
    PassiveHistoryProtocol protocol,
    const neighbor_t* neighbor) {
    if(!writer->ok || writer->count >= PASSIVE_HISTORY_MAX_RECORDS || !neighbor ||
       !history_protocol_valid(protocol)) {
        return false;
    }
    uint8_t payload[HISTORY_PAYLOAD_MAX];
    uint16_t payload_length;
    uint16_t flags;
    if(!history_encode_payload(protocol, neighbor, payload, &payload_length, &flags)) return false;

    uint8_t header[HISTORY_RECORD_HEADER_SIZE] = {0};
    header[0] = HISTORY_RECORD_VERSION;
    header[1] = (uint8_t)protocol;
    history_write_u16(header + 2, flags);
    history_write_u16(header + 4, payload_length);
    memcpy(header + 6, neighbor->mac, 6);
    history_write_u32(header + 12, history_crc32(payload, payload_length));

    writer->ok = storage_file_write(writer->file, header, sizeof(header)) == sizeof(header) &&
                 storage_file_write(writer->file, payload, payload_length) == payload_length;
    if(writer->ok) {
        writer->body_crc = history_crc32_update(writer->body_crc, header, sizeof(header));
        writer->body_crc = history_crc32_update(writer->body_crc, payload, payload_length);
        writer->body_size += sizeof(header) + payload_length;
        writer->count++;
    }
    return writer->ok;
}

static bool history_writer_finish(HistoryWriter* writer, Storage* storage) {
    uint8_t header[HISTORY_HEADER_SIZE] = {0};
    memcpy(header, history_magic, sizeof(history_magic));
    header[4] = HISTORY_FORMAT_VERSION;
    header[5] = HISTORY_HEADER_SIZE;
    header[6] = writer->count;
    history_write_u32(header + 8, writer->body_size);
    history_write_u32(header + 12, writer->body_crc ^ 0xFFFFFFFFUL);
    history_write_u32(header + 16, history_crc32(header, 16));

    bool ok = writer->ok && storage_file_seek(writer->file, 0, true) &&
              storage_file_write(writer->file, header, sizeof(header)) == sizeof(header) &&
              storage_file_sync(writer->file);
    storage_file_close(writer->file);
    storage_file_free(writer->file);
    writer->file = NULL;
    if(ok)
        ok = storage_common_rename(storage, PASSIVE_HISTORY_TEMP_PATH, PASSIVE_HISTORY_PATH) ==
             FSE_OK;
    if(!ok) storage_common_remove(storage, PASSIVE_HISTORY_TEMP_PATH);
    return ok;
}

static void history_writer_abort(HistoryWriter* writer, Storage* storage) {
    if(writer->file) {
        storage_file_close(writer->file);
        storage_file_free(writer->file);
        writer->file = NULL;
    }
    storage_common_remove(storage, PASSIVE_HISTORY_TEMP_PATH);
}

static neighbor_t* history_find_live(
    const uint8_t mac[6],
    PassiveHistoryProtocol protocol,
    PassiveHistoryProtocol filter) {
    if(!history_filter_matches(filter, protocol)) return NULL;
    return neighbor_db_find_by_source(mac, history_source_for_protocol(protocol));
}

bool passive_history_merge_live(Storage* storage, PassiveHistoryProtocol filter) {
    if(!storage || (filter != PassiveHistoryProtocolAll && !history_protocol_valid(filter)))
        return false;
    PassiveHistory history;
    memset(&history, 0, sizeof(history));
    history_load(&history, storage);

    HistoryWriter writer;
    if(!history_writer_begin(&writer, storage)) {
        history_writer_abort(&writer, storage);
        return false;
    }

    File* source = NULL;
    bool have_existing = history.status == PassiveHistoryStatusReady;
    if(have_existing) {
        source = storage_file_alloc(storage);
        have_existing =
            storage_file_open(source, PASSIVE_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    }

    bool ok = !history.count || have_existing;
    for(size_t i = 0; ok && i < history.count; i++) {
        PassiveHistoryProtocol protocol = (PassiveHistoryProtocol)history.entries[i].protocol;
        neighbor_t* live = history_find_live(history.entries[i].mac, protocol, filter);
        if(!live) {
            ok = history_record_read(source, &history.entries[i], &history.scratch);
            live = &history.scratch;
        }
        if(ok) ok = history_writer_add(&writer, protocol, live);
    }

    size_t live_count = neighbor_db_count();
    for(size_t i = 0; ok && i < live_count; i++) {
        neighbor_t* live = neighbor_db_get_by_position(i);
        if(!live) continue;
        for(PassiveHistoryProtocol protocol = PassiveHistoryProtocolLldp;
            protocol <= PassiveHistoryProtocolEapol;
            protocol++) {
            uint8_t source_bit = history_source_for_protocol(protocol);
            if(!(live->discovery_sources & source_bit) ||
               !history_filter_matches(filter, protocol) ||
               history_key_exists(&history, live->mac, protocol)) {
                continue;
            }
            if(writer.count < PASSIVE_HISTORY_MAX_RECORDS) {
                ok = history_writer_add(&writer, protocol, live);
            }
        }
    }

    if(source) {
        storage_file_close(source);
        storage_file_free(source);
    }
    ok = ok ? history_writer_finish(&writer, storage) : false;
    if(!ok && writer.file) history_writer_abort(&writer, storage);
    return ok;
}

bool passive_history_clear_storage(Storage* storage, PassiveHistoryProtocol filter) {
    if(!storage) return false;
    PassiveHistory history;
    memset(&history, 0, sizeof(history));
    history_load(&history, storage);
    return passive_history_clear(&history, storage, filter);
}

bool passive_history_clear(
    PassiveHistory* history,
    Storage* storage,
    PassiveHistoryProtocol filter) {
    if(!history || !storage ||
       (filter != PassiveHistoryProtocolAll && !history_protocol_valid(filter))) {
        return false;
    }
    if(history->status == PassiveHistoryStatusInvalid && filter != PassiveHistoryProtocolAll)
        return false;

    HistoryWriter writer;
    if(!history_writer_begin(&writer, storage)) {
        history_writer_abort(&writer, storage);
        return false;
    }

    File* source = NULL;
    bool have_existing = history->status == PassiveHistoryStatusReady;
    if(have_existing) {
        source = storage_file_alloc(storage);
        have_existing =
            storage_file_open(source, PASSIVE_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    }

    bool ok = !history->count || have_existing;
    for(size_t i = 0; ok && i < history->count; i++) {
        PassiveHistoryProtocol protocol = (PassiveHistoryProtocol)history->entries[i].protocol;
        if(history_filter_matches(filter, protocol)) continue;
        ok = history_record_read(source, &history->entries[i], &history->scratch) &&
             history_writer_add(&writer, protocol, &history->scratch);
    }
    if(source) {
        storage_file_close(source);
        storage_file_free(source);
    }
    ok = ok ? history_writer_finish(&writer, storage) : false;
    if(!ok && writer.file) history_writer_abort(&writer, storage);
    if(ok) history_load(history, storage);
    return ok;
}
