#pragma once

#include "neighbor_db.h"

#include <storage/storage.h>

#define PASSIVE_HISTORY_MAX_RECORDS 32U
#define PASSIVE_HISTORY_PATH        EXT_PATH("apps_data/ethernet/passive_history.bin")

typedef enum {
    PassiveHistoryProtocolAll = 0,
    PassiveHistoryProtocolLldp = 1,
    PassiveHistoryProtocolCdp = 2,
    PassiveHistoryProtocolEapol = 3,
} PassiveHistoryProtocol;

typedef enum {
    PassiveHistoryStatusReady,
    PassiveHistoryStatusMissing,
    PassiveHistoryStatusInvalid,
} PassiveHistoryStatus;

typedef struct PassiveHistory PassiveHistory;

PassiveHistory* passive_history_alloc(Storage* storage);
void passive_history_free(PassiveHistory* history);

PassiveHistoryStatus passive_history_get_status(const PassiveHistory* history);
size_t passive_history_count(const PassiveHistory* history, PassiveHistoryProtocol filter);

bool passive_history_get_key(
    const PassiveHistory* history,
    PassiveHistoryProtocol filter,
    size_t position,
    uint8_t mac[6],
    PassiveHistoryProtocol* protocol);

neighbor_t* passive_history_decode(
    PassiveHistory* history,
    Storage* storage,
    const uint8_t mac[6],
    PassiveHistoryProtocol protocol);

bool passive_history_merge_live(Storage* storage, PassiveHistoryProtocol filter);
bool passive_history_clear_storage(Storage* storage, PassiveHistoryProtocol filter);
bool passive_history_clear(
    PassiveHistory* history,
    Storage* storage,
    PassiveHistoryProtocol filter);
