#ifndef GHOST_HEARTBEAT_H
#define GHOST_HEARTBEAT_H

// =====================================================
// Ghost Hive v1.7.1
// Heartbeat — P2 FINAL
// Spec-Basis: §12.5, §20, §23
// =====================================================

#include "ghost_core.h"
#include "ghost_data.h"
#include "registry.h"

const uint8_t HEARTBEAT_INTERVAL_SEC = 30;
const uint8_t HEARTBEAT_MISS_LIMIT = 3;

class GhostHeartbeat {
public:
    GhostHeartbeat();

    void init();
    void send(const char* deviceId, uint32_t now);
    void update(const char* deviceId, uint32_t timestamp);
    void tick(uint32_t now, Registry* registry);
    void checkAll(uint32_t now, Registry& registry);

    bool isAlive(const char* deviceId, uint32_t now) const;
    uint8_t getMissCount(const char* deviceId) const;
    HeartbeatInfo getInfo(const char* deviceId) const;
    uint8_t trackedCount() const;
    const char* idAt(uint8_t index) const;

private:
    struct Entry {
        char device_id[32];
        uint32_t last_beat;
        uint8_t miss_count;
    };

    Entry entries_[32];
    uint8_t count_;

    int16_t findIndex(const char* deviceId) const;
    static void copyId(char* dst, const char* src);
};

#endif // GHOST_HEARTBEAT_H
