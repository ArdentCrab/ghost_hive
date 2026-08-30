#ifndef REPLAY_GUARD_H
#define REPLAY_GUARD_H

// =====================================================
// Ghost Hive v1.7.1
// Replay Guard
// Spec-Basis: §15, §33, §44
// =====================================================

#include "ghost_core.h"
#include "ghost_crypto.h"
#include "registry.h"

const uint8_t REPLAY_WINDOW_PER_MINE = 64;
const uint8_t MAX_TRACKED_MINES = 32;

class ReplayGuard {
public:
    ReplayGuard();

    void init();

    bool check(const MinePayload& payload, uint32_t now);
    bool check(const MinePayload& payload, uint32_t now,
               const uint8_t* totpSeed, uint8_t seedLen);
    bool acceptFromTransport(const MinePayload& payload, uint32_t now,
                             Registry* registry);

    void blockMine(const char* mineId);
    void blockMine(const char* mineId, Registry& registry);
    bool isBlocked(const char* mineId) const;
    bool setTotpSeed(const char* mineId, const uint8_t* seed, uint8_t len);

    uint8_t trackedCount() const;
    const char* mineIdAt(uint8_t index) const;
    uint32_t lastCounterAt(uint8_t index) const;
    uint32_t lastTimestampAt(uint8_t index) const;
    bool blockedAt(uint8_t index) const;

private:
    struct MineWindow {
        char mine_id[32];
        uint32_t counters[REPLAY_WINDOW_PER_MINE];
        uint8_t index;
        uint32_t last_timestamp;
        uint32_t last_counter;
        uint8_t totp_seed[TOTP_SEED_LEN];
        bool has_seed;
        bool blocked;
    };

    MineWindow windows_[MAX_TRACKED_MINES];
    uint8_t windowCount_;

    int16_t findWindow(const char* mineId) const;
    static bool sameId(const char* a, const char* b);
    static bool hashOk(const MinePayload& payload);
};

#endif // REPLAY_GUARD_H
